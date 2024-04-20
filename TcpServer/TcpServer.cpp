#include "TcpServer.h"
#include <thread>

//TODO: Disable SSLv2 & SSLv3 negociation
TcpServer::TcpServer(const std::string& cert, const std::string& privateKey, unsigned short port):certPath(cert),
privateKey(privateKey), port(port){

    //Need to check if sucess
    this->sslCtx = SSL_CTX_new(TLS_server_method());
    
}

int TcpServer::SocketSetup(){

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (sSocket < 0) {
        std::cerr << "Error setting up socket" << std::endl;
        return -1;
    }

    int optval = 1;
    if (setsockopt(sSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting up socket options" << std::endl;
        return -1;    
    }

    if (bind(sSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error binding up socket" << std::endl;
        return -1;
    }

    if (listen(sSocket, 1) < 0) {
        std::cerr << "Error listening socket" << std::endl;
        return -1;
    }

    return 0;
}

//Only able to use PEM format
int TcpServer::CtxConfig(){
    if (SSL_CTX_use_certificate_file(sslCtx, certPath.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "cert error" << std::endl;
        return -1;
    }
    if (SSL_CTX_use_PrivateKey_file(sslCtx, privateKey.c_str(), SSL_FILETYPE_PEM) <= 0 ) {
        std::cerr << "key error" << std::endl;
        return -1;
    }
    return 0;
}

int TcpServer::Setup(){
    if(CtxConfig() != 0 ){
        std::cerr << "Error while configuring ssl context" << std::endl;
        return -1;
    };
    
    if(SocketSetup()){
        std::cerr << "Error while setting up socket" << std::endl;
        return -1;
    };

    return 0;
}

int TcpServer::Recv(std::vector<unsigned char>& buf, size_t toRecv, int total, SSL* ssl1, Logger* logger){

    if(logger)
        logger->log(DEBUG, "DOWNLOADING PAYLOAD");

    do{
        int nRecv = SSL_read(ssl1, buf.data() + total, toRecv - total);
        
        if(logger)
            logger->log(DEBUG_DOWNLOAD, std::to_string(total) + " BYTES / " + std::to_string(toRecv) + " BYTES");

        if(nRecv <= 0)
            break;
        total += nRecv;

    }while (total < toRecv);
    
    if(logger)
            logger->log(DEBUG, std::to_string(total) + " BYTES / " + std::to_string(toRecv) + " BYTES");

    return total;
}

int TcpServer::SendAll(std::vector<unsigned char>& buf, SSL* ssl){
    int totalSent = 0;
    int leftToSend = buf.size();
    int nSent;

    while (totalSent < buf.size()) {
        nSent = SSL_write(ssl, buf.data() + totalSent, leftToSend);

        if (nSent <= 0) {
            int error = SSL_get_error(ssl, nSent);
            std::cout << "Error while sending: " << error << std::endl;
            break;
        }

        totalSent += nSent;
        leftToSend -= nSent;

        if (leftToSend == 0) {
            break;
        }
    }

    return totalSent;
}