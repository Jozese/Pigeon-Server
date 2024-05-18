#include "PigeonServer.h"

/**
 * @brief Manual Constructor for PigeonServer class.
 * @param certPath The path to the certificate.
 * @param keyPath The path to the key.
 * @param serverName The name of the server.
 * @param port The port number.
 * @param logger Pointer to Logger object.
 */
PigeonServer::PigeonServer(const std::string &certPath, const std::string &keyPath, const std::string &serverName, unsigned short port, Logger *logger) : TcpServer(certPath, keyPath, port), serverName(serverName), logger(logger)
{
    clients = new std::unordered_map<int, Client *>();

    logger->log(DEBUG, "Setting up TCP server");

    if (TcpServer::Setup() != 0)
    {
        logger->log(ERROR, "Error while setting up tcp server");
        exit(EXIT_FAILURE);
    }
};

/*
* @brief Constructor for PigeonServer class from Pigeon Data directly.
*/
PigeonServer::PigeonServer(PigeonData &data, Logger *logger) : TcpServer(data.GetData()["cert"].asString(), data.GetData()["key"].asString(), (unsigned short)data.GetData()["port"].asInt()),
                                                               serverName(data.GetData()["serverName"].asString()),
                                                               logger(logger),
                                                               m_data(&data)
{
    clients = new std::unordered_map<int, Client *>();
    

    logger->log(DEBUG, "Setting up TCP server");

    if (TcpServer::Setup() != 0)
    {
        logger->log(ERROR, "Error while setting up tcp server");
        exit(EXIT_FAILURE);
    }

    /*
    * Watcher thread that prevents zombie tcp connections. If a tcp connection has not sent a CLIENT_HELLO message in 10 seconds
    * it will be instantly disconnected. DisconnectClient will close the socket and wake up the blocking main thread of the client,
    * so the client is freed properly after disconnecting. All clients are checked every one second.
    */

    std::thread([this]{
        
        while (true)
        {
            std::time_t currentCheckTime = std::time(0);

            std::unique_lock<std::mutex> lock(this->m_clientsMtx);

            if(!clients->empty()){
                for(auto& client: *clients){
                    if(!client.second->hasLogged){
                        if(currentCheckTime - client.second->logTimestamp >= 10){
                            this->logger->log(ERROR,"Zombie connection detected. Killing FD: " + std::to_string(client.first));
                            DisconnectClient(client.first, client.second->clientSsl);
                        }
                    }  
                }
            }

            lock.unlock();

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();
}

/**
 * @brief Runs the Pigeon server.
 */
void PigeonServer::Run()
{
    signal(SIGPIPE, SIG_IGN);

    while (1)
    {
       /*
        * Storing client ip addr just in case we need it might be useful to check if the same ip is connecting twice or more
        * 
       */
        sockaddr_in clientAddr;
        unsigned int len = sizeof(sockaddr_in);

        int client = accept(sSocket, (struct sockaddr *)&addr, &len);

        char clientIp[INET_ADDRSTRLEN];

        struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&addr;

        struct in_addr ipAddr = pV4Addr->sin_addr;

        inet_ntop(AF_INET, &ipAddr, clientIp, INET_ADDRSTRLEN);

        if (client < 0)
        {

            logger->log(ERROR, "Failed TCP Handshake");

            continue;
        }

        /*if(CheckIp(std::string(clientIp))){
            close(client);
            continue;
        }*/

        logger->log(DEBUG, "OK TCP Handshake " + std::string(clientIp));

        ssl = SSL_new(sslCtx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) == 0)
        {

            logger->log(ERROR, "Failed TLS Handshake " + std::string(clientIp));

            SSL_shutdown(ssl);
            SSL_free(ssl);
            continue;
        }
        else
        {

            logger->log(DEBUG, "OK TLS Handshake " + std::string(clientIp));

            Client *newClient = new Client();
            newClient->clientSsl = ssl;
            newClient->ipv4 = std::string(clientIp);
            newClient->logTimestamp = std::time(0);

            //manual lock
            std::unique_lock<std::mutex> lock(this->m_clientsMtx);
            auto clientIter = clients->insert({client, newClient});
            lock.unlock();

            /*
             *  Each client is managed within its own thread. Probably not the best approach efficiently wise, but its pretty simple to implement.
             *  Others approach with select() and non blocking network I/O calls might perform better but are not as easy to implement.
             *  Thread is terminated when the read packet was empty (meaning error while recving or client was closed)
             *  or the recv packet was not processed correctly.
             *
             *
             */

            std::thread([clientIter, this]
            {         
                    logger->log(INFO," [INFO] NEW THREAD FOR CLIENT FD: " + std::to_string(clientIter.first->first));

                    while (1)
                    {   
                    
                        // thread will block here untill a disconnection (empty packet) or an actual packet was read.
                        std::vector<unsigned char> clientPacket = ReadPacket(clientIter.first->second->clientSsl);

                        if(m_data->GetData()["LogPkt"].asBool())
                            logger->log(DEBUG, "NEW PKT: " + String::HexToString(clientPacket));


                        if(clientPacket.empty())
                        {
                                std::lock_guard<std::mutex> lock(this->m_clientsMtx);

                                    if(clients->find(clientIter.first->first) != clients->end()){
                                            
                                        logger->log(DEBUG,"ENDED THREAD FOR FD: " + std::to_string(clientIter.first->first));

                                        DisconnectClient(clientIter.first->first,clientIter.first->second->clientSsl);
                                        FreeClient(clientIter.first->first);
                                        
                                        this->NotifyNewPresence();
  
                                    }

                                logger->log(INFO,"AMOUNT OF CLIENTS: " + std::to_string(clients->size()));
                            break;
                        }
                        

                        auto clientPigeonPacket = DeserializePacket(clientPacket);
                   
                        PigeonPacket toSend = ProcessPacket(clientPigeonPacket,clientIter.first->first);

                        if(toSend.HEADER.OPCODE == ACK_MEDIA_DOWNLOAD){
                                                         
                            logger->log(INFO,"SENDING FILE TO " + clientIter.first->second->username);
 
                             auto bufToSend = SerializePacket(toSend);
                             SendAll(bufToSend,clientIter.first->second->clientSsl);
                             continue;
                        }

                        //if to send is server_hello, that means a successfull client_hello was read, so we notify the right client and then we broadcast the new presence list to evry client
                        if(toSend.HEADER.OPCODE == SERVER_HELLO){

                            auto bufToSend = SerializePacket(toSend);
                            SendAll(bufToSend,clientIter.first->second->clientSsl);
                            
                            this->NotifyNewPresence();

                            continue;
                        }

                        if(toSend.HEADER.OPCODE == PRESENCE_UPDATE){
                            this->NotifyNewPresence();
                            continue;
                        }

                        //bad packet, close connection and notify all clients
                        if((toSend.HEADER.OPCODE & 0xF0) == 0xE0){
                            if(clients->find(clientIter.first->first) != clients->end()){                             
                                
                                logger->log(DEBUG,"ENDED THREAD FOR FD: " + std::to_string(clientIter.first->first));                                

                                auto buf = SerializePacket(toSend);
                                SendAll(buf,clientIter.first->second->clientSsl);

                                DisconnectClient(clientIter.first->first,clientIter.first->second->clientSsl);
                                FreeClient(clientIter.first->first);

                                this->NotifyNewPresence();

                                logger->log(INFO,"AMOUNT OF CLIENTS: " + std::to_string(clients->size()));
          
                            }
                            break;
                        }

                        //If reached here, it means that whatever packet is there to send back, it must be broadcasted to all clients, not multicasted or sent directly to one client
                        BroadcastPacket(toSend);

                    }
                    return; })
                .detach(); // could also probaby just store the threads somewhere instead of detach
        }
    }
}

/**
 * @brief Serializes a PigeonPacket object. (From PigeonPacket to uchar vector)
 * @param packet The PigeonPacket to serialize.
 * @return The serialized packet as a vector of unsigned characters.
 */
std::vector<unsigned char> PigeonServer::SerializePacket(const PigeonPacket &packet)
{
    std::vector<unsigned char> serializedPacket;

    int headerLength = packet.HEADER.HEADER_LENGTH;
    unsigned char *headerLengthBytes = reinterpret_cast<unsigned char *>(&headerLength);
    serializedPacket.insert(serializedPacket.end(), headerLengthBytes, headerLengthBytes + sizeof(int));

    std::time_t timestamp = packet.HEADER.TIME_STAMP;
    unsigned char *timestampBytes = reinterpret_cast<unsigned char *>(&timestamp);
    serializedPacket.insert(serializedPacket.end(), timestampBytes, timestampBytes + sizeof(std::time_t));

    serializedPacket.insert(serializedPacket.end(), packet.HEADER.username.begin(), packet.HEADER.username.end());
    serializedPacket.push_back('\0');

    serializedPacket.push_back(static_cast<unsigned char>(packet.HEADER.OPCODE));

    int contentLength = packet.HEADER.CONTENT_LENGTH;
    unsigned char *contentLengthBytes = reinterpret_cast<unsigned char *>(&contentLength);
    serializedPacket.insert(serializedPacket.end(), contentLengthBytes, contentLengthBytes + sizeof(int));

    serializedPacket.insert(serializedPacket.end(), packet.PAYLOAD.begin(), packet.PAYLOAD.end());

    return serializedPacket;
}

/**
 * @brief Deserializes a packet. From uchar vector to PigeonPacket
 * @param packet The packet to deserialize.
 * @return The deserialized PigeonPacket object.
 */
PigeonPacket PigeonServer::DeserializePacket(std::vector<unsigned char> &packet)
{
    if (packet.empty())
        return PigeonPacket();

    PigeonPacket packetRet;

    int offset = 0;

    std::memcpy(&packetRet.HEADER.HEADER_LENGTH, &packet[offset], sizeof(int));
    offset += sizeof(int);

    std::memcpy(&packetRet.HEADER.TIME_STAMP, &packet[offset], sizeof(std::time_t));
    offset += sizeof(std::time_t);

    while (packet[offset] != '\0')
    {
        packetRet.HEADER.username.push_back(packet[offset]);
        offset++;
    }

    offset++;
    // opc
    packetRet.HEADER.OPCODE = static_cast<PIGEON_OPCODE>(packet[offset]);
    offset++;

    std::memcpy(&packetRet.HEADER.CONTENT_LENGTH, &packet[offset], sizeof(int));
    offset += sizeof(int);

    packetRet.PAYLOAD.assign(packet.begin() + offset, packet.end());
    return packetRet;
}
/**
 * @brief Builds a packet from scratch
 * @param opcode The packet to deserialize.
 * @param payload Packet payload.
 * @param username Username to be in the packet.
 * @return Packet to be sent
 */

PigeonPacket PigeonServer::BuildPacket(PIGEON_OPCODE opcode, const std::string &username, const std::vector<unsigned char> &payload)
{

    PigeonPacket pkt;
    pkt.HEADER.OPCODE = opcode;
    pkt.PAYLOAD = payload;
    pkt.HEADER.CONTENT_LENGTH = pkt.PAYLOAD.size();
    pkt.HEADER.TIME_STAMP = std::time(0);
    pkt.HEADER.username = username;
    pkt.HEADER.HEADER_LENGTH = sizeof(std::time_t) +            
                               pkt.HEADER.username.length() + 1 +
                               sizeof(unsigned char) +
                               sizeof(int);
    return pkt;
}

/**
 * @brief Builds a new packet using BuildPacket based on the recived packet.
 * @param recv Recived PigeonPacket.
 */
// TODO: ADD LOGS FOR EACH EDGE CASE
PigeonPacket PigeonServer::ProcessPacket(PigeonPacket &recv, int clientFD)
{
    PigeonPacket newPacket;
    Json::Reader reader;
    Json::Value value;

    // If client completed handshake, username is stored in Client, if a client tries to send a packet before making a handshake (aka username doesnt exist), bad client = close con,
    //  that way we ensure a client has completed handshake properly

    switch (recv.HEADER.OPCODE)
    {
        /*
        *  Client sends a CLIENT_HELLO packet
        *  Check if payload/username empty
        *  Check if bad json in payload
        *  Check if client already exist
        *  Check if username exceeds MAX_USERNAME
        */
    case CLIENT_HELLO:
        if (!recv.PAYLOAD.empty() && !recv.HEADER.username.empty())
        {
            newPacket = BuildPacket(SERVER_HELLO, recv.HEADER.username, String::StringToBytes(R"({"ServerName":")" + m_data->GetData()["servername"].asString() + R"(","MOTD":")" + m_data->GetData()["MOTD"].asString() + +R"(","sizelimit":)" + std::to_string(m_data->GetData()["sizelimit"].asInt()) + R"(})"));
            
            auto it = this->clients->find(clientFD);

            if (!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value))
            {
                newPacket = BuildPacket(JSON_NOT_VALID, "", {});
                break;
            }

            logger->log(INFO, "CLIENT HELLO FROM: " + recv.HEADER.username + " STATUS: " + value["status"].asString());

            if (it != clients->end())
            {
                it->second->logTimestamp = std::time(0);

                // Does username already exist?
                bool exists = false;
                for (auto &p : *this->clients)
                {
                    if (p.second->username == recv.HEADER.username)
                        exists = true;
                }

                // On connection, status will be Online by default.
                if (!exists)
                {
                    it->second->username = recv.HEADER.username;

                    if (value["status"].asString() == "ONLINE")
                    {
                        it->second->status = ONLINE;
                    }
                    else if (value["status"].asString() == "IDLE")
                    {
                        it->second->status = IDLE;
                    }
                    else if (value["status"].asString() == "DND")
                    {
                        it->second->status = DND;
                    }

                    it->second->hasLogged = true;
                }
                else
                {
                    newPacket = BuildPacket(USER_COLLISION, recv.HEADER.username, {});

                    logger->log(ERROR, "USER COLLISION: " + recv.HEADER.username + " IS ALREADY USED");
                }

                // Username max length check
                if (it->second->username.length() > MAX_USERNAME)
                {
                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username, {});

                    logger->log(ERROR, "USERNAME LENGTH EXCEEDED: " + recv.HEADER.username);
                }
            }
        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        }
        break;

    /*
    *  Client Sends a TEXT_MESSAGE packet
    *   Usual payload/username check
    *   Check if client exist
    *   Check txt message length
    *   
    */
    case TEXT_MESSAGE:
        if (!recv.PAYLOAD.empty() && !recv.HEADER.username.empty())
        {

            // We can saftely assume that after a successfful CLIENT_HELLO, the clients fd has a valid username and is connected
            // So we need to check if the FD who sent the packet has the same username thats contained in the packet itself.

            logger->log(INFO, "TEXT MESSAGE BY: " + recv.HEADER.username + " " + std::string(recv.PAYLOAD.data(), recv.PAYLOAD.data() + recv.HEADER.CONTENT_LENGTH));

            auto it = clients->find(clientFD);

            if (it->second->username == recv.HEADER.username)
            {
                // If both usernames match, we just need to verify the packets payload
                if (recv.PAYLOAD.size() > 512)
                {
                    logger->log(WARNING, "TEXT MESSAGE TOO BIG: " + recv.HEADER.username);

                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username, {});
                    break;
                }
                newPacket = BuildPacket(TEXT_MESSAGE, recv.HEADER.username, recv.PAYLOAD);
            }
            else
            {
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username, {});
            }
        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        }
        break;

    /*
    *  Client sends a file
    *  Username/payload check
    *  Check if payload exceeds limit
    *  Check if user is ratelimited
    *  Check valid json with all expected fields
    *  Writing to disk
    */
    case MEDIA_FILE:
        if (!recv.PAYLOAD.empty() && !recv.HEADER.username.empty())
        {

            logger->log(INFO, "NEW MEDIA FILE BY: " + recv.HEADER.username + " SIZE: " + std::to_string((recv.PAYLOAD.size() / (1000))) + " KB");

            auto it = clients->find(clientFD);
            if (it->second->username == recv.HEADER.username)
            {

                if (recv.PAYLOAD.size() > m_data->GetData()["sizelimit"].asInt() * 1000 * 1000)
                {

                    logger->log(ERROR, "MEDIA FILE TOO BIG: " + recv.HEADER.username);
                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username, {});
                    break;
                }

                if (std::time(0) - it->second->logTimestamp < m_data->GetData()["ratelimit"].asInt())
                {
                    newPacket = BuildPacket(RATE_LIMITED, recv.HEADER.username, {});

                    logger->log(ERROR, "RATE LIMITED: " + recv.HEADER.username);
                    break;
                }

                it->second->logTimestamp = std::time(0);
                if (!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value))
                {

                    logger->log(ERROR, "MALFORMED MEDIA PACKET: " + recv.HEADER.username);
                    newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username, {});
                    break;
                }

                std::string fileContent = "";
                std::string fileExt = "";
                std::string fileName = "";

                fileContent = value["content"].asString();
                fileExt = value["ext"].asString();
                fileName = value["filename"].asString();

                // std::cout << fileExt << fileName << std::endl;

                if (fileContent == "" || fileName == "")
                {

                    logger->log(ERROR, "MALFORMED MEDIA PACKET: " + recv.HEADER.username);
                    newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username, {});
                    break;
                }

                auto contentBuffer = String::StringToBytes(B64::base64_decode(fileContent));

                //Mutual exlcusion using RAII locks
                {
                    std::lock_guard<std::mutex> lock(this->m_clientsMtx);
                    bool err = File::BufferToDisk(recv.PAYLOAD, ("Files/" + std::to_string(std::time(0)) + "_" + fileName + ".json"));
                }
                newPacket = BuildPacket(MEDIA_FILE, recv.HEADER.username, String::StringToBytes(R"({"filename":")" + fileName + R"(", "ext":")" + fileExt + R"("})"));
            }
            else
            {
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username, {});
            }
            break;
        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        }
        break;
    /*
    *   Media download request
    *   Username/payload check
    *   Check if bad json and has all the expected fields
    *   If to send buffer was empty it means it couldnt read the file aka it doesnt exist
    */
    case MEDIA_DOWNLOAD:

        logger->log(INFO, "NEW MEDIA DOWNLOAD REQUEST BY: " + recv.HEADER.username);

        if (!recv.PAYLOAD.empty() && !recv.HEADER.username.empty())
        {

            auto it = clients->find(clientFD);

            if (it->second->username != recv.HEADER.username)
            {
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username, {});
                break;
            }


            if (std::time(0) - it->second->logTimestamp < 15)
            {
                newPacket = BuildPacket(RATE_LIMITED, recv.HEADER.username, {});

                logger->log(ERROR, "RATE LIMITED: " + recv.HEADER.username);

                break;
            }

            it->second->logTimestamp = std::time(0);

            if (!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value))
            {

                newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username, {});
                break;
            }

            std::string filename = "";
            filename = value["filename"].asString();

            if (filename == "")
            {

                logger->log(ERROR, "MALFORMED DOWNLOAD REQUEST: " + recv.HEADER.username);

                newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username, {});
                break;
            }

            //verify filename later in case of path traversal but it wont really happen
            std::vector<unsigned char> buffer;
            {
                std::lock_guard<std::mutex> lock(this->m_clientsMtx);
                buffer = File::DiskToBuffer("Files/" + filename + ".json");
            }

            if (buffer.empty())
            {
                logger->log(WARNING, "FILE NOT FOUND: " + recv.HEADER.username);
                newPacket = BuildPacket(FILE_NOT_FOUND, recv.HEADER.username, {});
                break;
            }

            newPacket = BuildPacket(ACK_MEDIA_DOWNLOAD, recv.HEADER.username, buffer);
        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        }

        break;

    /*
    * All clients presence request
    * No need to check if payload cuz this packet must not have a payload
    * 
    *
    */
    case PRESENCE_REQUEST:

        if (!recv.HEADER.username.empty())
        {

            logger->log(INFO, "NEW PRESENCE REQUEST BY: " + recv.HEADER.username);

            auto it = clients->find(clientFD);

            if (it->second->username != recv.HEADER.username)
            {
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username, {});
                break;
            }

            newPacket = BuildPacket(PRESENCE_UPDATE,"",{});

        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        }

        break;
    /*
    * Presence update request
    * Verify payload/username
    * Bad json check
    * Only if the status field exist then presence will update
    */
    case PRESENCE_UPDATE:
        if (!recv.PAYLOAD.empty() && !recv.HEADER.username.empty())
        {

            logger->log(INFO, "NEW PRESENCE UPDATE REQUEST BY: " + recv.HEADER.username);

            auto it = this->clients->find(clientFD);

            if (it->second->username != recv.HEADER.username)
            {
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username, {});
                break;
            }

            if (!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value))
            {
                newPacket = BuildPacket(JSON_NOT_VALID, "", {});
                break;
            }

            if (value["status"].asString() == "ONLINE")
            {
                it->second->status = ONLINE;
            }
            else if (value["status"].asString() == "IDLE")
            {
                it->second->status = IDLE;
            }
            else if (value["status"].asString() == "DND")
            {
                it->second->status = DND;
            }

            newPacket = BuildPacket(PRESENCE_UPDATE,"",{});
        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        }
        break;
    /*
    * If opcode doesnt match any opcode then it must be bad packet/other protocol
    *
     */
    default:
        newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username, {});
        break;
    }
    return newPacket;
}

