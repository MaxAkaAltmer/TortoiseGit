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
#pragma once

#include <string>

// GUI settings stored in the [General] section of the application config
// (see CConfig::CreateConfigStore - config.ini under $XDG_CONFIG_HOME on Linux).
namespace AppSettings
{
    // Max number of commits loaded by the log dialog, 0 = all. Default: 100.
    int LogLimit();
    void SetLogLimit(int limit);

    // Directory suggested by the "Open Repository" dialog. Default: empty.
    std::string DefaultRepoPath();
    void SetDefaultRepoPath(const std::string& path);
}
