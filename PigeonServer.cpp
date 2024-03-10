#include "PigeonServer.h"



/**
     * @brief Constructor for PigeonServer class.
     * @param certPath The path to the certificate.
     * @param keyPath The path to the key.
     * @param serverName The name of the server.
     * @param port The port number.
     * @param log Pointer to ImGuiLog object.
     */
PigeonServer::PigeonServer(const std::string& certPath, const std::string& keyPath, const std::string& serverName, unsigned short port,ImGuiLog* log):
    TcpServer(certPath,keyPath,port), serverName(serverName),log(log)
{
    clients = new std::unordered_map<int,Client*>();
    this->bytesRecv = new double(0);
    this->bytesSent = new double(0);


    log->AddLog((GetDate() + " [INFO] Setting up TCP server\n").c_str());

    if(TcpServer::Setup() != 0){
        throw std::string("[ERROR] PigeonServer::PigeonServer Error while setting up tcp server");
    }

};


/**
     * @brief Runs the Pigeon server.
     * @param shouldDelete Boolean reference indicating if the server should be deleted.
     */
void PigeonServer::Run(bool& shouldDelete) {
    signal(SIGPIPE, SIG_IGN);

        while (1)
        {
            /*
            * We need to store clients ip, just in case another client with the same ip tries to connect.
            * Checking is done before SSL/TLS handshake. If after TCP handhsake ip is duplicated, con will be closed.
            * 
            * This isnt really needed, but it provides another layer of security against attacks in a local network env.
            * Another approach should be used if this ever gets deployed.
            */

            sockaddr_in clientAddr;
            unsigned int len = sizeof(sockaddr_in);

            int client = accept(sSocket, (struct sockaddr*)&addr, &len);

            char clientIp[INET_ADDRSTRLEN];

            struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&addr;

            struct in_addr ipAddr = pV4Addr->sin_addr;

            inet_ntop(AF_INET, &ipAddr, clientIp, INET_ADDRSTRLEN);
        

        if (client < 0) {
            log->AddLog((GetDate() + " [ERR] Failed TCP Handshake\n").c_str());
            continue;
        }

        /*if(CheckIp(std::string(clientIp))){
            log->AddLog((GetDate() + " [ERR] IP is already in use \n").c_str());
            close(client);
            continue;
        }*/

        log->AddLog((GetDate() + " [OK] TCP Handshake\n").c_str());


        ssl = SSL_new(sslCtx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) == 0) {
            log->AddLog((GetDate() + " [ERR] Failed SSL/TLS Handshake\n").c_str());
            SSL_shutdown(ssl);
            SSL_free(ssl);
            continue;
        } else {
                log->AddLog((GetDate() + " [OK] SSL/TLS Handshake\n").c_str());
                Client* client1 = new Client();
                client1->clientSsl = ssl;
                client1->ipv4 = std::string(clientIp);
                auto latestClientIter = clients->insert({client,client1});

                /*
                *  Each client is managed within its own thread. Probably not the best approach efficiently wise, but its pretty simple to implement.
                *  Others approach with select() and non blocking network I/O calls might perform better but are not as easy to implement.   
                *  Thread is terminated when the read packet was empty (meaning error while recving or client was closed)
                *  or the recv packet was not processed correctly.
                *
                * 
                */

                std::thread([latestClientIter, this]{
                    log->AddLog((GetDate() + " [INFO] NEW THREAD FOR CLIENT FD: " + std::to_string(latestClientIter.first->first) + "\n").c_str());
                    while (1)
                    {   

                        std::vector<unsigned char> a = ReadPacket(latestClientIter.first->second->clientSsl);

                        if(!a.empty() && bytesRecv != nullptr){
                            std::unique_lock<std::mutex>(recvMutex);
                            *this->bytesRecv += a.size();
                        }

                        if(a.empty()){
     
                                if(clients->find(latestClientIter.first->first) != clients->end()){
                                    log->AddLog((GetDate() + " [FIN] ENDED THREAD FOR FD: " + std::to_string(latestClientIter.first->first) + "\n").c_str());             
                                    
                                    DisconnectClient(latestClientIter.first->second->clientSsl);
                                    FreeClient(latestClientIter.first->first,latestClientIter.first->second->clientSsl);

                                    //If a client doesnt complete the handshake a broadcast of presence will also be sent, shouldnt be a concern at least for now

                                    std::string toSend = "{";
                                    for(auto& c : *clients){
                                        if(!c.second->username.empty()){
                                            std::string pair = ( '"' + c.second->username + '"' + ":" + '"' + std::to_string(c.second->status) + '"' + ',');
                                            toSend += pair;
                                        }
                                    }
                                    
                                    if(toSend.length() >= 2){
                                        //remove last ,
                                        toSend.pop_back();
                                        toSend += '}';
                                    }
                                   
                                    BroadcastPacket(BuildPacket(PRESENCE_UPDATE,this->serverName,String::StringToBytes(toSend)));

                                    log->AddLog((GetDate() + " [FIN] AMOUNT OF CLIENTS: " + std::to_string(clients->size()) + "\n").c_str());             
                                }
                            break;
                        }
                        auto c = DeserializePacket(a);
                   
                        PigeonPacket toSend = ProcessPacket(c,latestClientIter.first->first);

                        if(!a.empty() && bytesRecv != nullptr){
                            std::unique_lock<std::mutex>(sentMutex);
                            *this->bytesSent += SerializePacket(toSend).size();
                        }

                        if(toSend.HEADER.OPCODE == ACK_MEDIA_DOWNLOAD){
                            log->AddLog((GetDate() + " [INFO] SENDING FILE TO " +latestClientIter.first->second->username + "\n").c_str());
                             auto bufToSend = SerializePacket(toSend);
                             SendAll(bufToSend,latestClientIter.first->second->clientSsl);
                             continue;
                        }

                        //if to send is server_hello, that means a successfull client_hello was read, so we notify the right client and then we broadcast the new presence list to evry client
                        if(toSend.HEADER.OPCODE == SERVER_HELLO){

                            auto bufToSend = SerializePacket(toSend);
                            SendAll(bufToSend,latestClientIter.first->second->clientSsl);

                                log->AddLog((GetDate() + " [BRD] UPDATE PRESENCE " + "\n").c_str());

                                    
                                    std::string toSend = "{";
                                    for(auto& c : *clients){
                                        std::string pair = ( '"' + c.second->username + '"' + ":" + '"' + std::to_string(c.second->status) + '"' + ',');
                                        toSend += pair;
                                    }
                                    //remove last ,
                                    toSend.pop_back();
                                    toSend += '}';
                                
                                BroadcastPacket(BuildPacket(PRESENCE_UPDATE,this->serverName,String::StringToBytes(toSend)));
                                

                               
                            continue;
                        }

                        //bad packet, close connection and notify all clients
                        if(toSend.HEADER.OPCODE == USER_COLLISION || toSend.HEADER.OPCODE == LENGTH_EXCEEDED || toSend.HEADER.OPCODE == JSON_NOT_VALID || toSend.HEADER.OPCODE == RATE_LIMITED || toSend.HEADER.OPCODE == FILE_NOT_FOUND || toSend.HEADER.OPCODE == USERNAME_MISMATCH){
                            if(clients->find(latestClientIter.first->first) != clients->end()){
                                
                                log->AddLog((GetDate() + " [FIN] ENDED THREAD FOR FD: " + std::to_string(latestClientIter.first->first) + "\n").c_str());             
                                
                                
                                DisconnectClient(latestClientIter.first->second->clientSsl);
                                FreeClient(latestClientIter.first->first,latestClientIter.first->second->clientSsl);

                                log->AddLog((GetDate() + " [BRD] UPDATE PRESENCE " + "\n").c_str());

                                    std::string toSend = "{";
                                    for(auto& c : *clients){
                                        std::string pair = ( '"' + c.second->username + '"' + ":" + '"' + std::to_string(c.second->status) + '"' + ',');
                                        toSend += pair;
                                    }
                                    //remove last ,
                                    toSend.pop_back();
                                    toSend += '}';
                                
                                BroadcastPacket(BuildPacket(PRESENCE_UPDATE,this->serverName,String::StringToBytes(toSend)));

                                log->AddLog((GetDate() + " [FIN] AMOUNT OF CLIENTS: " + std::to_string(clients->size()) + "\n").c_str());             
                                
                            }
                            break;
                        }

                        BroadcastPacket(toSend);

                    }
                    return;
                }).detach();
                
                
            }
        }

}



