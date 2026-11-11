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
#include <QLockFile>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QStandardPaths>
#include <memory>

#include "CrossPlatform/Path.h"
#include "CrossPlatform/Logging.h"
#include "CrossPlatform/ConfigStore.h"
#include "CrossPlatform/FileSystem.h"

#include "DiffDialog.h"
#include "MainWindow.h"
#include "LogDialog.h"
#include "StatusDialog.h"

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
        {{"s", "action"}, "Action to perform (commit, log, diff, etc.)", "action"},
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

    // Check single instance. QLockFile stores the owner PID in the lock file
    // and automatically breaks a stale lock left behind by a crashed or
    // killed process - QSharedMemory would block all further starts.
    QLockFile singleInstanceLock(
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("tortoisegit-%1.lock").arg(QString::fromLatin1(APP_NAME))));
    if (!singleInstanceLock.tryLock(100))
    {
        CLogging::LogMessage(CLogging::LogLevel::Info, "Another instance is running");
        // TODO: Forward command to existing instance via D-Bus
        return 0;
    }

    CLogging::LogMessage(CLogging::LogLevel::Info, "Main application initialized");

    MainWindow window;

    const QString action = parser.value("action").trimmed().toLower();
    const QString pathArg = parser.value("path");

    if (!action.isEmpty())
    {
        // Dialog mode (TortoiseGit-style): open one dialog for the action
        // on the repository found at --path (default: current directory).
        const QString repoPath = pathArg.isEmpty() ? QDir::currentPath() : pathArg;
        if (!window.OpenRepository(repoPath))
            return 1;

        const GitCore::Repository* repo = window.Repo();
        if (action == QLatin1String("log"))
        {
            LogDialog dialog(repo);
            dialog.exec();
        }
        else if (action == QLatin1String("status"))
        {
            StatusDialog dialog(repo);
            dialog.exec();
        }
        else if (action == QLatin1String("diff"))
        {
            DiffDialog dialog(repo, QStringLiteral("HEAD"), QString());
            dialog.exec();
        }
        else
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
