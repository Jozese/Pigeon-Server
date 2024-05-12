#!/bin/bash
g++ -o ./bin/server.out TcpServer/*.cpp Logger/Logger/*.cpp src/PigeonData.cpp src/PigeonServer.cpp src/main.cpp -lssl -ljsoncpp -lcrypto -std=c++20
