#pragma once

#include "../PigeonServer.h"

#include <iostream>
#include <SDL2/SDL.h>
#include "../ImGui/imgui_impl_sdl2.h"
#include "../ImGui/imgui_impl_opengl3.h"
#include "../ImGui/implot.h"
#include "../ImGui/imgui_stdlib.h"
#include <GL/glew.h>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <sys/resource.h>
#include <vector>
#include <thread>


//Took it from https://github.com/epezent/implot/blob/master/implot_demo.cpp
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 10000) {
        MaxSize = max_size;
        Offset  = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x,y));
        else {
            Data[Offset] = ImVec2(x,y);
            Offset =  (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset  = 0;
        }
    }
};



namespace PigeonServerGUI{

     SDL_Window* sdlWindow = nullptr;
     SDL_GLContext glContext = NULL;
     SDL_Event currentEvent;
     bool shouldClose = false;
     bool shouldDelete = false;

    static bool shouldLog = false;

    ImGuiLog* log = nullptr;


     int windowWidth = 1280;
     int windowHeight = 720;
    
     PigeonServer* server = nullptr;
     static std::string status = "Server is not running!";

    int CreateWindow(){

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL  | SDL_WINDOW_ALLOW_HIGHDPI);
        
        sdlWindow = SDL_CreateWindow("Pigeon Server Control Panel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, window_flags);
        
        glContext = SDL_GL_CreateContext(sdlWindow);
        if(SDL_GL_MakeCurrent(sdlWindow, glContext) != 0){
            std::cerr << "Error setting up OpenGL context " << std::endl;
            return -1;
        };

        if (glewInit() != GLEW_OK) {
            std::cout << "Error initializing glew" << std::endl;
        }

        SDL_GL_SetSwapInterval(1); 
        return 0;
    }

