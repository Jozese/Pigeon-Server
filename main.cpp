#include <iostream>
#include <SDL2/SDL.h>
#include "ImGui/imgui_impl_sdl2.h"
#include "ImGui/imgui_impl_opengl3.h"
#include <GL/glew.h>

#include "PigeonServerGUI/PigeonServerGUI.h"

int main(int argc, char *argv[])
{
   system("clear");

   Logger *logger = new Logger();

   // fix
   bool shouldDelete = false;

   if (strcmp(argv[1], "--headless") == 0)
   {
      Json::Reader reader;
      Json::Value value;

      auto file = File::DiskToBuffer("config.json");
      if(file.empty()){
         logger->log(ERROR, "config.json does not exist");
         return EXIT_FAILURE;
      }

      if (!reader.parse(std::string(file.begin(), file.end()), value))      {
         logger->log(ERROR, "config.json contains invalid json");
         return EXIT_FAILURE;
      }

      static std::string serverName = value["servername"].asString();
      static std::string serverPort = value["port"].asString();
      static std::string certPath = value["cert"].asString();
      static std::string keyPath = value["key"].asString();

      if(serverName.empty()){
         logger->log(ERROR, "servername cannot be empty");
         return EXIT_FAILURE;
      }

      // No need to check other vars since the tcp server wont start if any value is not correct

      PigeonServer *server = new PigeonServer(certPath, keyPath, serverName, std::stoi(serverPort), nullptr, logger);
      server->Run(shouldDelete);
      delete server;
   }
   else
   {

      PigeonServerGUI::logger = logger;

      if (PigeonServerGUI::CreateWindow() != 0)
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