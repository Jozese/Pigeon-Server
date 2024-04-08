//
// Created by Jozese on 3/2/2023.
//

#pragma once
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <sstream>

#include "Colors.h"

enum LogTypes{
    INFO,
    DEBUG,
    WARNING,
    ERROR,

};

class Logger {
public:
    Logger();
    ~Logger();
    void runLogger();
    void log(LogTypes type, std::string&& logMessage);
    std::string GetTime();
private:
    struct Log{
        LogTypes type;
        std::string logMessage;
    };

    bool isRunning = true;
    std::thread logThread;
    std::mutex logMutex;
    std::queue<Log> logQueue;
    std::condition_variable queueCv;





};