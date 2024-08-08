#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

class Logger {
public:
    enum class LogLevel {
        INFO,
        WARNING,
        ERROR
    };

    static Logger& Instance();

    void Log(LogLevel level, const std::string& message, const char* file, int line);

private:
    Logger();
    ~Logger();

    std::string GetTimestamp();
    std::string LogLevelToString(LogLevel level);
    std::string GetLogFileName();

    std::ofstream m_logFile;
    std::mutex m_mutex;
};

// Macro for easy logging
#define LOG_INFO(msg) Logger::Instance().Log(Logger::LogLevel::INFO, msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::Instance().Log(Logger::LogLevel::WARNING, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::Instance().Log(Logger::LogLevel::ERROR, msg, __FILE__, __LINE__)
