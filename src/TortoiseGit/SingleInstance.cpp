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
#include "SingleInstance.h"

#include <QDir>
#include <QLockFile>
#include <QStandardPaths>

#include "CrossPlatform/Logging.h"

#ifdef TGIT_HAVE_DBUS
#    include <QDBusConnection>
#    include <QDBusInterface>
#endif

namespace
{
    // Well-known name, object path and interface of the running instance.
    // Keep in sync with the "Single instance" section in linux_readme.md.
    constexpr const char* BusServiceName   = "org.tortoisegit.TortoiseGit";
    constexpr const char* BusObjectPath    = "/org/tortoisegit/TortoiseGit";
    constexpr const char* BusInterfaceName = "org.tortoisegit.TortoiseGit";

    // Time a second instance waits for the running one to accept the command.
    constexpr int BusCallTimeoutMs = 5000;
    // Time to wait for the lock file of a starting instance.
    constexpr int LockTimeoutMs = 100;
} // namespace

#ifdef TGIT_HAVE_DBUS
namespace SingleInstanceBus
{
    Adaptor::Adaptor(SingleInstance* owner)
        : QDBusAbstractAdaptor(owner)
        , m_owner(owner)
    {
    }

    void Adaptor::Activate(const QString& action, const QString& path,
                           const QString& url, const QString& revision)
    {
        m_owner->EmitCommandReceived({action, path, url, revision});
    }
} // namespace SingleInstanceBus
#endif

SingleInstance::SingleInstance(const QString& applicationName, QObject* parent)
    : QObject(parent)
    , m_applicationName(applicationName)
{
}

SingleInstance::~SingleInstance()
{
#ifdef TGIT_HAVE_DBUS
    if (m_registeredOnBus)
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QLatin1String(BusObjectPath));
        bus.unregisterService(QLatin1String(BusServiceName));
        m_registeredOnBus = false;
    }
#endif
    if (m_lock)
        m_lock->unlock();
}

SingleInstance::Result SingleInstance::Start(const Command& command)
{
#ifdef TGIT_HAVE_DBUS
    if (QDBusConnection::sessionBus().isConnected())
    {
        if (RegisterOnBus())
            m_result = Result::Primary;
        else if (ForwardToPrimary(command))
            m_result = Result::Forwarded;
        else
            m_result = Result::Busy;
        return m_result;
    }

    CLogging::LogMessage(CLogging::LogLevel::Debug,
                         "No D-Bus session bus - falling back to the lock file");
#endif
    m_result = AcquireLockFile();
    return m_result;
}

void SingleInstance::EmitCommandReceived(const Command& command)
{
    CLogging::LogMessage(CLogging::LogLevel::Info,
                         "Command received from another instance: '" + command.action.toStdString() + "'");
    emit CommandReceived(command.action, command.path, command.url, command.revision);
}

#ifdef TGIT_HAVE_DBUS
bool SingleInstance::RegisterOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    // The name is the actual single-instance primitive: registering it fails
    // when another process already owns it (ReplaceNonExisting is the default).
    if (!bus.registerService(QLatin1String(BusServiceName)))
        return false;
    m_registeredOnBus = true;

    // The adaptor is a child of this object and is deleted with it; the bus
    // only exports it.
    new SingleInstanceBus::Adaptor(this);

    if (!bus.registerObject(QLatin1String(BusObjectPath), this,
                            QDBusConnection::ExportAdaptors))
    {
        CLogging::LogMessage(CLogging::LogLevel::Warning,
                             "Cannot export the D-Bus object of the running instance");
        // Still the only instance: keep the name so that further starts are
        // detected even when they cannot reach us.
    }
    return true;
}

bool SingleInstance::ForwardToPrimary(const Command& command)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface interface(QLatin1String(BusServiceName), QLatin1String(BusObjectPath),
                             QLatin1String(BusInterfaceName), bus);
    if (!interface.isValid())
    {
        CLogging::LogMessage(CLogging::LogLevel::Warning,
                             "Another instance holds the D-Bus name but has no interface");
        return false;
    }
    interface.setTimeout(BusCallTimeoutMs);

    QDBusMessage reply = interface.call(QStringLiteral("Activate"), command.action,
                                       command.path, command.url, command.revision);
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        CLogging::LogMessage(CLogging::LogLevel::Warning,
                             "Cannot reach the running instance: " + reply.errorMessage().toStdString());
        return false;
    }
    return true;
}
#endif // TGIT_HAVE_DBUS

SingleInstance::Result SingleInstance::AcquireLockFile()
{
    // QLockFile stores the owner PID and breaks a stale lock left behind by a
    // crashed or killed process - QSharedMemory would block all further starts.
    const QString lockPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("tortoisegit-%1.lock").arg(m_applicationName));
    m_lock = std::make_unique<QLockFile>(lockPath);
    if (!m_lock->tryLock(LockTimeoutMs))
    {
        // Do not keep a lock we do not own around, so that the destructor of a
        // second instance never unlocks the owner.
        m_lock.reset();
        return Result::Busy;
    }
    return Result::Primary;
}
