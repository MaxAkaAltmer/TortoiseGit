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
#include <memory>

#include "CrossPlatform/Path.h"
#include "CrossPlatform/Logging.h"
#include "CrossPlatform/ConfigStore.h"
#include "CrossPlatform/FileSystem.h"

#include "MainWindow.h"
#include "SingleInstance.h"

static const char* APP_NAME = "TortoiseGit";
static const char* APP_VERSION = "3.0.0";

int main(int argc, char* argv[])
{
    // Initialize cross-platform logging
    CLogging::Init(APP_NAME);
    CLogging::LogMessage(CLogging::LogLevel::Info, "TortoiseGit Linux starting up");

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
        {{"s", "action"}, "Action to perform (log, status, diff, commit, push, pull)", "action"},
        {{"p", "path"}, "Target path", "path"},
        {{"u", "url"}, "URL (for clone/push/pull)", "url"},
        {{"r", "revision"}, "Revision/commit to work with", "revision"},
        {{"d", "debug"}, "Enable debug mode"},
        // Note: no "v" shortcut - it is taken by the built-in --version option
        {QStringList{"verbose"}, "Enable verbose output"},
    });

    parser.process(app);

    // Configure Qt logging: Qt's internal debug categories are only enabled
    // with --debug/--verbose to keep the default stderr output clean.
    if (parser.isSet("debug") || parser.isSet("verbose"))
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    else
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=true"));

    // Initialize config store
    CConfig::LinuxConfigStore config(APP_NAME);
    config.Load();

    const QString action  = parser.value("action").trimmed().toLower();
    const QString pathArg = parser.value("path");

    // Single instance: the first process owns the D-Bus name of the application
    // and keeps running, every further start hands its command line over to it
    // and exits. Without a session bus only the "already running" case is
    // detected, the command itself is dropped (see SingleInstance).
    SingleInstance singleInstance(APP_NAME);
    switch (singleInstance.Start({action, pathArg, parser.value("url"),
                                  parser.value("revision")}))
    {
    case SingleInstance::Result::Forwarded:
        CLogging::LogMessage(CLogging::LogLevel::Info, "Command forwarded to the running instance");
        return 0;
    case SingleInstance::Result::Busy:
        CLogging::LogMessage(CLogging::LogLevel::Info, "Another instance is running");
        return 0;
    case SingleInstance::Result::Primary:
    case SingleInstance::Result::Unavailable:
        break;
    }

    CLogging::LogMessage(CLogging::LogLevel::Info, "Main application initialized");

    MainWindow window;
    // Commands of further instances open their dialogs in this window
    QObject::connect(&singleInstance, &SingleInstance::CommandReceived,
                     &window, &MainWindow::HandleCommand);

    if (!action.isEmpty())
    {
        // Dialog mode (TortoiseGit-style): open one dialog for the action
        // on the repository found at --path (default: current directory).
        const QString repoPath = pathArg.isEmpty() ? QDir::currentPath() : pathArg;
        if (!window.OpenRepository(repoPath))
            return 1;

        if (!window.ExecAction(action, parser.value("revision")))
        {
            CLogging::LogMessage(CLogging::LogLevel::Warning,
                                 "Unknown action '" + action.toStdString() + "', starting GUI");
            window.show();
        }
        return 0;
    }

    // Normal GUI mode: show the main window, optionally opening --path
    if (!pathArg.isEmpty())
        window.OpenRepository(pathArg);
    window.show();
    return app.exec();
}
