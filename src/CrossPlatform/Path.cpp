#include "Path.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
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

    std::string DropPathPrefixes(const std::string& path)
    {
#ifdef _WIN32
        std::string result = path;
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

    std::filesystem::path MakeAbsolute(const std::string& relativePath, const std::string& baseDir)
    {
        std::filesystem::path p(relativePath);
        if (p.is_relative())
            return std::filesystem::path(baseDir) / p;
        return p;
    }

    // Special directories
    std::string GetHomeDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            return CStringA(buf).GetString();
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

    std::string GetAppDataDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            return CStringA(buf).GetString();
        return "C:\\Users\\Default\\AppData\\Roaming";
#else
        const char* xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && *xdg)
            return xdg;
        return GetHomeDirectory() + "/.config";
#endif
    }

    std::string GetLocalAppDataDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            return CStringA(buf).GetString();
        return "C:\\Users\\Default\\AppData\\Local";
#else
        const char* xdg = getenv("XDG_DATA_HOME");
        if (xdg && *xdg)
            return xdg;
        return GetHomeDirectory() + "/.local/share";
#endif
    }

    std::string GetTempDirectory()
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        if (GetTempPathW(MAX_PATH, buf))
            return CStringA(buf).GetString();
        return "/tmp";
#else
        const char* temp = getenv("TMPDIR");
        if (temp && *temp)
            return temp;
        return "/tmp";
#endif
    }

    std::string GetTempFileName()
    {
        return GetTempDirectory() + "/tgit_XXXXXX.tmp";
    }

    std::string CreateTempFile(const std::string& prefix, const std::string& suffix)
    {
#ifdef _WIN32
        wchar_t template_[MAX_PATH];
        wchar_t fullTemplate[MAX_PATH];
        GetTempPathW(MAX_PATH, fullTemplate);
        wcscat_s(fullTemplate, MAX_PATH, (CString(prefix + "XXXXXX" + suffix).GetString()));
        if (_wmktemp_s(fullTemplate) != 0)
            return GetTempDirectory() + "/" + prefix + "1.tmp" + suffix;
        return CStringA(fullTemplate).GetString();
#else
        std::string template_ = GetTempDirectory() + "/" + prefix + "XXXXXX" + suffix;
        int fd = mkstemps(&template_[0], static_cast<int>(suffix.size()));
        if (fd != -1)
        {
            close(fd);
            return template_;
        }
        return GetTempDirectory() + "/" + prefix + "1.tmp" + suffix;
#endif
    }
}