    void PlotSent(double* sent){
    
        struct rusage usage;
        static double t = 0;

        static ScrollingBuffer buffer;

        t += ImGui::GetIO().DeltaTime;

        buffer.AddPoint(t,(*sent/1000));

        if (ImPlot::BeginPlot("Bytes Sent",ImVec2(-1,175))) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::SetupAxisLimits(ImAxis_X1,t-30, t, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1,-1,2000);

            ImPlot::PlotLine("KB", &buffer.Data[0].x, &buffer.Data[0].y, buffer.Data.size(), 0, buffer.Offset, 2 * sizeof(int));

            // End plot
            ImPlot::EndPlot();
        }
        *sent = 0;
        
    }

    void PlotData(double* data){
    
        struct rusage usage;
        static double t = 0;

        static ScrollingBuffer buffer;

        t += ImGui::GetIO().DeltaTime;

        buffer.AddPoint(t,(*data/1000));

        if (ImPlot::BeginPlot("Bytes Recv",ImVec2(-1,175))) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::SetupAxisLimits(ImAxis_X1,t-30, t, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1,-1,2000);

            ImPlot::PlotLine("KB", &buffer.Data[0].x, &buffer.Data[0].y, buffer.Data.size(), 0, buffer.Offset, 2 * sizeof(float));

            // End plot
            ImPlot::EndPlot();
        }
    *data = 0;

        
    }

    void PlotFps(){
    
        struct rusage usage;
        static double t = 0;

        static ScrollingBuffer buffer;
        static ScrollingBuffer bufferMem;

        t += ImGui::GetIO().DeltaTime;

        float memory_usage_gb = -1;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            memory_usage_gb = static_cast<float>(usage.ru_maxrss) / (1024);
        }

        buffer.AddPoint(t, ImGui::GetIO().Framerate);
        bufferMem.AddPoint(t,memory_usage_gb);

        // Begin plot
        if (ImPlot::BeginPlot("Framerate",ImVec2(-1,175))) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::SetupAxisLimits(ImAxis_X1,t-30, t, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1,30,90);

            ImPlot::PlotLine("FPS", &buffer.Data[0].x, &buffer.Data[0].y, buffer.Data.size(), 0, buffer.Offset, 2 * sizeof(float));
            ImPlot::PlotLine("RSS", &bufferMem.Data[0].x, &bufferMem.Data[0].y, bufferMem.Data.size(), 0, bufferMem.Offset, 2 * sizeof(float));

            // End plot
            ImPlot::EndPlot();
        }
        
        
    }

    void ServerCreation(){
        static std::string serverName = "PGN-EU-1";
        static std::string serverPort = "4444";
        static std::string certPath = "cert.pem";
        static std::string keyPath = "key.pem";

        ImGui::Text("Sever  Name: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##srvName", &serverName);
        
        ImGui::Text("Server Port: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##port", &serverPort);

        ImGui::Text("Certificate: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##certificate", &certPath);

        ImGui::Text("Private Key: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##pkey", &keyPath);

        


        if(ImGui::Button("Create Server")){
            if(server == nullptr){
                log = new ImGuiLog();
                server = new PigeonServer(certPath,keyPath,serverName, std::stoi(serverPort), log);
                std::thread([&]{
                    server->Run(shouldDelete);                    
                }).detach();
            }
        }

        ImGui::SameLine();

        if(ImGui::Button("Clear Logs")){
            if(log != nullptr){
                log->Clear();
            }
        }
    }

    void ServerInformation(){
        ImVec4 color;

        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

        if(server==nullptr){
            color = ImVec4(255,0,0,255);
            status = "Server is not running!";            
        }else{
            color = ImVec4(0,255,0,255);
            status = "Server is running!";
        }
            ImGui::TextColored(color,status.c_str());
        if(server!=nullptr){
            ImGui::BulletText(std::ctime(&currentTime));
            ImGui::BulletText("Clients connected: ");
            ImGui::SameLine();
            ImGui::Text((std::to_string((server->GetClients()->size()))).c_str());
            ImGui::Checkbox("Show Logs", &shouldLog);
        }
    }   

    void SetUpStyle(){
        ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
		colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
		colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(8.00f, 8.00f);
		style.FramePadding = ImVec2(5.00f, 2.00f);
		style.CellPadding = ImVec2(6.00f, 6.00f);
		style.ItemSpacing = ImVec2(6.00f, 6.00f);
		style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
		style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
		style.IndentSpacing = 25;
		style.ScrollbarSize = 5;
		style.GrabMinSize = 10;
		style.WindowBorderSize = 1;
		style.ChildBorderSize = 1;
		style.PopupBorderSize = 1;
		style.FrameBorderSize = 1;
		style.TabBorderSize = 1;
		style.WindowRounding = 7;
		style.ChildRounding = 4;
		style.FrameRounding = 3;
		style.PopupRounding = 4;
		style.ScrollbarRounding = 9;
		style.GrabRounding = 3;
		style.LogSliderDeadzone = 4;
		style.TabRounding = 4;
        style.ScaleAllSizes(2.5);
        ImGui::GetStyle().WindowRounding = 0.0f;
        ImGui::GetStyle().ChildRounding = 0.0f;
        ImGui::GetStyle().FrameRounding = 0.0f;
        ImGui::GetStyle().GrabRounding = 0.0f;
        ImGui::GetStyle().PopupRounding = 0.0f;
        ImGui::GetStyle().ScrollbarRounding = 0.0f;
    }

    void SetUpImGui(){
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        //Docking creo que esta bug con SDL, con glfw si funciona
        //imIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        //imIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL2_InitForOpenGL(sdlWindow, glContext);
        ImGui_ImplOpenGL3_Init("#version 330 core");
        SetUpStyle();
    }

    void StartImGuiFrame(){
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //This is to make the first imgui window fill all the sdl window
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
        ImGui::SetNextWindowPos(ImVec2(0, 0));
    }

    void Render(){
        //End of the main window
        ImGui::End();
        ImGui::Render();

        glViewport(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

        glClearColor(0.10f, 0.10f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(sdlWindow);
    }

    void MainWindow(){
        ImGui::Begin("Pigeon Control Panel", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    }

    /*
    *  Creation of all child windows.
    *  
    * 
    */

    void RenderTabBar(){
        ImGui::BeginChild("srv creation",ImVec2(320, 220),true);

            ServerCreation();

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("serv info",ImVec2(250, 220),true);

            ServerInformation();
            
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("FPSw",ImVec2(635, 220),true);

            PlotFps();
            
        ImGui::EndChild();


        ImGui::BeginChild("serv log",ImVec2(585, 425),true);
            if(server!=nullptr && shouldLog){
                log->Draw("Log");
            }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("plots",ImVec2(635, 425),true);
        if(server!=nullptr && server->bytesRecv != nullptr && server->bytesSent != nullptr){
            PlotData(server->bytesRecv);
            PlotSent(server->bytesSent);
        }
        ImGui::EndChild();

    }
    

    void Cleanup(){
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        
        ImGui::DestroyContext();
        ImPlot::DestroyContext();

        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
    }

}