/**
 * @brief Read incoming packet from a valid SSL/TLS connection.
 * @param ssl1 SSL/TLS connection from an active client.
 * 
 * If a emtpy vector is returned, the connection with the client will be closed
 */
// Packet is 4 bytes + 8 bytes + 1 byte + username (max 20 chars == 20 bytes) + 1 byte null char + 4 bytes payload size + payload ( max 256 MB)
std::vector<unsigned char> PigeonServer::ReadPacket(SSL *ssl1)
{
    std::vector<unsigned char> packetBuffer(MAX_HEADER);

    int total = TcpServer::Recv(packetBuffer, 4, 0, ssl1);

    // This could also mean error when receving, but mainly used in case of a disconnect. In case of error/wrong packet, empty vector is returned and the connection will be closed
    if (total < 4)
    {
        return {};
    }

    // HUGE bug here when trying to recv huge files
    int headerLength = 0;

    /*headerLength += packetBuffer[0] << (8 * 0);
    headerLength += packetBuffer[1] << (8 * 1);
    headerLength += packetBuffer[2] << (8 * 2);
    headerLength += packetBuffer[3] << (8 * 3);
    */
    for (int i = 0; i < 4; i++)
    {
        headerLength += packetBuffer[i] << (8 * i);
    }

    if (headerLength > MAX_HEADER)
    {
        logger->log(ERROR, "PACKET HEADER TOO BIG");

        return {};
    }

    total = TcpServer::Recv(packetBuffer, headerLength + 4, total, ssl1);

    packetBuffer.resize(total);

    long long payloadLength = 0;
    for (int i = packetBuffer.size() - 1; i >= packetBuffer.size() - 4; --i)
    {
        payloadLength = (payloadLength << 8) | packetBuffer[i];
    }

    if(payloadLength >= 256*1000*1000){
        logger->log(ERROR, "PAYLOAD TOO BIG");
        return {};
    }

    if (payloadLength == 0)
    {
        return packetBuffer;
    }

    packetBuffer.resize(packetBuffer.size() + payloadLength);

    //Logger is passed here so we can log the recv bytes when downloading the file from the client
    total = TcpServer::Recv(packetBuffer, headerLength + 4 + payloadLength, total, ssl1, this->logger);

    return packetBuffer;
}

