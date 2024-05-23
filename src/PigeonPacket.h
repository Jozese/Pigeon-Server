#pragma once

#include <chrono>
#include <vector>
#include <iostream>
#include <string>

#define MAX_USERNAME 20
#define MAX_HEADER 38

enum PIGEON_OPCODE
{

    // HANDSHAKE
    CLIENT_HELLO = 0x01,
    SERVER_HELLO = 0x02,

    // MESSAGE OPCODES, THOSE ARE BROADCASTABLE
    TEXT_MESSAGE = 0x10,
    MEDIA_FILE = 0x11,
    MEDIA_DOWNLOAD = 0x12,
    ACK_MEDIA_DOWNLOAD = 0x13,

    // PRESENCE, ALSO BROADCASTABLE
    PRESENCE_REQUEST = 0x20,
    PRESENCE_UPDATE = 0x22,

    // CLOSING EVENTS (not used but should be used)
    CLIENT_DISCONNECT = 0xF0,

    // ERROR OPCODES, CONNECTION IS CLOSED
    JSON_NOT_VALID = 0xE0,
    USER_COLLISION = 0xE1,
    PROTOCOL_MISMATCH = 0xE2,
    LENGTH_EXCEEDED = 0xE3,
    USERNAME_MISMATCH = 0xE4,
    RATE_LIMITED = 0xE5,
    FILE_NOT_FOUND = 0xE6,


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
