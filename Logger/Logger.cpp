#include "Logger.h"

Logger::Logger() {
    logThread = std::thread(&Logger::runLogger, this);
}

std::string Logger::GetTime() {
    time_t now = time(0);
    std::string time = ctime(&now);
    time[time.length() - 1] = '\0';
    return time;
}

void Logger::log(LogTypes type, std::string&& logMessage) {

    std::unique_lock<std::mutex> lock(logMutex);
    logQueue.push(Log{type , logMessage});
    lock.unlock();
    queueCv.notify_all();
}

void Logger::runLogger() {
    while(isRunning){
        std::unique_lock<std::mutex> lock(logMutex);
        while(logQueue.empty()){
            queueCv.wait(lock); // lock is unlocked or cv is notified.
        }
        // FIFO
        Log log = logQueue.front();
        logQueue.pop();

        switch (log.type) {
            case INFO:
                std::cout << GetTime() << "[INFO] " << log.logMessage << std::endl;
                break;
            case DEBUG:
                std::cout << GetTime() << "[DEBUG] " << log.logMessage << std::endl;
                break;
            case WARNING:
                std::cout << GetTime() << "[WARNING] " << log.logMessage << std::endl;
                break;
            case ERROR:
                std::cerr << GetTime() << "[ERROR] " << log.logMessage << std::endl;
                break;
        }

    }

}

Logger::~Logger(){
    isRunning = false;
    if(logThread.joinable())
        logThread.join();
}