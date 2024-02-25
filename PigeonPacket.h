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
    TEXT_MESSAGE = 0x10,
    MEDIA_FILE = 0x11,

    //PRESENCE
    PRESENCE_REQUEST = 0x20,

    //HEARBEAT
    HEARTBEAT = 0x30,

    //CLOSING EVENTS
    CLIENT_DISCONNECT = 0xF0,

    //ERRORS
    JSON_NOT_VALID = 0xE0,
    USER_COLLISION = 0xE1,
    PROTOCOL_MISMATCH = 0xE2,
    LENGTH_EXCEEDED = 0xE3,


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

