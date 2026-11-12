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
#include "PushPullDialog.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>
#include <exception>
#include <utility>

#include "CredentialsDialog.h"
#include "CrossPlatform/Logging.h"
#include "GitCore/Credentials.h"

namespace
{
    QString DescribeBranch(const GitCore::RepositoryInfo& info)
    {
        if (info.isHeadDetached)
            return QObject::tr("(detached HEAD)");
        if (!info.headBranch.empty())
            return QString::fromStdString(info.headBranch);
        return QObject::tr("(no commits yet)");
    }
}

PushPullDialog::PushPullDialog(GitCore::Repository* repo, Mode mode, QWidget* parent)
    : QDialog(parent)
    , m_repo(repo)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Push ? tr("Git Push") : tr("Git Pull"));
    // Owned by MainWindow and not interruptible: the user must not be able to
    // close the repository or the dialog while the transfer runs.
    setWindowModality(Qt::WindowModal);
    resize(640, 420);

    m_remotes = new QComboBox(this);

    auto* remoteRow = new QHBoxLayout();
    remoteRow->addWidget(new QLabel(mode == Mode::Push ? tr("Push to:") : tr("Pull from:"), this));
    remoteRow->addWidget(m_remotes, 1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setTextFormat(Qt::RichText);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_actionButton = new QPushButton(mode == Mode::Push ? tr("&Push") : tr("&Pull"), this);
    m_closeButton  = new QPushButton(tr("&Close"), this);
    m_actionButton->setDefault(true);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(m_actionButton);
    bottomRow->addWidget(m_closeButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(remoteRow);
    layout->addWidget(m_hintLabel);
    layout->addWidget(m_log, 1);
    layout->addLayout(bottomRow);

    m_credentials = new CredentialsRequester(this);

    connect(m_remotes, &QComboBox::currentIndexChanged, this, [this] { UpdateHint(); });
    connect(m_actionButton, &QPushButton::clicked, this, &PushPullDialog::Start);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &PushPullDialog::OnFinished);

    // The remotes are read here (not in a worker): it is a config lookup and
    // the dialog needs the list before it can enable anything.
    try
    {
        for (const auto& remote : GitCore::ListRemotes(*m_repo))
        {
            m_remotes->addItem(QString::fromStdString(remote.name),
                               QString::fromStdString(remote.url));
        }
    }
    catch (const std::exception& e)
    {
        AppendLine(tr("Failed to read the remotes: %1").arg(QString::fromUtf8(e.what())));
    }

    if (m_remotes->count() == 0)
        AppendLine(tr("This repository has no remotes configured. Add one with "
                      "`git remote add origin <url>` or with the Clone dialog."));

    UpdateHint();
}

PushPullDialog::~PushPullDialog()
{
    // The transfer uses the repository owned by MainWindow and reports into
    // this dialog, so it has to finish before the dialog goes away.
    if (m_watcher.isRunning())
        m_watcher.future().waitForFinished();
}

QString PushPullDialog::CurrentRemoteName() const
{
    return m_remotes->currentText();
}

QString PushPullDialog::CurrentRemoteUrl() const
{
    return m_remotes->currentData().toString();
}

void PushPullDialog::UpdateHint()
{
    const GitCore::RepositoryInfo& info = m_repo->Info();
    const QString remoteName = CurrentRemoteName();
    const QString url        = CurrentRemoteUrl();
    const QString what       = m_mode == Mode::Push ? tr("Pushing") : tr("Pulling");

    m_transportAvailable = true;
    bool   blocked = false;
    QString note;
    if (url.isEmpty())
    {
        blocked = true;
        note    = tr("No remote is configured.");
    }
    else
    {
        const QString missing = QString::fromStdString(GitCore::MissingTransportHint(url.toStdString()));
        if (!missing.isEmpty())
        {
            m_transportAvailable = false;
            blocked              = true;
            note                 = missing;
        }
    }

    if (info.isHeadDetached)
    {
        blocked = true;
        note    = tr("HEAD is detached - check out a branch first.");
    }
    else if (m_mode == Mode::Push && info.isEmpty)
    {
        blocked = true;
        note    = tr("The repository has no commits yet, so there is nothing to push.");
    }

    QString text = tr("%1 <b>%2</b> &rarr; <b>%3</b><br>%4")
                       .arg(what, DescribeBranch(info),
                            remoteName.isEmpty() ? tr("(none)") : remoteName.toHtmlEscaped(),
                            url.toHtmlEscaped());
    if (!note.isEmpty())
    {
        text += blocked ? QStringLiteral("<br><span style=\"color:#b00020;\"><b>%1</b></span>")
                                 .arg(note.toHtmlEscaped())
                        : QStringLiteral("<br><i>%1</i>").arg(note.toHtmlEscaped());
    }
    m_hintLabel->setText(text);

    m_actionButton->setEnabled(!m_watcher.isRunning() && !blocked);

    CLogging::LogMessage(CLogging::LogLevel::Debug,
                         "Push/Pull dialog target: " + remoteName.toStdString() + " " + url.toStdString());
}

