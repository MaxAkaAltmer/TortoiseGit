// TortoiseGit - a Windows shell extension for easy version control
// Copyright (C) 2025 - TortoiseGit
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
#include <map>
#include <system_error>

using DWORD = unsigned long;

namespace CFileSystem
{
    // File/directory existence checks
    bool FileExists(const std::filesystem::path& path);
    bool DirectoryExists(const std::filesystem::path& path);
    bool PathExists(const std::filesystem::path& path);

    // Directory operations
    bool CreateDirectory(const std::filesystem::path& path);
    bool MakeSureDirectoryPathExists(const std::filesystem::path& path);
    bool RemoveDirectory(const std::filesystem::path& path);
    bool RemoveFile(const std::filesystem::path& path);
    bool CopyFile(const std::filesystem::path& src, const std::filesystem::path& dst, bool overwrite = false);
    bool MoveFile(const std::filesystem::path& src, const std::filesystem::path& dst);
    bool Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);

    // Path operations
    std::filesystem::path GetFullPath(const std::filesystem::path& path);
    std::filesystem::path GetFileName(const std::filesystem::path& path);
    std::filesystem::path GetFileExtension(const std::filesystem::path& path);
    std::filesystem::path GetFileRoot(const std::filesystem::path& path);
    std::filesystem::path GetParentPath(const std::filesystem::path& path);
    std::filesystem::path GetTempDirectory();
    std::string GetCurrentDirectory();
    std::filesystem::path CombinePaths(const std::filesystem::path& base, const std::filesystem::path& relative);

    // File operations
    bool ReadFileToString(const std::filesystem::path& path, std::string& content);
    bool WriteStringToFile(const std::filesystem::path& path, const std::string& content, bool append = false);
    bool ReadFileToWString(const std::filesystem::path& path, std::wstring& content);
    bool WriteWStringToFile(const std::filesystem::path& path, const std::wstring& content, bool append = false);
    bool GetFileSize(const std::filesystem::path& path, std::uintmax_t& size);
    bool GetFileTimestamp(const std::filesystem::path& path, std::time_t& timestamp);
    bool SetFileTimestamp(const std::filesystem::path& path, std::time_t timestamp);

    // Directory enumeration
    using DirEntry = std::filesystem::directory_entry;
    using DirCallback = std::function<bool(const DirEntry& entry, int depth)>;
    bool EnumerateDirectory(const std::filesystem::path& dirPath, DirCallback callback, bool recursive = false);
    std::vector<std::filesystem::path> GetFilesInDirectory(const std::filesystem::path& dirPath, const std::wstring& pattern = L"*");
    std::vector<std::filesystem::path> GetDirectoriesInDirectory(const std::filesystem::path& dirPath);

    // Path normalization
    std::filesystem::path NormalizePath(const std::filesystem::path& path);
    bool IsSamePath(const std::filesystem::path& a, const std::filesystem::path& b);
    bool IsPathChild(const std::filesystem::path& child, const std::filesystem::path& parent);
    bool IsPathAncestor(const std::filesystem::path& ancestor, const std::filesystem::path& descendant);

    // Special directories
    std::filesystem::path GetHomeDirectory();
    std::filesystem::path GetAppDataDirectory();
    std::filesystem::path GetLocalAppDataDirectory();
    std::filesystem::path GetTempFile();
    std::filesystem::path CreateTempFile(const std::string& prefix = "tgit_", const std::string& suffix = ".tmp");
    std::string GetTempFileName();

    // File attributes
    bool IsReadOnly(const std::filesystem::path& path);
    bool SetReadOnly(const std::filesystem::path& path, bool readOnly);
    bool IsHidden(const std::filesystem::path& path);
    bool IsExecutable(const std::filesystem::path& path);

    // Symbolic link support
    std::filesystem::path ReadLink(const std::filesystem::path& path);
    bool CreateSymlink(const std::filesystem::path& target, const std::filesystem::path& symlink);
    bool SymlinkExists(const std::filesystem::path& path);

    // File watching (Linux: inotify-based, Windows: FindFirstChangeNotification)
    // Simple polling-based watcher (can be replaced with inotify on Linux)
    class CFileWatcher
    {
    public:
        using ChangeCallback = std::function<void(const std::filesystem::path& path, int changeType)>;
        enum ChangeType { Added, Removed, Modified, Renamed };

        CFileWatcher();
        ~CFileWatcher();
        CFileWatcher(const CFileWatcher&) = delete;
        CFileWatcher& operator=(const CFileWatcher&) = delete;

        bool StartWatching(const std::filesystem::path& dirPath, ChangeCallback callback, DWORD pollIntervalMs = 500);
        void StopWatching();
        bool IsWatching() const;

    private:
        void WatcherThreadFunc();

        std::filesystem::path m_watchDir;
        ChangeCallback m_callback;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
        DWORD m_pollInterval;
        std::mutex m_mutex;
        std::map<std::filesystem::path, std::time_t> m_lastState;
    };
}
