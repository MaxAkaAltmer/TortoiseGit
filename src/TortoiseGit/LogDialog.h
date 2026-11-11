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
#include <vector>

#include "GitCore/Log.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace GitCore
{
    class Repository;
}

// Shows the commit history of the current HEAD (GitCore::GetLog) with a
// details pane; double-clicking a commit opens its diff against the first
// parent. The history is loaded on a worker thread (QtConcurrent).
class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(const GitCore::Repository* repo, QWidget* parent = nullptr);
    ~LogDialog() override;

private slots:
    void Reload();
    void OnLoadFinished();
    void OnCurrentRowChanged();
    void OnCommitActivated();

private:
    const GitCore::Repository* m_repo;
    QComboBox* m_limitCombo   = nullptr;
    QPushButton* m_reloadButton = nullptr;
    QLabel* m_countLabel      = nullptr;
    QTableWidget* m_table     = nullptr;
    QPlainTextEdit* m_details = nullptr;
    std::vector<GitCore::CommitInfo> m_commits;
    QFutureWatcher<std::vector<GitCore::CommitInfo>> m_watcher;
};
