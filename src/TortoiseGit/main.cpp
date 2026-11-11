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

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QLoggingCategory>
#include <QSharedMemory>
#include <QFileInfo>
#include <QStandardPaths>
#include <memory>

#include "CrossPlatform/Path.h"
#include "CrossPlatform/Logging.h"
#include "CrossPlatform/ConfigStore.h"
#include "CrossPlatform/FileSystem.h"

// Forward declaration
class MainWindow;

static const char* APP_NAME = "TortoiseGit";
static const char* APP_VERSION = "3.0.0";

int main(int argc, char* argv[])
{
    // Initialize cross-platform logging
    CLogging::Init(APP_NAME);
    CLogging::LogMessage(CLogging::LogLevel::Info, "TortoiseGit Linux starting up");

    // Configure Qt logging
    QLoggingCategory::setFilterRules("*.debug=true\n"
                                     "*.info=true\n"
                                     "*.warning=true\n"
                                     "*.critical=false");

    // Enable High DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(APP_NAME);
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("TortoiseGit");
    app.setOrganizationDomain("tortoisegit.org");

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("TortoiseGit - Linux GUI for Git");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOptions({
        {{'s', 'action'}, "Action to perform (commit, log, diff, etc.)", "action"},
        {{'p', 'path'}, "Target path", "path"},
        {{'u', 'url'}, "URL (for clone/push/pull)", "url"},
        {{'r', 'revision'}, "Revision/commit to work with", "revision"},
        {{'d', 'debug'}, "Enable debug mode"},
        {{'v', 'verbose'}, "Enable verbose output"},
    });

    parser.process(app);

    // Initialize config store
    CConfig::LinuxConfigStore config(APP_NAME);
    config.Load();

    // Check single instance
    QSharedMemory sharedMem(APP_NAME);
    if (!sharedMem.create(1, QSharedMemory::ShareExclusive))
    {
        if (sharedMem.error() == QSharedMemory::AlreadyExists)
        {
            CLogging::LogMessage(CLogging::LogLevel::Info, "Another instance is running");
            // TODO: Forward command to existing instance via D-Bus
            return 0;
        }
    }

    CLogging::LogMessage(CLogging::LogLevel::Info, "Main application initialized");
    return 0;
}