GitCore::RemoteOptions PushPullDialog::MakeOptions()
{
    GitCore::RemoteOptions options;
    options.remoteName = CurrentRemoteName().toStdString();

    // libgit2 reports the transfer and asks for credentials from the thread
    // running it. The progress text is buffered and flushed into the widget by
    // the GUI thread, credentials are requested through a modal dialog there.
    options.progress = [this](const std::string& text) {
        {
            std::lock_guard<std::mutex> lock(m_progressMutex);
            m_progressText += QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
        }
        if (!m_flushQueued.exchange(true))
            QMetaObject::invokeMethod(this, [this] { FlushProgress(); }, Qt::QueuedConnection);
    };
    options.credentials = m_credentials->Provider();
    return options;
}

void PushPullDialog::Start()
{
    const QString remoteName = CurrentRemoteName();
    if (remoteName.isEmpty() || m_watcher.isRunning())
        return;

    GitCore::RemoteOptions options = MakeOptions();
    const Mode mode                = m_mode;

    AppendLine(m_mode == Mode::Push ? tr("Pushing to '%1'...").arg(remoteName)
                                    : tr("Pulling from '%1'...").arg(remoteName));
    SetBusy(true);

    GitCore::Repository* repo = m_repo;
    m_watcher.setFuture(QtConcurrent::run([repo, mode, options] {
        Result result;
        try
        {
            if (mode == Mode::Push)
                GitCore::Push(*repo, {}, options);
            else if (!GitCore::Pull(*repo, options))
                result.diverged = true;
        }
        catch (const std::exception& e)
        {
            result.error = QString::fromUtf8(e.what());
        }
        return result;
    }));
}

void PushPullDialog::SetBusy(bool busy)
{
    m_remotes->setEnabled(!busy);
    // Interrupting a transfer is not implemented, so leaving the dialog is not
    // offered either - the log would keep arriving in a closed window.
    m_closeButton->setEnabled(!busy);

    if (busy)
        m_actionButton->setEnabled(false);
    else
        UpdateHint(); // re-enables the action button when the target is usable again
}

void PushPullDialog::AppendLine(const QString& line)
{
    // The remote repeats its progress line many times per second.
    if (line.isEmpty() || line == m_lastLine)
        return;

    m_lastLine = line;
    m_log->appendPlainText(line);
    CLogging::LogMessage(CLogging::LogLevel::Debug, "Remote progress: " + line.toStdString());
}

void PushPullDialog::FlushProgress()
{
    QString text;
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        text.swap(m_progressText);
    }
    m_flushQueued = false;

    for (const QString& chunk : text.split(QLatin1Char('\n')))
    {
        // A remote overwrites its progress line with '\r', so only the last
        // part of such a chunk is new information.
        AppendLine(chunk.section(QLatin1Char('\r'), -1).trimmed());
    }
}

void PushPullDialog::OnFinished()
{
    const Result result = m_watcher.result();
    FlushProgress();
    SetBusy(false);

    if (!result.error.isEmpty())
    {
        AppendLine(result.error);
        QMessageBox::warning(this, windowTitle(), result.error);
        return;
    }

    if (result.diverged)
    {
        const QString message = tr("The branch has diverged from '%1'.\n"
                                   "Nothing was merged - run a merge or a rebase first.")
                                    .arg(CurrentRemoteName());
        AppendLine(message);
        QMessageBox::information(this, windowTitle(), message);
        return;
    }

    AppendLine(m_mode == Mode::Push ? tr("Pushed to '%1'.").arg(CurrentRemoteName())
                                    : tr("Pulled from '%1'.").arg(CurrentRemoteName()));
    accept();
}
