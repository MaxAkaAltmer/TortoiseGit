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

#include <QDialog>
#include <QFutureWatcher>
#include <QString>
#include <atomic>
#include <mutex>

#include "GitCore/Remote.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace GitCore
{
    class Repository;
}

class CredentialsRequester;

// Push or pull of one of the configured remotes, with the log the remote
// reports (the "progress" window of TortoiseGit). The transfer runs on a
// worker thread; progress lines are buffered and flushed into the log, and
// credential requests are shown by CredentialsRequester in the GUI thread.
//
// A transfer cannot be interrupted here, so the dialog is window-modal and the
// Close button is disabled while it runs.
class PushPullDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        Push,
        Pull,
    };

    PushPullDialog(GitCore::Repository* repo, Mode mode, QWidget* parent = nullptr);
    ~PushPullDialog() override;

private slots:
    void UpdateHint();
    void Start();
    void OnFinished();

private:
    struct Result
    {
        QString error;      // empty when the transfer went through
        bool    diverged = false; // a pull that cannot fast-forward
    };

    GitCore::RemoteOptions MakeOptions();
    QString CurrentRemoteName() const;
    QString CurrentRemoteUrl() const;
    // Worker threads cannot throw through a QFuture<void>, so they report the
    // failure as a message instead.
    void Run();
    void SetBusy(bool busy);
    void AppendLine(const QString& line);
    // Takes the lines the transfer reported so far into the log.
    void FlushProgress();

    GitCore::Repository* m_repo = nullptr;
    Mode                 m_mode;

    QComboBox*      m_remotes      = nullptr;
    QLabel*         m_hintLabel    = nullptr;
    QPlainTextEdit* m_log          = nullptr;
    QPushButton*    m_actionButton = nullptr;
    QPushButton*    m_closeButton  = nullptr;

    CredentialsRequester* m_credentials = nullptr;

    // Progress arrives from the worker thread much faster than the widget can
    // be painted, so the text is collected here and flushed at most once per
    // event loop iteration (m_flushQueued says whether a flush is pending).
    std::mutex m_progressMutex;
    QString    m_progressText;
    QString    m_lastLine; // repeated progress lines are shown once
    std::atomic_bool m_flushQueued {false};

    bool m_transportAvailable = true;

    QFutureWatcher<Result> m_watcher;
};