/**
     * @brief Serializes a PigeonPacket object. (From PigeonPacket to uchar vector)
     * @param packet The PigeonPacket to serialize.
     * @return The serialized packet as a vector of unsigned characters.
     */
std::vector<unsigned char> PigeonServer::SerializePacket(const PigeonPacket& packet){
    std::vector<unsigned char> serializedPacket;

    int headerLength = packet.HEADER.HEADER_LENGTH;
    unsigned char* headerLengthBytes = reinterpret_cast<unsigned char*>(&headerLength);
    serializedPacket.insert(serializedPacket.end(), headerLengthBytes, headerLengthBytes + sizeof(int));

    std::time_t timestamp = packet.HEADER.TIME_STAMP;
    unsigned char* timestampBytes = reinterpret_cast<unsigned char*>(&timestamp);
    serializedPacket.insert(serializedPacket.end(), timestampBytes, timestampBytes + sizeof(std::time_t));

    serializedPacket.insert(serializedPacket.end(), packet.HEADER.username.begin(), packet.HEADER.username.end());
    serializedPacket.push_back('\0'); 

    serializedPacket.push_back(static_cast<unsigned char>(packet.HEADER.OPCODE));

    int contentLength = packet.HEADER.CONTENT_LENGTH;
    unsigned char* contentLengthBytes = reinterpret_cast<unsigned char*>(&contentLength);
    serializedPacket.insert(serializedPacket.end(), contentLengthBytes, contentLengthBytes + sizeof(int));

    serializedPacket.insert(serializedPacket.end(), packet.PAYLOAD.begin(), packet.PAYLOAD.end());

    return serializedPacket;
}

