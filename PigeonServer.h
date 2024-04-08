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
#include <regex>
#include <mutex>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <jsoncpp/json/json.h>

#include "TcpServer/TcpServer.h"
#include "PigeonPacket.h"
#include "PigeonServerGUI/ImGuiLogger.h"
#include "Utils.h"
#include "Logger/Logger/Logger.h"
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
    std::string ipv4;
    SSL* clientSsl;
    std::time_t logTimestamp;
    std::string username;
    Status status;

    Client():clientSsl(nullptr), logTimestamp(std::time(0)), username(""), status(ONLINE), ipv4(""){};
};





/**
 * @class PigeonServer
 * @brief A Server based on the Pigeon Protocol
 */

class PigeonServer: public TcpServer{

public:
    PigeonServer(const std::string& certPath, const std::string& keyPath, const std::string& serverName, unsigned short port, ImGuiLog* log, Logger* logger);
    ~PigeonServer(){
        log->AddLog((GetDate() + " [INFO] DELETING SERVER \n").c_str());
        for(auto& p : *clients){
                SSL_shutdown(p.second->clientSsl);
                close(p.first);
            log->AddLog((GetDate() + " [FREE] FREED CLIENT FD: " + std::to_string(p.first) + "\n").c_str());
        }

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

    
    
//UTILS
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

    inline void FreeClient(int c, SSL* cSSL){
        shutdown(c,SHUT_RDWR);
        close(c);
        auto it = clients->find(c);
        if (it != clients->end()) { 
            delete it->second; 
            clients->erase(it); 
        }
        SSL_free(cSSL);
    };
    
    inline void DisconnectClient(SSL* cSSL){
        SSL_shutdown(cSSL);
    }

    inline bool CheckIp(const std::string& ipv4){
        for(auto& c : *clients){
            if(c.second->ipv4 == ipv4){
                return true;
            }
        }
        return false;
    }

    inline bool isBase64(const std::string& str){
        std::regex b64Pattern("^(?:[A-Za-z0-9+\\/]{4})*(?:[A-Za-z0-9+\\/]{2}==|[A-Za-z0-9+\\/]{3}=|[A-Za-z0-9+\\/]{4})$");
        return std::regex_match(str,b64Pattern);
    }

public:
    double* bytesRecv = nullptr;
    double* bytesSent = nullptr;

private:
    std::string serverName = "";

    //mutexes for send and recvd bytes, could also use atomics
    std::mutex recvMutex;
    std::mutex sentMutex;

    bool isLocked = false;
    ImGuiLog* log = nullptr;
    Logger* logger = nullptr;

private:
    std::unordered_map<int,Client*>* clients;
    std::vector<std::thread> threadPool;
};
