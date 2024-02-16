#pragma once
#include <chrono>
#include <vector>
#include <iostream>
#include <string>

#define MAX_USERNAME 20
#define MAX_HEADER 38

enum PIGEON_OPCODE{
    CLIENT_HELLO = 0xFF
};

struct PigeonHeader
{
    int HEADER_LENGTH;
    std::time_t TIME_STAMP;
    std::string username;
    PIGEON_OPCODE OPCODE;
    int CONTENT_LENGTH;
};


struct PigeonPacket
{
    
    PigeonHeader HEADER;
    std::vector<unsigned char> PAYLOAD;
};

