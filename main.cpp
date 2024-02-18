#include <iostream>
#include <SDL2/SDL.h>
#include "ImGui/imgui_impl_sdl2.h"
#include "ImGui/imgui_impl_opengl3.h"
#include <GL/glew.h>

#include "PigeonServerGUI/PigeonServerGUI.h"

int main(int argc, char* argv[])
{
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
   return 0;
}