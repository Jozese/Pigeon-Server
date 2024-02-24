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
    clients = new std::unordered_map<int,Client>();
    this->bytesRecv = new double(0);
    this->bytesSent = new double(0);


    log->AddLog((GetDate() + " [INFO] Setting up TCP server\n").c_str());
    std::cout << this->certPath << " - " << this->privateKey << std::endl;

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
            
        sockaddr_in clientAddr;
        unsigned int len = sizeof(sockaddr_in);

        struct timeval timeout;
        timeout.tv_sec = 1; 
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sSocket, &readfds);

        int result = select(sSocket + 1, &readfds, nullptr, nullptr, &timeout);
        if (result < 0) {
            break;
        } else if (result == 0) {
            if(shouldDelete)
                break;
            continue;
        }

        int client = accept(sSocket, (struct sockaddr*)&addr, &len);

        if (client < 0) {
            log->AddLog((GetDate() + " [ERR] Failed TCP Handshake\n").c_str());
            continue;
        }

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
                Client client1;
                client1.clientSsl = ssl;
                auto latestClientIter = clients->insert({client,client1});

                std::thread([latestClientIter, this]{
                    log->AddLog((GetDate() + " [INFO] NEW THREAD FOR CLIENT FD: " + std::to_string(latestClientIter.first->first) + "\n").c_str());
                    //logger->log(INFO, " NEW THREAD FOR CLIENT FD: " + std::to_string(latestClientIter.first->first));
                    while (1)
                    {   

                        auto a = ReadPacket(latestClientIter.first->second.clientSsl);
                        //logger->log(DEBUG, "PACKET RECV FROM FD: " + std::to_string(latestClientIter.first->first) + " SIZE: " + std::to_string(a.size()) + " BYTES");

                        *this->bytesRecv += a.size();

                        if(a.empty()){
                            log->AddLog((GetDate() + " [INFO] ENDED THREAD FOR FD: " + std::to_string(latestClientIter.first->first) + "\n").c_str());
                            //logger->log(INFO, "ENDED THREAD FOR FD: " + std::to_string(latestClientIter.first->first));
                            
                            int key = latestClientIter.first->first;
                            Client client = latestClientIter.first->second;                                               

                            //posible race condition con destructor pero me da igual
                            this->FreeClient(key,client.clientSsl);    
                            log->AddLog((GetDate() + " [INFO] AMOUNT OF CLIENTS: " + std::to_string(clients->size()) + "\n").c_str());             
                            break;
                        }

                        //PROCESS PACKET
                        std::unique_lock<std::mutex>(mapMutex);

                        auto c = DeserializePacket(a);
                   
                        PigeonPacket toSend = ProcessPacket(c,latestClientIter.first->first);

                        //idk if mutex necessry but just in case
                        BroadcastPacket(toSend);



                    }
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


    //opc
    packetRet.HEADER.OPCODE = static_cast<PIGEON_OPCODE>(packet[offset]);
    offset++;

    std::memcpy(&packetRet.HEADER.CONTENT_LENGTH, &packet[offset], sizeof(int));
    offset += sizeof(int);

    packetRet.PAYLOAD.assign(packet.begin() + offset + 1, packet.end());
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
        pkt.HEADER.OPCODE = CLIENT_HELLO;
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
PigeonPacket PigeonServer::ProcessPacket(PigeonPacket& recv, int clientFD){
    PigeonPacket newPacket;
    Json::Reader reader;
    Json::Value value;

    std::cout << recv.HEADER.OPCODE << std::endl;

    switch (recv.HEADER.OPCODE)
    {
    case CLIENT_HELLO:
        if(!recv.PAYLOAD.empty() && !recv.HEADER.username.empty()){
            newPacket = BuildPacket(SERVER_HELLO,recv.HEADER.username,StringToBytes(R"({"ServerName":")" + this->serverName + R"("})"));
            

            auto it = this->clients->find(clientFD);
            
            if(!reader.parse(std::string(recv.PAYLOAD.begin(), recv.PAYLOAD.end()), value)){
                std::cout << "FAILED READING" << std::endl;
                printBytesInHex(recv.PAYLOAD);
                newPacket = BuildPacket(JSON_NOT_VALID, "",{});
            }

            log->AddLog((GetDate() + " [INFO] CLIENT HELLO FROM: " + recv.HEADER.username + " STATUS: " + value["status"].asString() + "\n").c_str());

            if (it != clients->end()) {
                it->second.logTimestamp = std::time(0);
                it->second.username = recv.HEADER.username;
                if(value["status"].asString() == "ONLINE"){
                    it->second.status = ONLINE;
                }
                else if(value["status"].asString() == "IDLE"){
                    it->second.status = IDLE;
                }
                else if(value["status"].asString() == "DND"){
                    it->second.status = DND;
                }
            }
        }else{
            newPacket = BuildPacket(PROTOCOL_MISMATCH, "",{});
        }
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

    if(total < 4){
        return {};
    }

    int headerLength = 0;
    for (int i = 0; i < 4; ++i) {
        headerLength |= packetBuffer[i] << (8 * i); 
    }

    total = TcpServer::Recv(packetBuffer,headerLength + 4,total,ssl1);

    packetBuffer.resize(total);

    int payloadLength = 0;
    for (int i = packetBuffer.size()-1; i >= packetBuffer.size() - 4; --i) {
        payloadLength = (payloadLength << 8) | packetBuffer[i];
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
            sent = SendAll(packetToSend, c.second.clientSsl);

        }
        //logger->log(DEBUG, "BRD " + std::to_string(sent) + " BYTES TO FDs: " + clientsStr);
    }
    return nullptr;
}


