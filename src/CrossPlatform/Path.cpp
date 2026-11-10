// TortoiseGit - cross-platform path utilities
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
#include "Path.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <libgen.h>
#endif

namespace CPath
{
    bool IsRelative(const std::string& path)
    {
#ifdef _WIN32
        return !PathIsRelativeW(CStringW(path.c_str()).GetString());
#else
        return !std::filesystem::path(path).is_absolute();
#endif
    }

    bool IsAbsolutePath(const std::string& path)
    {
#ifdef _WIN32
        return PathIsAbsoluteW(CStringW(path.c_str()).GetString());
#else
        return std::filesystem::path(path).is_absolute();
#endif
    }

    std::string GetFileName(const std::string& path)
    {
        return std::filesystem::path(path).filename().string();
    }

    std::string GetFileExtension(const std::string& path)
    {
        auto ext = std::filesystem::path(path).extension().string();
        // Remove leading dot if present
        if (!ext.empty() && ext[0] == '.')
            ext.erase(ext.begin());
        return ext;
    }

    std::string GetFileRootName(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    std::string GetParentPath(const std::string& path)
    {
        auto p = std::filesystem::path(path).parent_path();
        return p.string();
    }

    std::string GetFullPath(const std::string& path)
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (GetFullPathNameW(CStringW(path.c_str()).GetString(), MAX_PATH, buf, nullptr))
            return CStringA(buf).GetString();
        return path;
#else
        std::filesystem::path p(path);
        if (p.is_relative())
            p = std::filesystem::current_path() / p;
        return p.string();
#endif
    }

    std::string GetCurrentDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, buf))
            return CStringA(buf).GetString();
        return "";
#else
        char buf[PATH_MAX];
        if (getcwd(buf, sizeof(buf)))
            return std::string(buf);
        return "";
#endif
    }

    void SetCurrentDirectory(const std::string& path)
    {
#ifdef _WIN32
        SetCurrentDirectoryW(CStringW(path.c_str()).GetString());
#else
        ::chdir(path.c_str());
#endif
    }

    std::string CombinePaths(const std::string& base, const std::string& relative)
    {
        return (std::filesystem::path(base) / relative).string();
    }

    bool DirectoryExists(const std::string& path)
    {
        return std::filesystem::is_directory(path);
    }

    bool FileExists(const std::string& path)
    {
        return std::filesystem::exists(path) && !std::filesystem::is_directory(path);
    }

    bool PathExists(const std::string& path)
    {
        return std::filesystem::exists(path);
    }

    bool CreateDirectory(const std::string& path)
    {
        std::error_code ec;
        return std::filesystem::create_directory(path, ec);
    }

    bool MakeSureDirectoryPathExists(const std::string& path)
    {
        std::error_code ec;
        return std::filesystem::create_directories(path, ec);
    }

    bool RemoveDirectory(const std::string& path)
    {
        std::error_code ec;
        return std::filesystem::remove_all(path, ec) > 0;
    }

    bool RemoveFile(const std::string& path)
    {
        std::error_code ec;
        return std::filesystem::remove(path, ec);
    }

    bool CopyFile(const std::string& src, const std::string& dst, bool overwrite)
    {
        std::error_code ec;
        if (overwrite)
            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        else
            std::filesystem::copy(src, dst, std::filesystem::copy_options::skip_existing, ec);
        return !ec;
    }

    bool MoveFile(const std::string& src, const std::string& dst)
    {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        return !ec;
    }

    bool IsSamePath(const std::string& a, const std::string& b)
    {
        auto pa = std::filesystem::path(a).lexically_normal();
        auto pb = std::filesystem::path(b).lexically_normal();
#ifdef _WIN32
        // Case-insensitive comparison on Windows
        std::string la = pa.string();
        std::string lb = pb.string();
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la == lb;
#else
        return pa == pb;
#endif
    }

    bool IsPathChild(const std::string& child, const std::string& parent)
    {
        auto pc = std::filesystem::path(child).lexically_normal();
        auto pp = std::filesystem::path(parent).lexically_normal();
        auto rel = std::filesystem::relative(pc, pp);
        return !rel.empty() && rel.begin()->string() != "..";
    }

    bool IsPathAncestor(const std::string& ancestor, const std::string& descendant)
    {
        return IsPathChild(descendant, ancestor);
    }

    std::string NormalizePath(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }

    std::string NormalizePathSeparators(const std::string& path)
    {
        std::string result = path;
#ifdef _WIN32
        std::replace(result.begin(), result.end(), '/', '\\');
#else
        std::replace(result.begin(), result.end(), '\\', '/');
#endif
        return result;
    }

    std::filesystem::path MakeAbsolute(const std::string& relativePath, const std::string& baseDir)
    {
        std::filesystem::path p(relativePath);
        if (p.is_relative())
            return std::filesystem::path(baseDir) / p;
        return p;
    }

    std::string DropPathPrefixes(const std::string& path)
    {
#ifdef _WIN32
        std::string result = path;
        // Remove Windows long path prefixes: \\?\, \\.\, \??\
        if (result.size() > 4 && result.substr(0, 4) == "\\\\?\\")
            result = result.substr(4);
        if (result.size() > 3 && result.substr(0, 3) == "\\\\.\\")
            result = result.substr(3);
        return result;
#else
        return path;
#endif
    }

    std::string ExpandFileName(const std::string& fileName)
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (GetFullPathNameW(CStringW(fileName.c_str()).GetString(), MAX_PATH, buf, nullptr))
            return CStringA(buf).GetString();
        return fileName;
#else
        std::filesystem::path p(fileName);
        if (p.is_relative())
            return (std::filesystem::current_path() / p).string();
        return fileName;
#endif
    }
}
