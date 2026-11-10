#include "Logging.h"
#include "Path.h"

#include <iostream>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <cstdarg>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <atlstr.h>
#else
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#endif

// Static member initialization
std::once_flag CLogging::s_initFlag;
std::mutex CLogging::s_logMutex;
std::string CLogging::s_logFilePath;
bool CLogging::s_initialized = false;
CLogging::LogLevel CLogging::s_minLevel = CLogging::LogLevel::Debug;

void CLogging::Init(const std::string& appName, const std::string& logDir)
{
    std::call_once(s_initFlag, [&]() {
        s_initialized = true;

        // Determine log directory
        std::string logPath;
        if (!logDir.empty())
        {
            logPath = logDir;
        }
        else
        {
#ifdef _WIN32
            // Windows: AppData\TortoiseGit\logs
            wchar_t buf[MAX_PATH];
            if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            {
                logPath = CStringA(buf).GetString();
            }
            else
            {
                logPath = "C:\\Temp";
            }
#else
            // Linux: XDG_DATA_HOME/TortoiseGit/logs
            const char* xdg = getenv("XDG_DATA_HOME");
            if (xdg && *xdg)
            {
                logPath = xdg;
            }
            else
            {
                const char* home = getenv("HOME");
                if (home && *home)
                    logPath = home;
                else
                    logPath = "/tmp";
            }
#endif
        }

        // Create log subdirectory
        CPath::MakeSureDirectoryPathExists(logPath);
        s_logFilePath = logPath + "/TortoiseGit/logs/debug.log";

        // Create log directory if needed
        CPath::MakeSureDirectoryPathExists(CPath::GetParentPath(s_logFilePath));

        LogMessage(LogLevel::Info, "Logging initialized");
    });
}

void CLogging::LogMessage(LogLevel level, const std::string& message)
{
    if (!s_initialized || level < s_minLevel)
        return;

    // Format timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << "[";
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    oss << "] [TID:" << std::this_thread::get_id() << "]";

    const char* levelStr = "UNKNOWN";
    switch (level)
    {
        case LogLevel::Debug:   levelStr = "DEBUG"; break;
        case LogLevel::Info:    levelStr = "INFO";  break;
        case LogLevel::Warning: levelStr = "WARN";  break;
        case LogLevel::Error:   levelStr = "ERROR"; break;
        case LogLevel::Fatal:   levelStr = "FATAL"; break;
    }
    oss << " [" << levelStr << "] " << message;

    std::string formatted = oss.str();

    // Output to console (stderr)
    {
        std::lock_guard<std::mutex> lock(s_logMutex);
        std::cerr << formatted << std::endl;

        // Also write to file
        if (!s_logFilePath.empty())
        {
            std::ofstream logFile(s_logFilePath, std::ios::app);
            if (logFile.is_open())
                logFile << formatted << std::endl;
        }
    }

    // On Windows, also send to OutputDebugString
#ifdef _WIN32
    if (level >= LogLevel::Warning)
    {
        OutputDebugStringA(formatted.c_str());
    }
#endif
}

void CLogging::LogFormat(LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[4096];
#ifdef _WIN32
    vsnprintf_s(buffer, sizeof(buffer), fmt, args);
#else
    vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif

    va_end(args);
    LogMessage(level, std::string(buffer));
}

void CLogging::SetMinLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_minLevel = level;
}

CLogging::LogLevel CLogging::GetMinLevel()
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    return s_minLevel;
}

void CLogging::Shutdown()
{
    LogMessage(LogLevel::Info, "Logging shutting down");
    s_initialized = false;
}
