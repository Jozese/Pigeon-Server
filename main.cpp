#include <iostream>
#include <SDL2/SDL.h>
#include "ImGui/imgui_impl_sdl2.h"
#include "ImGui/imgui_impl_opengl3.h"
#include <GL/glew.h>
#include "PigeonData.h"

#include "PigeonServerGUI/PigeonServerGUI.h"

int main(int argc, char *argv[])
{
   system("clear");

   Logger *logger = new Logger();

   // fix
   bool shouldDelete = false;

   if (strcmp(argv[1], "--headless") == 0)
   {
            
      PigeonData data("config.json");

      PigeonServer *server = new PigeonServer(data, nullptr, logger);
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