#include "FileSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>
#include <glob.h>
#endif

namespace
{
    // std::filesystem::file_time_type::clock is not guaranteed to be
    // system_clock (and does not provide to_time_t/from_time_t), so convert
    // between the two clocks via their current offset.
    std::time_t FileTimeToTimeT(const std::filesystem::file_time_type& ftime)
    {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        return std::chrono::system_clock::to_time_t(sctp);
    }

    std::filesystem::file_time_type TimeTToFileTime(std::time_t timestamp)
    {
        auto sctp = std::chrono::time_point<std::chrono::system_clock>(std::chrono::seconds(timestamp));
        return std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
            sctp - std::chrono::system_clock::now() + std::filesystem::file_time_type::clock::now());
    }

    // Wildcard match supporting '*' and '?', ASCII case-insensitive to keep
    // the semantics of the Win32 PathMatchSpec() used before porting.
    bool WildcardMatch(const std::string& text, const std::string& pattern)
    {
        auto lowerAscii = [](char c) {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        };
        size_t t = 0, p = 0;
        size_t starPos = std::string::npos, starTextPos = 0;
        while (t < text.size())
        {
            if (p < pattern.size() && (pattern[p] == '?' || lowerAscii(pattern[p]) == lowerAscii(text[t])))
            {
                ++t;
                ++p;
            }
            else if (p < pattern.size() && pattern[p] == '*')
            {
                starPos = p++;
                starTextPos = t;
            }
            else if (starPos != std::string::npos)
            {
                // backtrack: let the last '*' consume one more character
                p = starPos + 1;
                t = ++starTextPos;
            }
            else
                return false;
        }
        while (p < pattern.size() && pattern[p] == '*')
            ++p;
        return p == pattern.size();
    }
}

namespace CFileSystem
{
    // === File/Directory Existence ===
    bool FileExists(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    }

    bool DirectoryExists(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }

    bool PathExists(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path);
    }

    // === Directory Operations ===
    bool CreateDirectory(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::create_directory(path, ec);
    }

    bool MakeSureDirectoryPathExists(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        return !ec;
    }

    bool RemoveDirectory(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::remove_all(path, ec) > 0;
    }

    bool RemoveFile(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::remove(path, ec);
    }

    bool CopyFile(const std::filesystem::path& src, const std::filesystem::path& dst, bool overwrite)
    {
        std::error_code ec;
        if (overwrite)
            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        else
            std::filesystem::copy(src, dst, std::filesystem::copy_options::skip_existing, ec);
        return !ec;
    }

    bool MoveFile(const std::filesystem::path& src, const std::filesystem::path& dst)
    {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        return !ec;
    }

    bool Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
    {
        return MoveFile(oldPath, newPath);
    }

    // === Path Operations ===
    std::filesystem::path GetFullPath(const std::filesystem::path& path)
    {
        if (path.is_absolute())
            return path.lexically_normal();
        return std::filesystem::current_path() / path;
    }

    std::filesystem::path GetFileName(const std::filesystem::path& path)
    {
        return path.filename();
    }

    std::filesystem::path GetFileExtension(const std::filesystem::path& path)
    {
        return path.extension();
    }

    std::filesystem::path GetFileRoot(const std::filesystem::path& path)
    {
        return path.stem();
    }

    std::filesystem::path GetParentPath(const std::filesystem::path& path)
    {
        return path.parent_path();
    }

    std::filesystem::path GetTempDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (GetTempPathW(MAX_PATH, buf))
            return buf;
        return std::filesystem::temp_directory_path();
#else
        const char* temp = getenv("TMPDIR");
        if (temp && *temp)
            return temp;
        return "/tmp";
