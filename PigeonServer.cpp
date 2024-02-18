#include "PigeonServer.h"


PigeonServer::PigeonServer(const std::string& certPath, const std::string& keyPath, const std::string& serverName, unsigned short port):
    TcpServer(certPath,keyPath,port), serverName(serverName)
{
    clients = new std::unordered_map<int,Client>();

    std::cout << "Setting up tcp server" << std::endl;
    std::cout << this->certPath << " - " << this->privateKey << std::endl;

    if(TcpServer::Setup() != 0){
        throw std::string("[ERROR] PigeonServer::PigeonServer Error while setting up tcp server");
    }

    this->logger = new Logger();
};



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
            std::cout << shouldDelete << std::endl;
            if(shouldDelete)
                break;
            continue;
        }

        int client = accept(sSocket, (struct sockaddr*)&addr, &len);

        if (client < 0) {
            std::cerr << "Unable to accept client" << std::endl;
            continue;
        }

            logger->log(INFO, " OK TCP HANDSHAKE");

        ssl = SSL_new(sslCtx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) == 0) {
            logger->log(ERROR, " FAILED SSL/TLS HANDSHAKE");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            continue;
        } else {
                logger->log(INFO, " OK SSL/TLS HANDSHAKE");
                Client client1;
                client1.clientSsl = ssl;
                auto latestClientIter = clients->insert({client,client1});

                std::thread([latestClientIter, this]{
                    logger->log(INFO, " NEW THREAD FOR CLIENT FD: " + std::to_string(latestClientIter.first->first));
                    while (1)
                    {   
                        auto a = ReadPacket(latestClientIter.first->second.clientSsl);
                        //logger->log(DEBUG, "PACKET RECV FROM FD: " + std::to_string(latestClientIter.first->first) + " SIZE: " + std::to_string(a.size()) + " BYTES");

                        if(a.empty()){
                            //logger->log(INFO, "ENDED THREAD FOR FD: " + std::to_string(latestClientIter.first->first));
                            
                            int key = latestClientIter.first->first;
                            Client client = latestClientIter.first->second;                                               

                            //posible race condition con destructor pero me da igual
                            this->FreeClient(key,client.clientSsl);                            
                            //logger->log(INFO, "AMOUNT OF CLIENTS: " + std::to_string(clients.size()));
                            break;
                        }

                        //PROCESS PACKET
                        PigeonPacket pkt = BuildPacket(CLIENT_HELLO, "JOZESE", {});
                         

                        //idk if mutex necessry but just in case
                        std::unique_lock<std::mutex>(mapMutex);
                        BroadcastPacket(pkt);

                    }
                }).detach();
                
                
            }
        }

}




std::vector<unsigned char> PigeonServer::SerializePacket(const PigeonPacket& packet){
    std::vector<unsigned char> serializedPacket;

    int headerLength = packet.HEADER.HEADER_LENGTH;
    unsigned char* headerLengthBytes = reinterpret_cast<unsigned char*>(&headerLength);
    serializedPacket.insert(serializedPacket.end(), headerLengthBytes, headerLengthBytes + sizeof(int));

    std::time_t timestamp = packet.HEADER.TIME_STAMP;
    unsigned char* timestampBytes = reinterpret_cast<unsigned char*>(&timestamp);
    serializedPacket.insert(serializedPacket.end(), timestampBytes, timestampBytes + sizeof(std::time_t));

    serializedPacket.insert(serializedPacket.end(), packet.HEADER.username.begin(), packet.HEADER.username.end());
    serializedPacket.push_back('\0'); // Null terminator for the username

    serializedPacket.push_back(static_cast<unsigned char>(packet.HEADER.OPCODE));

    int contentLength = packet.HEADER.CONTENT_LENGTH;
    unsigned char* contentLengthBytes = reinterpret_cast<unsigned char*>(&contentLength);
    serializedPacket.insert(serializedPacket.end(), contentLengthBytes, contentLengthBytes + sizeof(int));

    serializedPacket.insert(serializedPacket.end(), packet.PAYLOAD.begin(), packet.PAYLOAD.end());

    return serializedPacket;
}

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




int PigeonServer::ProcessPacket(){
    return 0;
}

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

void* PigeonServer::BroadcastPacket(const PigeonPacket& packet){
    auto packetToSend = SerializePacket(packet);
    int sent = -1;
    std::string clientsStr = "";
    if(!packetToSend.empty()){
        for(auto& c : *clients){
            clientsStr += std::to_string(c.first) + " ";
            sent = SendAll(packetToSend, c.second.clientSsl);

        }
        logger->log(DEBUG, "BRD " + std::to_string(sent) + " BYTES TO FDs: " + clientsStr);
    }
    return nullptr;
}


