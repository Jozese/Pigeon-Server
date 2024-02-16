#!/bin/bash
g++ -o ./bin/server.out TcpServer/*.cpp Logger/*.cpp PigeonServer.cpp main.cpp -lssl -lcrypto -std=c++17