#endif
    }

    std::string GetCurrentDirectory()
    {
        return std::filesystem::current_path().string();
    }

    std::filesystem::path CombinePaths(const std::filesystem::path& base, const std::filesystem::path& relative)
    {
        return (base / relative).lexically_normal();
    }

    // === File Operations ===
    bool ReadFileToString(const std::filesystem::path& path, std::string& content)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;
        content.assign(std::istreambuf_iterator<char>(file), {});
        return true;
    }

    bool WriteStringToFile(const std::filesystem::path& path, const std::string& content, bool append)
    {
        std::ofstream file(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
        if (!file.is_open())
            return false;
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file.good();
    }

    bool ReadFileToWString(const std::filesystem::path& path, std::wstring& content)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;
        std::string str;
        if (!ReadFileToString(path, str))
            return false;
        content.assign(str.begin(), str.end());
        return true;
    }

    bool WriteWStringToFile(const std::filesystem::path& path, const std::wstring& content, bool append)
    {
        std::string utf8(content.begin(), content.end());
        return WriteStringToFile(path, utf8, append);
    }

    bool GetFileSize(const std::filesystem::path& path, std::uintmax_t& size)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path))
            return false;
        size = std::filesystem::file_size(path, ec);
        return !ec;
    }

    bool GetFileTimestamp(const std::filesystem::path& path, std::time_t& timestamp)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path))
            return false;
        auto ftime = std::filesystem::last_write_time(path, ec);
        if (ec)
            return false;
        timestamp = FileTimeToTimeT(ftime);
        return true;
    }

    bool SetFileTimestamp(const std::filesystem::path& path, std::time_t timestamp)
    {
        std::error_code ec;
        auto ftime = TimeTToFileTime(timestamp);
        std::filesystem::last_write_time(path, ftime, ec);
        return !ec;
    }

    // === Directory Enumeration ===
    bool EnumerateDirectory(const std::filesystem::path& dirPath, DirCallback callback, bool recursive)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(dirPath, ec))
            return false;

        for (auto& entry : std::filesystem::directory_iterator(dirPath, ec))
        {
            if (callback(entry, 0))
                return true;
            if (recursive && entry.is_directory())
                EnumerateDirectory(entry.path(), callback, true);
        }
        return true;
    }

    std::vector<std::filesystem::path> GetFilesInDirectory(const std::filesystem::path& dirPath, const std::wstring& pattern)
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!std::filesystem::is_directory(dirPath, ec))
            return result;
        // filesystem::path performs the platform-specific wstring -> UTF-8 conversion
        const std::string utf8Pattern = pattern.empty() ? std::string("*")
                                                        : std::filesystem::path(pattern).string();
        for (auto& entry : std::filesystem::directory_iterator(dirPath, ec))
        {
            if (!entry.is_regular_file())
                continue;
            if (WildcardMatch(entry.path().filename().string(), utf8Pattern))
                result.push_back(entry.path());
        }
        return result;
    }

    std::vector<std::filesystem::path> GetDirectoriesInDirectory(const std::filesystem::path& dirPath)
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!std::filesystem::is_directory(dirPath, ec))
            return result;
        for (auto& entry : std::filesystem::directory_iterator(dirPath, ec))
        {
            if (entry.is_directory())
                result.push_back(entry.path());
        }
        return result;
    }

    // === Path Normalization ===
    std::filesystem::path NormalizePath(const std::filesystem::path& path)
    {
        return path.lexically_normal();
    }

    bool IsSamePath(const std::filesystem::path& a, const std::filesystem::path& b)
    {
        auto pa = a.lexically_normal();
        auto pb = b.lexically_normal();
#ifdef _WIN32
        std::string la = pa.string();
        std::string lb = pb.string();
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la == lb;
#else
        return pa == pb;
#endif
    }

    bool IsPathChild(const std::filesystem::path& child, const std::filesystem::path& parent)
    {
        auto pc = child.lexically_normal();
        auto pp = parent.lexically_normal();
        auto rel = std::filesystem::relative(pc, pp);
        return !rel.empty() && rel.begin()->string() != "..";
    }

    bool IsPathAncestor(const std::filesystem::path& ancestor, const std::filesystem::path& descendant)
    {
        return IsPathChild(descendant, ancestor);
    }

    // === Special Directories ===
    std::filesystem::path GetHomeDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            return buf;
        return "C:\\Users\\Default";
#else
        const char* home = getenv("HOME");
        if (home && *home)
            return home;
        struct passwd* pw = getpwuid(getuid());
        if (pw)
            return pw->pw_dir;
        return "/home";
#endif
    }

    std::filesystem::path GetAppDataDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            return buf;
        return "C:\\Users\\Default\\AppData\\Roaming";
#else
        const char* xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && *xdg)
            return xdg;
        return GetHomeDirectory() / ".config";
#endif
    }

    std::filesystem::path GetLocalAppDataDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            return buf;
        return "C:\\Users\\Default\\AppData\\Local";
#else
        const char* xdg = getenv("XDG_DATA_HOME");
        if (xdg && *xdg)
            return xdg;
        return GetHomeDirectory() / ".local" / "share";
