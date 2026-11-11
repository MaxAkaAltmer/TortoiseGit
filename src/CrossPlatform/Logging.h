#pragma once

#include <string>
#include <mutex>
#include <memory>
#include <source_location>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// Cross-platform logging singleton
// Note: LogLevel is nested in the class - callers use CLogging::LogLevel::Xxx
class CLogging
{
public:
    // Logging levels matching Windows DebugOutput severity
    enum class LogLevel
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Fatal = 4
    };

    // Initialize logging (call once at startup)
    static void Init(const std::string& appName = "TortoiseGit",
                    const std::string& logDir = "");

    // Log a message
    static void LogMessage(LogLevel level, const std::string& message);
    static void LogFormat(LogLevel level, const char* fmt, ...);

    // Set/get minimum log level
    static void SetMinLevel(LogLevel level);
    static LogLevel GetMinLevel();

    // Shutdown (optional)
    static void Shutdown();

    // Check if logging is initialized
    static bool IsInitialized() { return s_initialized; }

    // Thread-safe log entry
    static void LogEntry(LogLevel level, const std::string& message,
                       const char* file = "", int line = 0,
                       const char* func = "");

    // Stream-based logging for complex output
    static std::ostream& GetLogStream(LogLevel level);

private:
    static std::once_flag s_initFlag;
    static std::mutex s_logMutex;
    static std::string s_logFilePath;
    static bool s_initialized;
    static LogLevel s_minLevel;
};

// Helper macro for convenient logging
#define LOG_DEBUG(msg)   CLogging::LogEntry(CLogging::LogLevel::Debug, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(msg)    CLogging::LogEntry(CLogging::LogLevel::Info, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN(msg)    CLogging::LogEntry(CLogging::LogLevel::Warning, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(msg)   CLogging::LogEntry(CLogging::LogLevel::Error, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL(msg)   CLogging::LogEntry(CLogging::LogLevel::Fatal, msg, __FILE__, __LINE__, __FUNCTION__)

// Stream-style logging
#define LOG_STREAM(level) CLogging::GetLogStream(level)

// Windows DEBUG equivalent
#ifdef _WIN32
inline void DebugOutput(const char* message)
{
    if (CLogging::IsInitialized())
        CLogging::LogMessage(CLogging::LogLevel::Debug, message);
    else
        OutputDebugStringA(message);
}

inline void DebugOutputFormat(const char* fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DebugOutput(buffer);
}
#endif
