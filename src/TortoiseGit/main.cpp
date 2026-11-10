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
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSharedMemory>
#include <memory>

#include "CrossPlatform/Path.h"
#include "CrossPlatform/Logging.h"
#include "CrossPlatform/ConfigStore.h"
#include "CrossPlatform/FileSystem.h"

// Forward declaration of main window
class MainWindow;

static const char* APP_NAME = "TortoiseGit";
static const char* APP_VERSION = "3.0.0";

int main(int argc, char* argv[])
{
    // Initialize cross-platform logging
    CLogging::Init(APP_NAME);
    CLogging::LogMessage(CLogging::LogLevel::Info, "TortoiseGit starting up");

    // Configure Qt logging
    QLoggingCategory::setFilterRules("*.debug=true\n"
                                     "*.info=true\n"
                                     "*.warning=true\n"
                                     "*.critical=false");

    // Use High DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(APP_NAME);
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("TortoiseGit");
    app.setOrganizationDomain("tortoisegit.org");
    app.setDesktopFileName("org.tortoisegit.TortoiseGit");

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("TortoiseGit - Linux GUI for Git");
    parser.addHelpOption();
    parser.addVersionOption();

    // Common TortoiseGit command line options
    parser.addOptions({
        {{"s", "action"}, "Action to perform (commit, log, diff, etc.)", "action"},
        {{"p", "path"}, "Target path", "path"},
        {{"u", "url"}, "URL (for clone/push/pull)", "url"},
        {{"r", "revision"}, "Revision/commit to work with", "revision"},
        {{"c", "config"}, "Configuration file", "config"},
        {{"d", "debug"}, "Enable debug mode"},
        {{"v", "verbose"}, "Enable verbose output"},
    });

    parser.addPositionalOptions({"positional"},
                                Qt::CommandLinePosition::Last,
                                QObject::tr("positions"));

    parser.process(app);

    // Initialize config store (XDG_CONFIG_HOME on Linux)
    CConfigStore config(APP_NAME);
    config.Load();

    // Get working directory
    QString workDir = QDir::currentPath();
    CLogging::LogMessage(CLogging::LogLevel::Info, QString("Working directory: %1").arg(workDir).toStdString());

    // Check if this is a shell extension invocation
    QString shellExtension = parser.value("shell-extension");
    if (!shellExtension.isEmpty())
    {
        // Shell extension mode - show context menu and exit
        CLogging::LogMessage(CLogging::LogLevel::Info, "Shell extension mode");
        return 0;
    }

    // Check for single instance
    QSharedMemory sharedMem(QString::fromUtf8(APP_NAME));
    if (sharedMem.isAttached())
    {
        // Already running - pass command to existing instance via D-Bus
        CLogging::LogMessage(CLogging::LogLevel::Info, "Another instance is running, forwarding to it");
        // TODO: Implement D-Bus forwarding to existing instance
        return 0;
    }
    sharedMem.create(1);

    // TODO: Initialize shell extension cache connection (tgit-status-cache via D-Bus)
    // TODO: Parse arguments and dispatch to appropriate command handler

    // For now, show about dialog if no arguments
    // In full implementation, this would parse args and show the appropriate dialog
    CLogging::LogMessage(CLogging::LogLevel::Info, "Starting main application");

    // Return 0 for now - the full implementation would create the MainWindow
    // and process the command arguments
    CLogging::LogMessage(CLogging::LogLevel::Info, "TortoiseGit shutting down");
    return 0;
}
