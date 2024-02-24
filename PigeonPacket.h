#pragma once

#include <chrono>
#include <vector>
#include <iostream>
#include <string>

#define MAX_USERNAME 20
#define MAX_HEADER 38

enum PIGEON_OPCODE{

    //HANDSHAKE
    CLIENT_HELLO = 0x00,
    SERVER_HELLO = 0x01,

    //MESSAGES
    TEXT_MESSAGE = 0x0A,
    MEDIA_FILE = 0x0B,

    //PRESENCE
    PRESENCE_REQUEST = 0x0C,

    //HEARBEAT
    HEARTBEAT = 0xBB,

    //CLOSING EVENTS
    CLIENT_DISCONNECT = 0xFE,

    //ERRORS
    PROTOCOL_MISMATCH = 0xEE,
    JSON_NOT_VALID = 0xE0,

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

