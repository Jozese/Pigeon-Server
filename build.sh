#!/bin/bash
g++ -o ./bin/server.out TcpServer/*.h TcpServer/*.cpp Logger/*.h Logger/*.cpp PigeonPacket.h PigeonServer.h PigeonServer.cpp main.cpp -lssl -lcrypto -std=c++17
