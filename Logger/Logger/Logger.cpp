

#include "Logger.h"

Logger::Logger()
{
    logThread = std::thread(&Logger::runLogger, this);
}

std::string Logger::GetTime()
{
    time_t now = time(0);
    std::string time = ctime(&now);
    time[time.length() - 1] = '\0';
    return time;
}

void Logger::log(LogTypes type, std::string &&logMessage)
{

    std::unique_lock<std::mutex> lock(logMutex);
    logQueue.push(Log{type, logMessage});
    lock.unlock();
    queueCv.notify_all();
}

void Logger::runLogger()
{
    while (isRunning)
    {
        std::unique_lock<std::mutex> lock(logMutex);
        while (logQueue.empty())
        {
            queueCv.wait(lock); // lock is unlocked or cv is notified.
        }
        // FIFO
        Log log = logQueue.front();
        logQueue.pop();

        std::stringstream ss;

        switch (log.type)
        {
        case INFO:
            ss << GetTime() << " [INFO] "
               << " " << log.logMessage;
            std::cout << Bold(Color(ss.str(), KCYN, "")) << std::endl;
            break;

        case DEBUG:
            ss << GetTime() << " [DEBUG] " << log.logMessage;
            std::cout << Bold(Color(ss.str(), KMAG, "")) << std::endl;
            break;

        case DEBUG_DOWNLOAD:
            ss << GetTime() << " [FILE] " << log.logMessage;
            std::cout << Bold(Color(ss.str(), KMAG, "")) << "\r";
            break;

        case WARNING:
            ss << GetTime() << " [WARN] "
               << " " << log.logMessage;
            std::cout << Bold(Color(ss.str(), KYEL, "")) << std::endl;
            break;

        case ERROR:
            ss << GetTime() << " [ERROR] " << log.logMessage;
            std::cout << Bold(Color(ss.str(), KRED, "")) << std::endl;
            break;
        }
    }
}

Logger::~Logger()
{
    isRunning = false;
    logThread.join();
}