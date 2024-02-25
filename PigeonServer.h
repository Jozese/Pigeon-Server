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
#include <cstring>

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
#include <jsoncpp/json/json.h>

#include "TcpServer/TcpServer.h"
#include "PigeonPacket.h"
#include "PigeonServerGUI/ImGuiLogger.h"
#include <thread>

enum Status{
    ONLINE = 0,
    IDLE = 1,
    DND = 2,
};

/**
 * @struct Client
 * @brief Representation of a client in a Pigeon Server
 */

struct Client{
    SSL* clientSsl;
    std::time_t logTimestamp;
    std::string username;
    Status status;

    Client():clientSsl(nullptr), logTimestamp(std::time(0)), username(""), status(ONLINE){};
};

//UTILS
static void printBytesInHex(const std::vector<unsigned char>& bytes) {
    for (const unsigned char& byte : bytes) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " " << std::dec;
    }
    std::cout << std::endl;
}

static std::vector<unsigned char> StringToBytes(const std::string& str){
    return std::vector<unsigned char>(str.begin(), str.end());
}


/**
 * @class PigeonServer
 * @brief A Server based on the Pigeon Protocol
 */

class PigeonServer: public TcpServer{

public:
    PigeonServer(const std::string& certPath, const std::string& keyPath, const std::string& serverName, unsigned short port, ImGuiLog* log);
    ~PigeonServer(){
        log->AddLog((GetDate() + " [INFO] DELETING SERVER \n").c_str());
        for(auto& p : *clients){
                SSL_shutdown(p.second->clientSsl);
                close(p.first);
            log->AddLog((GetDate() + " [FREE] FREED CLIENT FD: " + std::to_string(p.first) + "\n").c_str());
        }
        //This is really bad practice but fixes the segfault, mutex is probably needed somewhere else
                        sleep(5);

        log->Clear();

        delete clients;
        delete bytesRecv;
        delete bytesSent;
        delete log;

        bytesRecv = nullptr;
        bytesSent = nullptr;
        clients = nullptr;
        log = nullptr;




    };
public:
    void Run(bool&  shouldDelete);
    
    std::vector<unsigned char> ReadPacket(SSL* ssl1);
    PigeonPacket ProcessPacket(PigeonPacket& recv, int clientFD);

    std::vector<unsigned char> SerializePacket(const PigeonPacket& packet);
    PigeonPacket DeserializePacket(std::vector<unsigned char>& packet);

    PigeonPacket BuildPacket(PIGEON_OPCODE opcode, const std::string& username, const std::vector<unsigned char>& payload);

    void* BroadcastPacket(const PigeonPacket& packet);

    void FreeClient(int c, SSL* cSSL){
        shutdown(c,SHUT_RDWR);
        close(c);
        auto it = clients->find(c);
        if (it != clients->end()) { 
            delete it->second; 
            clients->erase(it); 
        }
        SSL_free(cSSL);
    };
    
    void DisconnectClient(SSL* cSSL){
        SSL_shutdown(cSSL);
    }
    

public:
    inline std::unordered_map<int,Client*>* GetClients(){
        return this->clients;
    }

    inline std::string GetDate(){
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

        std::string time = std::string(std::ctime(&currentTime));
        size_t pos = time.find('\n');
        if (pos != std::string::npos) {
            time.erase(pos);
        }
        return time;
    }

    double* bytesRecv = nullptr;
    double* bytesSent = nullptr;

private:
    std::string serverName = "";
    std::mutex mapMutex;
    bool isLocked = false;
    ImGuiLog* log = nullptr;

private:
    std::unordered_map<int,Client*>* clients;
    std::vector<std::thread> threadPool;
};
