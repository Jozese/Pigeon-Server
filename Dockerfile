FROM ubuntu:latest AS build

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libjsoncpp-dev \
    net-tools \
    sudo
WORKDIR /Pigeon-Server
COPY . .
RUN g++ -o ./bin/server.out TcpServer/*.cpp Logger/Logger/*.cpp src/PigeonData.cpp src/PigeonServer.cpp src/main.cpp -lssl -ljsoncpp -lcrypto -std=c++20
RUN mkdir -p bin/Files
