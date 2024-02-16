#include "PigeonServer.h"


int main(){

   PigeonServer server("../TcpServer/cert.pem","../TcpServer/key.pem","EU-001", 443);

   server.Run();

}