#endif
    }

    std::filesystem::path GetTempFile()
    {
        return GetTempDirectory() / "tgit_temp";
    }

    std::filesystem::path CreateTempFile(const std::string& prefix, const std::string& suffix)
    {
        auto tmpDir = GetTempDirectory();
        std::string template_ = tmpDir.string() + "/" + prefix + "XXXXXX" + suffix;
#ifdef _WIN32
        wchar_t wtemp[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, template_.c_str(), -1, wtemp, MAX_PATH);
        if (_wmktemp_s(wtemp))
            return tmpDir / (prefix + "1.tmp" + suffix);
        char result[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wtemp, -1, result, MAX_PATH, nullptr, nullptr);
        return result;
#else
        int fd = mkstemps(&template_[0], static_cast<int>(suffix.size()));
        if (fd != -1)
        {
            close(fd);
            return template_;
        }
        return tmpDir / (prefix + "1.tmp" + suffix);
#endif
    }

    std::string GetTempFileName()
    {
        return CreateTempFile("tgit_", ".tmp").filename().string();
    }

    // === File Attributes ===
    bool IsReadOnly(const std::filesystem::path& path)
    {
        std::error_code ec;
        auto perms = std::filesystem::status(path, ec).permissions();
        return (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none &&
               (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none;
    }

    bool SetReadOnly(const std::filesystem::path& path, bool readOnly)
    {
        std::error_code ec;
        if (readOnly)
            std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace, ec);
        else
            std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
        return !ec;
    }

    bool IsHidden(const std::filesystem::path& path)
    {
#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(path.wstring().c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_HIDDEN);
#else
        std::string filename = path.filename().string();
        return !filename.empty() && filename[0] == '.';
#endif
    }

    bool IsExecutable(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec))
            return false;
#ifdef _WIN32
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".exe" || ext == ".bat" || ext == ".cmd";
#else
        auto perms = std::filesystem::status(path, ec).permissions();
        return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
#endif
    }

    // === Symbolic Links ===
    std::filesystem::path ReadLink(const std::filesystem::path& path)
    {
        std::error_code ec;
        auto target = std::filesystem::read_symlink(path, ec);
        if (ec)
            return "";
        return target;
    }

    bool CreateSymlink(const std::filesystem::path& target, const std::filesystem::path& symlink)
    {
        std::error_code ec;
        std::filesystem::create_symlink(target, symlink, ec);
        return !ec;
    }

    bool SymlinkExists(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::is_symlink(path, ec);
    }

    // === CFileWatcher ===
    CFileWatcher::CFileWatcher() : m_pollInterval(500)
    {
    }

    CFileWatcher::~CFileWatcher()
    {
        StopWatching();
    }

    bool CFileWatcher::StartWatching(const std::filesystem::path& dirPath, ChangeCallback callback, DWORD pollIntervalMs)
    {
        m_watchDir = dirPath;
        m_callback = callback;
        m_pollInterval = pollIntervalMs;
        m_running = true;
        m_thread = std::thread(&CFileWatcher::WatcherThreadFunc, this);
        return true;
    }

    void CFileWatcher::StopWatching()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running = false;
        }
        if (m_thread.joinable())
            m_thread.join();
    }

    bool CFileWatcher::IsWatching() const
    {
        return m_running;
    }

    void CFileWatcher::WatcherThreadFunc()
    {
        while (m_running)
        {
            std::map<std::filesystem::path, std::time_t> currentState;
            std::error_code ec;
            if (std::filesystem::is_directory(m_watchDir))
            {
                for (auto& entry : std::filesystem::directory_iterator(m_watchDir, ec))
                {
                    if (entry.is_regular_file())
                    {
                        auto ts = std::filesystem::last_write_time(entry.path(), ec);
                        currentState[entry.path()] = FileTimeToTimeT(ts);
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& [path, ts] : currentState)
                {
                    auto it = m_lastState.find(path);
                    if (it == m_lastState.end())
                    {
                        if (m_callback) m_callback(path, Added);
                    }
                    else if (it->second != ts)
                    {
                        if (m_callback) m_callback(path, Modified);
                        it->second = ts;
                    }
                }
                for (auto it = m_lastState.begin(); it != m_lastState.end(); )
                {
                    if (currentState.find(it->first) == currentState.end())
                    {
                        if (m_callback) m_callback(it->first, Removed);
                        it = m_lastState.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                m_lastState = currentState;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(m_pollInterval));
        }
    }
}
