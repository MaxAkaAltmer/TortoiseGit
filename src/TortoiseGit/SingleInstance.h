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

#include <QObject>
#include <QString>
#include <memory>

class QLockFile;
class SingleInstance;

#ifdef TGIT_HAVE_DBUS
#    include <QDBusAbstractAdaptor>

namespace SingleInstanceBus
{
    // Adaptor exposing the running instance on the session bus. It is declared
    // in this header (instead of next to its user in the .cpp) so that CMake's
    // AUTOMOC generates its meta-object code.
    class Adaptor : public QDBusAbstractAdaptor
    {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.tortoisegit.TortoiseGit")

    public:
        explicit Adaptor(SingleInstance* owner);

    public slots:
        // Called by every further instance with its command line.
        void Activate(const QString& action, const QString& path,
                      const QString& url, const QString& revision);

    private:
        SingleInstance* m_owner;
    };
} // namespace SingleInstanceBus
#endif

// Single-instance guard of the GUI application.
//
// The first process owns the well-known D-Bus name org.tortoisegit.TortoiseGit
// on the session bus; any further process forwards its command line to that
// instance (see linux_readme.md, "Single instance") and exits. When no session
// bus is available - headless runs, CI, `QT_QPA_PLATFORM=offscreen` without a
// desktop session - the guard falls back to a QLockFile, which can detect a
// running instance but cannot forward commands.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // Command line of a second instance, handed over to the running one.
    struct Command
    {
        QString action;   // "log", "status", "diff", ...; empty: just activate
        QString path;
        QString url;
        QString revision;
    };

    enum class Result
    {
        Primary,     // this process is the only instance - keep running
        Forwarded,   // the command was handed to the running instance
        Busy,        // another instance is running, forwarding is not possible
        Unavailable, // no instance handling available - run without a guard
    };

    explicit SingleInstance(const QString& applicationName, QObject* parent = nullptr);
    ~SingleInstance() override;

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // Makes this process the primary instance, or forwards `command` to the
    // instance that is already running. See Result for the outcome, which is
    // also returned; IsPrimary() reports the same information later on.
    Result Start(const Command& command);

    bool IsPrimary() const { return m_result == Result::Primary; }

    // Called by SingleInstanceBus::Adaptor in the primary instance.
    void EmitCommandReceived(const Command& command);

signals:
    // Emitted in the primary instance when another process forwarded a command.
    void CommandReceived(const QString& action, const QString& path,
                         const QString& url, const QString& revision);

private:
    bool RegisterOnBus();
    static bool ForwardToPrimary(const Command& command);
    Result AcquireLockFile();

    QString m_applicationName;
    Result m_result = Result::Unavailable;
    bool m_registeredOnBus = false;
    std::unique_ptr<QLockFile> m_lock;
};
