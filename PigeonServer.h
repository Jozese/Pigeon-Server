/*
*   @author jozese
*   
*
*
*/



#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <chrono>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <unordered_map>
#include <stdexcept>
#include <iomanip>
#include <mutex>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "TcpServer/TcpServer.h"
#include "Logger/Logger.h"
#include "PigeonPacket.h"

struct Client{
    SSL* clientSsl;
    std::time_t logTimestamp;
    std::string username;

    Client():clientSsl(nullptr), logTimestamp(std::time(0)), username(""){};
};
static void printBytesInHex(const std::vector<unsigned char>& bytes) {
    for (const unsigned char& byte : bytes) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " " << std::dec;
    }
    std::cout << std::endl;
}

class PigeonServer: public TcpServer{

public:
    PigeonServer(const std::string& certPath, const std::string& keyPath, const std::string& serverName, unsigned short port);
    ~PigeonServer(){
        for(auto& p : clients){
            std::cout << "FREEING CLIENT " << std::to_string(p.first) << std::endl;
            this->FreeClient(p.first,p.second.clientSsl);
            std::cout << "FREED " << std::endl;
        }
        std::cout << "DELETED" << std::endl;
    };
public:
    void Run(bool&  shouldDelete);
    
    std::vector<unsigned char> ReadPacket(SSL* ssl1);
    int ProcessPacket();

    std::vector<unsigned char> SerializePacket(const PigeonPacket& packet);
    PigeonPacket DeserializePacket(std::vector<unsigned char>& packet);

    PigeonPacket BuildPacket(PIGEON_OPCODE opcode, const std::string& username, const std::vector<unsigned char>& payload);

    void* BroadcastPacket(const PigeonPacket& packet);

    void FreeClient(int c, SSL* cSSL){
        SSL_shutdown(cSSL);
        close(c);
    };

private:
    std::string serverName = "";
    std::mutex mapMutex;
    bool isLocked = false;

private:
    std::unordered_map<int,Client> clients;
    Logger* logger = nullptr;
};
