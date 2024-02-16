/*
*   @author Jozese
*   
*/

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <map>

#include <openssl/ssl.h>
#include <openssl/err.h>

class TcpServer{
public:
    TcpServer(const std::string& cert, const std::string& privateKey, unsigned short port);
public:
    int SocketSetup();
    int CtxConfig();
    int Setup();

    int Recv(std::vector<unsigned char>& buf, size_t toRecv, int total=0, SSL* ssl=nullptr);
    int SendAll(std::vector<unsigned char>& buf, SSL* ssl);
public:
    inline int GetSocketFD(){ return sSocket;};
protected:
    int sSocket = -1;

    unsigned short port = 0;
    std::string certPath = "";
    std::string privateKey = "";

    SSL_CTX* sslCtx = nullptr;
    SSL* ssl = nullptr;


    std::map<int,SSL*> sslPairMap;

    sockaddr_in addr;
};