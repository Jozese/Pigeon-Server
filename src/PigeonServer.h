/*
 *   @author jozese
 *
 *
 *
 */

#pragma once

#include "PigeonData.h"

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

#include "../TcpServer/TcpServer.h"
#include "PigeonPacket.h"
#include "Utils.h"
#include "../Logger/Logger/Logger.h"
#include <thread>

enum Status
{
    ONLINE = 0,
    IDLE = 1,
    DND = 2,
};

/**
 * @struct Client
 * @brief Representation of a client in a Pigeon Server
 */

struct Client
{
    std::string ipv4;
    SSL *clientSsl;
    std::time_t logTimestamp;
    std::string username;
    Status status;
    bool hasLogged = false;

    Client() : clientSsl(nullptr), logTimestamp(std::time(0)), username(""), status(ONLINE), ipv4(""){};
};

/**
 * @class PigeonServer
 * @brief A Server based on the Pigeon Protocol on top of a TCP/TLS server
 */

class PigeonServer : public TcpServer
{

public:
    PigeonServer(PigeonData& data, Logger *logger);
    PigeonServer(const std::string &certPath, const std::string &keyPath, const std::string &serverName, unsigned short port, Logger *logger);
    ~PigeonServer()
    {
        for (auto &p : *clients)
        {
            SSL_shutdown(p.second->clientSsl);
            close(p.first);
        }


        delete clients;
        clients = nullptr;
    };

public:
    void Run();

    std::vector<unsigned char> ReadPacket(SSL *ssl1);
    PigeonPacket ProcessPacket(PigeonPacket &recv, int clientFD);

    std::vector<unsigned char> SerializePacket(const PigeonPacket &packet);
    PigeonPacket DeserializePacket(std::vector<unsigned char> &packet);

    PigeonPacket BuildPacket(PIGEON_OPCODE opcode, const std::string &username, const std::vector<unsigned char> &payload);

    void* BroadcastPacket(const PigeonPacket &packet);

    void NotifyNewPresence();

    // UTILS
public:
    inline std::unordered_map<int, Client *> *GetClients()
    {
        return this->clients;
    }

    inline std::string GetDate()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

        std::string time = std::string(std::ctime(&currentTime));
        size_t pos = time.find('\n');
        if (pos != std::string::npos)
        {
            time.erase(pos);
        }
        return time;
    }

    /*
        Properly frees a client by its FD
    */
    inline void FreeClient(int c)
    {

        auto it = clients->find(c);
        if (it != clients->end())
        {
            SSL_free(it->second->clientSsl);
            it->second->clientSsl = nullptr;
            delete it->second;
            clients->erase(it);
        }
        
    };


    inline void DisconnectClient(int c,SSL *cSSL)
    {   
        SSL_shutdown(cSSL);
        shutdown(c, SHUT_RDWR);
        close(c);
    }

    inline bool CheckIp(const std::string &ipv4)
    {
        for (auto &c : *clients)
        {
            if (c.second->ipv4 == ipv4)
            {
                return true;
            }
        }
        return false;
    }

    //Not  used
    inline bool isBase64(const std::string &str)
    {
        std::regex b64Pattern("^(?:[A-Za-z0-9+\\/]{4})*(?:[A-Za-z0-9+\\/]{2}==|[A-Za-z0-9+\\/]{3}=|[A-Za-z0-9+\\/]{4})$");
        return std::regex_match(str, b64Pattern);
    }

private:
    std::string serverName = "";
    Logger *logger = nullptr;
    PigeonData* m_data = nullptr;

private:
    std::unordered_map<int, Client *> *clients;
    std::mutex m_clientsMtx;

};
