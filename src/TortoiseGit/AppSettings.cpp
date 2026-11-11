// TortoiseGit - Linux GUI for Git
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
#include "AppSettings.h"

#include "CrossPlatform/ConfigStore.h"

namespace
{
    constexpr const char* APP_NAME  = "TortoiseGit";
    constexpr const char* SECTION   = "General";
    constexpr const char* KEY_LOG_LIMIT     = "LogLimit";
    constexpr const char* KEY_REPO_PATH     = "DefaultRepoPath";

    // One store instance for the whole application lifetime, positioned
    // in the [General] section.
    CConfig::IConfigStore& Store()
    {
        static std::unique_ptr<CConfig::IConfigStore> store = CConfig::CreateConfigStore(APP_NAME);
        static bool initialized = [] {
            store->Load();
            store->Section(SECTION);
            return true;
        }();
        (void)initialized;
        return *store;
    }
}

namespace AppSettings
{
    int LogLimit()
    {
        CConfig::ConfigValue def;
        def.Set(100);
        int limit = Store().Get(KEY_LOG_LIMIT, def).AsInt();
        return limit < 0 ? 100 : limit;
    }

    void SetLogLimit(int limit)
    {
        CConfig::ConfigValue value;
        value.Set(limit);
        Store().Set(KEY_LOG_LIMIT, value);
        Store().Save();
    }

    std::string DefaultRepoPath()
    {
        CConfig::ConfigValue def;
        def.Set(std::string());
        return Store().Get(KEY_REPO_PATH, def).AsString();
    }

    void SetDefaultRepoPath(const std::string& path)
    {
        CConfig::ConfigValue value;
        value.Set(path);
        Store().Set(KEY_REPO_PATH, value);
        Store().Save();
    }
}
