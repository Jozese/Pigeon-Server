#include <iostream>
#include <SDL2/SDL.h>
#include "ImGui/imgui_impl_sdl2.h"
#include "ImGui/imgui_impl_opengl3.h"
#include <GL/glew.h>

#include "PigeonServerGUI/PigeonServerGUI.h"

int main(int argc, char* argv[])
{
   Logger* logger = new Logger();

   //fix
   bool shouldDelete = false;

   if(strcmp(argv[1],"--headless") == 0){

        static std::string serverName = "PGN-EU-1";
        static std::string serverPort = "4444";
        static std::string certPath = "cert.pem";
        static std::string keyPath = "key.pem";

      PigeonServer *server = new PigeonServer(certPath,keyPath,serverName,std::stoi(serverPort),nullptr,logger);
      server->Run(shouldDelete);
      delete server;
   }
   else{

      PigeonServerGUI::logger = logger;

      if(PigeonServerGUI::CreateWindow() != 0)
      {
         std::cerr << "Error to create SDL window " << std::endl;
      }

      PigeonServerGUI::SetUpImGui();
      

      while (!PigeonServerGUI::shouldClose)
      {


         while (SDL_PollEvent(&PigeonServerGUI::currentEvent))
         {
            ImGui_ImplSDL2_ProcessEvent(&PigeonServerGUI::currentEvent);
            if (PigeonServerGUI::currentEvent.type == SDL_QUIT)
            {
               PigeonServerGUI::shouldClose = true;
            }
         }
         PigeonServerGUI::StartImGuiFrame();
         
         PigeonServerGUI::MainWindow();

         PigeonServerGUI::RenderTabBar();

         PigeonServerGUI::Render();
      }

      PigeonServerGUI::Cleanup();
   }
   delete logger;
   return 0;
}