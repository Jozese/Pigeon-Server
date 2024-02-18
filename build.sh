#!/bin/bash
g++ -o ./bin/server.out -I/usr/include/SDL2 ImGui/*.cpp TcpServer/*.cpp Logger/*.cpp PigeonServer.cpp main.cpp -lssl -lcrypto -lSDL2 -lGLEW -lGL -ldl -std=c++17
