#pragma once

#include <string>
#include <filesystem>
#include <utility>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <atlstr.h>
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

namespace CPath
{
    // === Basic Path Operations ===
    bool IsRelative(const std::string& path);
    bool IsAbsolutePath(const std::string& path);
    std::string GetFileName(const std::string& path);
    std::string GetFileExtension(const std::string& path);
    std::string GetFileRootName(const std::string& path);
    std::string GetParentPath(const std::string& path);
    std::string GetFullPath(const std::string& path);
    std::string GetCurrentDirectory();
    void SetCurrentDirectory(const std::string& path);
    std::string CombinePaths(const std::string& base, const std::string& relative);

    // === File/Directory Operations ===
    bool DirectoryExists(const std::string& path);
    bool FileExists(const std::string& path);
    bool PathExists(const std::string& path);
    bool CreateDirectory(const std::string& path);
    bool MakeSureDirectoryPathExists(const std::string& path);
    bool RemoveDirectory(const std::string& path);
    bool RemoveFile(const std::string& path);
    bool CopyFile(const std::string& src, const std::string& dst, bool overwrite = false);
    bool MoveFile(const std::string& src, const std::string& dst);

    // === Path Comparison & Normalization ===
    bool IsSamePath(const std::string& a, const std::string& b);
    bool IsPathChild(const std::string& child, const std::string& parent);
    bool IsPathAncestor(const std::string& ancestor, const std::string& descendant);
    std::string NormalizePath(const std::string& path);
    std::string NormalizePathSeparators(const std::string& path);
    std::string DropPathPrefixes(const std::string& path);
    std::string ExpandFileName(const std::string& fileName);

    // === Special Directories ===
    std::string GetHomeDirectory();
    std::string GetAppDataDirectory();
    std::string GetLocalAppDataDirectory();
    std::string GetTempDirectory();
    std::string GetTempFileName();
    std::string CreateTempFile(const std::string& prefix = "tgit_", const std::string& suffix = ".tmp");

    // === Path type aliases for backward compatibility ===
    using String = std::string;
    using WString = std::wstring;

    // === Utility functions ===
    std::filesystem::path MakeAbsolute(const std::string& relativePath, const std::string& baseDir);

    // === CString compatibility shims ===
#ifdef _WIN32
    inline CStringW CStringW_FromString(const std::string& s)
    {
        return CStringW(s.c_str());
    }
    inline CStringA CStringA_FromString(const std::string& s)
    {
        return CStringA(s.c_str());
    }
#endif

    // === Cross-platform path separator ===
    inline std::string ReplaceSeparators(const std::string& path, char newSep)
    {
        std::string result = path;
        std::replace(result.begin(), result.end(), PATH_SEPARATOR, newSep);
        return result;
    }
}