/**
 * @brief Sends a PigeonPacket to all connected clients.
 * @param packet Packet to sent.
 */
void *PigeonServer::BroadcastPacket(const PigeonPacket &packet)
{
    auto packetToSend = SerializePacket(packet);
    int sent = -1;
    std::string clientsStr = "";
    if (!packetToSend.empty())
    {
        for (auto &c : *clients)
        {
            clientsStr += std::to_string(c.first) + " ";
            sent += SendAll(packetToSend, c.second->clientSsl);
        }
        this->logger->log(DEBUG, "BROADCASTED " + std::to_string(sent) + " BYTES");
    }
    return nullptr;
}

/**
 * @brief Sends a PigeonPacket to all connected clients.
 * @param packet Packet to sent.
 */
void PigeonServer::NotifyNewPresence()
{

    std::string toSend = "{";
    for (auto &c : *clients)
    {
        if (!c.second->username.empty())
        {
            std::string pair = ('"' + c.second->username + '"' + ":" + '"' + std::to_string(c.second->status) + '"' + ',');
            toSend += pair;
        }
    }

    if (toSend.length() >= 2)
    {
        // remove last ,
        toSend.pop_back();
        toSend += '}';
    }

    BroadcastPacket(BuildPacket(PRESENCE_UPDATE, this->serverName, String::StringToBytes(toSend)));
}
