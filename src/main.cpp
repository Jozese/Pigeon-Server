#include <iostream>
#include "PigeonData.h"
#include "PigeonServer.h"

#define PATH_TO_CONFIG "config.json"

int main(int argc, char *argv[])
{
   system("clear");

   Logger *logger = new Logger();

            
   PigeonData data(PATH_TO_CONFIG);

   PigeonServer *server = new PigeonServer(data, logger);
   server->Run();
   
   delete server;
   delete logger;

   return EXIT_SUCCESS;
}