/**
     * @brief Deserializes a packet. From uchar vector to PigeonPacket
     * @param packet The packet to deserialize.
     * @return The deserialized PigeonPacket object.
     */
PigeonPacket PigeonServer::DeserializePacket(std::vector<unsigned char>& packet){
    PigeonPacket packetRet;


    int offset = 0;

    std::memcpy(&packetRet.HEADER.HEADER_LENGTH, &packet[offset],sizeof(int));
    offset += sizeof(int);

    std::memcpy(&packetRet.HEADER.TIME_STAMP, &packet[offset], sizeof(std::time_t));
    offset += sizeof(std::time_t);

    while (packet[offset] != '\0') {
        packetRet.HEADER.username.push_back(packet[offset]);
        offset++;
    }

    offset++;
    //opc
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

PigeonPacket PigeonServer::BuildPacket(PIGEON_OPCODE opcode, const std::string& username, const std::vector<unsigned char>& payload){

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
PigeonPacket PigeonServer::ProcessPacket(PigeonPacket& recv, int clientFD){
    PigeonPacket newPacket;
    Json::Reader reader;
    Json::Value value;

    //If client completed handshake, username is stored in Client, if a client tries to send a packet before making a handshake (aka his username doesnt exist), bad client = close con,
    // that way we ensure a client has completed handshake properly


    switch (recv.HEADER.OPCODE)
    {
    case CLIENT_HELLO:
        if(!recv.PAYLOAD.empty() && !recv.HEADER.username.empty()){
            newPacket = BuildPacket(SERVER_HELLO,recv.HEADER.username,String::StringToBytes(R"({"ServerName":")" + this->serverName + R"("})"));
            
            if(recv.PAYLOAD.size() >  256 * 1000 * 1000){
                    log->AddLog((GetDate() + " [ERR] MESSAGE TOO BIG: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username,{});                
                    break;
            }

            auto it = this->clients->find(clientFD);
            
            if(!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value)){
                newPacket = BuildPacket(JSON_NOT_VALID, "",{});
                break;
            }

            log->AddLog((GetDate() + " [INFO] CLIENT HELLO FROM: " + recv.HEADER.username + " STATUS: " + value["status"].asString() + "\n").c_str());

            if (it != clients->end()) {
                it->second->logTimestamp = std::time(0);

                //Does username already exist?
                bool exists = false;
                for(auto& p : *this->clients){
                    if(p.second->username == recv.HEADER.username)
                        exists = true;
                }   

                //On connection, status will be Online by default.
                if(!exists){
                    it->second->username = recv.HEADER.username;

                    if(value["status"].asString() == "ONLINE"){
                        it->second->status = ONLINE;
                    }
                    else if(value["status"].asString() == "IDLE"){
                        it->second->status = IDLE;
                    }
                    else if(value["status"].asString() == "DND"){
                        it->second->status = DND;
                    }
                }else{
                    newPacket = BuildPacket(USER_COLLISION, recv.HEADER.username,{});
                    log->AddLog((GetDate() + " [ERR] USER COLLISION: " + recv.HEADER.username + " IS ALREADY USED" + "\n").c_str());
                }

                //Username max length check
                if(it->second->username.length() > MAX_USERNAME){
                    newPacket = BuildPacket(LENGTH_EXCEEDED,recv.HEADER.username,{});
                    log->AddLog((GetDate() + " [ERR] USERNAME LENGTH EXCEEDED: " + recv.HEADER.username + "\n").c_str());
                }
            }
        }
        else{
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        }
        break;

    case TEXT_MESSAGE:
        if(!recv.PAYLOAD.empty() && !recv.HEADER.username.empty()){

            //We can saftely assume that after a successfful CLIENT_HELLO, the clients fd has a valid username and is connected
            //So we need to check if the FD who sent the packet has the same username thats contained in the packet itself.
            
            log->AddLog((GetDate() + " [INFO] TEXT MESSAGE BY: " + recv.HEADER.username + " " + std::string(recv.PAYLOAD.data(), recv.PAYLOAD.data() + recv.HEADER.CONTENT_LENGTH) + "\n").c_str());

            auto it = clients->find(clientFD);
            if(it->second->username == recv.HEADER.username){
                //If both usernames match, we just need to verify the packets payload
                if(recv.PAYLOAD.size() >  512){
                    log->AddLog((GetDate() + " [ERR] MESSAGE TOO BIG: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username,{});                
                    break;
                }
                newPacket = BuildPacket(TEXT_MESSAGE, recv.HEADER.username,recv.PAYLOAD);

            }
            else {
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username,{});
            }
            


        }else{
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        }
        break;

    case MEDIA_FILE:
        if(!recv.PAYLOAD.empty() && !recv.HEADER.username.empty()){

            log->AddLog((GetDate() + " [INFO] NEW MEDIA FILE BY: " + recv.HEADER.username + " SIZE: " + std::to_string((recv.PAYLOAD.size() / (1000))) +" KB \n").c_str());

            auto it = clients->find(clientFD);
            if(it->second->username == recv.HEADER.username){


                if(recv.PAYLOAD.size() >  256 * 1000 * 1000){
                    log->AddLog((GetDate() + " [ERR] MESSAGE TOO BIG: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username,{});                
                    break;
                }

                if(std::time(0) - it->second->logTimestamp < 15){
                    newPacket = BuildPacket(RATE_LIMITED, recv.HEADER.username,{});
                    log->AddLog((GetDate() + " [ERR] RATE LIMITED: " + recv.HEADER.username + "\n").c_str());
                    break;
                }

                it->second->logTimestamp = std::time(0);
                if(!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value)){
                    log->AddLog((GetDate() + " [ERR] MALFORMED MEDIA PACKET: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username,{});
                    break;
                }


                std::string fileContent = "";
                std::string fileExt = "";
                std::string fileName = "";

                fileContent = value["content"].asString();
                fileExt = value["ext"].asString();
                fileName = value["filename"].asString();
                
                //std::cout << fileExt << fileName << std::endl;

                if(fileContent == "" || fileExt == "" || fileName == ""){
                    log->AddLog((GetDate() + " [ERR] MALFORMED MEDIA PACKET: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username,{});
                    break;
                }
                auto contentBuffer = String::StringToBytes(B64::base64_decode(fileContent));
                bool err = File::BufferToDisk(recv.PAYLOAD, ("Files/" + fileName + ".json"));

                newPacket = BuildPacket(MEDIA_FILE,recv.HEADER.username,String::StringToBytes(R"({"filename":")" + fileName + R"("})"));

            }else{
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username,{});
            }
            break;            
        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        }
        break;

    case MEDIA_DOWNLOAD:

        log->AddLog((GetDate() + " [INFO] NEW MEDIA DOWNLOAD REQUEST BY: " + recv.HEADER.username + "\n").c_str());

        if(!recv.PAYLOAD.empty() && !recv.HEADER.username.empty()){
            
            auto it = clients->find(clientFD);

            if(it->second->username != recv.HEADER.username){
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username,{});
                break;
            }

            if(recv.PAYLOAD.size() >  256 * 1000 * 1000){
                log->AddLog((GetDate() + " [ERR] MESSAGE TOO BIG: " + recv.HEADER.username + "\n").c_str());
                newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username,{});                
                break;
            }
            
            if(std::time(0) - it->second->logTimestamp < 15){
                    newPacket = BuildPacket(RATE_LIMITED, recv.HEADER.username,{});
                    log->AddLog((GetDate() + " [ERR] RATE LIMITED: " + recv.HEADER.username + "\n").c_str());
                    break;
                }

            it->second->logTimestamp = std::time(0);

            if(!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value)){
                    log->AddLog((GetDate() + " [ERR] MALFORMED DOWNLOAD PACKET: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username,{});
                    break;
            }
            
            std::string filename = "";
            filename = value["filename"].asString();

                if(filename == ""){
                    log->AddLog((GetDate() + " [ERR] MALFORMED DOWNLOAD PACKET: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(JSON_NOT_VALID, recv.HEADER.username,{});
                    break;
                }
            

            auto buffer = File::DiskToBuffer("Files/"+filename+".json");

            if(buffer.empty()){
                log->AddLog((GetDate() + " [ERR] FILE NOT FOUND: " + recv.HEADER.username + "\n").c_str());
                newPacket = BuildPacket(FILE_NOT_FOUND, recv.HEADER.username,{});
                break;
            }
            
            newPacket = BuildPacket(ACK_MEDIA_DOWNLOAD, recv.HEADER.username,buffer);

        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        }

        break;

    case PRESENCE_REQUEST:

        if(!recv.HEADER.username.empty()){

            log->AddLog((GetDate() + " [INFO] NEW PRESENCE REQUEST BY: " + recv.HEADER.username + "\n").c_str());

            auto it = clients->find(clientFD);

            if(it->second->username != recv.HEADER.username){
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username,{});
                break;
            }

            //ugly code, but it works

            std::string toSend = "{";
            for(auto& c : *clients){
                std::string pair = ( '"' + c.second->username + '"' + ":" + '"' + std::to_string(c.second->status) + '"' + ',');
                toSend += pair;
            }
            
            //remove last ,
            toSend.pop_back();
            toSend += '}';


            newPacket = BuildPacket(PRESENCE_UPDATE,recv.HEADER.username,String::StringToBytes(toSend));

        }
        else
        {
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        }

        break;


    case PRESENCE_UPDATE:
        if(!recv.PAYLOAD.empty() && !recv.HEADER.username.empty()){
            log->AddLog((GetDate() + " [INFO] NEW PRESENCE REQUEST BY: " + recv.HEADER.username + "\n").c_str());

            if(recv.PAYLOAD.size() >  256 * 1000 * 1000){
                    log->AddLog((GetDate() + " [ERR] MESSAGE TOO BIG: " + recv.HEADER.username + "\n").c_str());
                    newPacket = BuildPacket(LENGTH_EXCEEDED, recv.HEADER.username,{});                
                    break;
            }

            auto it = this->clients->find(clientFD);
            
            if(it->second->username != recv.HEADER.username){
                newPacket = BuildPacket(USERNAME_MISMATCH, recv.HEADER.username,{});
                break;
            }

            if(!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value)){
                newPacket = BuildPacket(JSON_NOT_VALID, "",{});
                break;
            }

                    if(value["status"].asString() == "ONLINE"){
                        it->second->status = ONLINE;
                    }
                    else if(value["status"].asString() == "IDLE"){
                        it->second->status = IDLE;
                    }
                    else if(value["status"].asString() == "DND"){
                        it->second->status = DND;
                    }
            
            std::string toSend = "{";
            for(auto& c : *clients){
                std::string pair = ( '"' + c.second->username + '"' + ":" + '"' + std::to_string(c.second->status) + '"' + ',');
                toSend += pair;
            }
            
            //remove last ,
            toSend.pop_back();
            toSend += '}';


            newPacket = BuildPacket(PRESENCE_UPDATE,recv.HEADER.username,String::StringToBytes(toSend));
        
        }else{
            newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        }
    default:
        newPacket = BuildPacket(PROTOCOL_MISMATCH, recv.HEADER.username,{});
        break;

    }
    return newPacket;
}


/**
     * @brief Read incoming packet from a valid SSL/TLS connection.
     * @param ssl1 SSL/TLS connection from an active client.
     */
//Packet is 4 bytes + 8 bytes + 1 byte + username (max 20 chars == 20 bytes) + 1 byte null char + 4 bytes payload size + payload ( max 256 MB)  
std::vector<unsigned char> PigeonServer::ReadPacket(SSL* ssl1){
    std::vector<unsigned char> packetBuffer(MAX_HEADER);

    int total = TcpServer::Recv(packetBuffer,4,0,ssl1);

    //This could also mean error when receving. In case of error/wrong packet, empty vector is returned and the connection will be closed
    if(total < 4){
        return {};
    }

    int headerLength = 0;
    for (int i = 0; i < 4; ++i) {
        headerLength |= packetBuffer[i] << (8 * i); 
    }

    if(headerLength > MAX_HEADER){
        log->AddLog((GetDate() + " [FATAL] PACKET HEADER TOO BIG \n").c_str());
        return {};
    }

    total = TcpServer::Recv(packetBuffer,headerLength + 4,total,ssl1);


    packetBuffer.resize(total);

    int payloadLength = 0;
    for (int i = packetBuffer.size()-1; i >= packetBuffer.size() - 4; --i) {
        payloadLength = (payloadLength << 8) | packetBuffer[i];
    }

    if(payloadLength == 0){
        return packetBuffer;
    }

    packetBuffer.resize(packetBuffer.size() + payloadLength);

    total = TcpServer::Recv(packetBuffer,headerLength + 4 + payloadLength,total,ssl1);


    return packetBuffer;
}

/**
     * @brief Sends a PigeonPacket to all connected clients.
     * @param packet Packet to sent.
     */
void* PigeonServer::BroadcastPacket(const PigeonPacket& packet){
    auto packetToSend = SerializePacket(packet);
    int sent = -1;
    std::string clientsStr = "";
    if(!packetToSend.empty()){
        for(auto& c : *clients){
            clientsStr += std::to_string(c.first) + " ";
            sent = SendAll(packetToSend, c.second->clientSsl);

        }
    }
    return nullptr;
}


