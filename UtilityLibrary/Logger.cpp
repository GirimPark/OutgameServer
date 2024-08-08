#include "Logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
{
    m_logFile.open(GetLogFileName(), std::ios::out | std::ios::app);
    if (!m_logFile.is_open()) 
    {
        std::cerr << "Failed to open log file!" << std::endl;
    }
}

Logger::~Logger()
{
    if (m_logFile.is_open()) 
    {
        m_logFile.close();
    }
}

void Logger::Log(LogLevel level, const std::string& message, const char* file, int line)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFile << "[" << GetTimestamp() << "] [" << LogLevelToString(level) << "] "
        << message << " (" << file << ":" << line << ")" << std::endl;
}

std::string Logger::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt = {};
    localtime_s(&bt, &in_time_t);
    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %X");
    return ss.str();
}

std::string Logger::LogLevelToString(LogLevel level)
{
    switch (level)
	{
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARNING:
        return "WARNING";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

std::string Logger::GetLogFileName()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt = {};
    localtime_s(&bt, &in_time_t);
    std::stringstream ss;
    ss << "./Log/log_" << std::put_time(&bt, "%Y%m%d_%H%M%S") << ".log";
    return ss.str();
}
