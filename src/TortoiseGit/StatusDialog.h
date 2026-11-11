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
#include <utility>
#include <vector>

#include "GitCore/Branches.h"
#include "GitCore/Status.h"

class QLabel;
class QPushButton;
class QTreeWidget;

namespace GitCore
{
    class Repository;
}

// Shows the working tree status (GitCore::GetStatus) and the branch list
// (GitCore::GetBranches); data is loaded on a worker thread (QtConcurrent).
class StatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatusDialog(const GitCore::Repository* repo, QWidget* parent = nullptr);
    ~StatusDialog() override;

private slots:
    void Reload();
    void OnLoadFinished();

private:
    const GitCore::Repository* m_repo;
    QTreeWidget* m_filesTree    = nullptr;
    QTreeWidget* m_branchesTree = nullptr;
    QPushButton* m_reloadButton = nullptr;
    QLabel* m_summaryLabel      = nullptr;
    QFutureWatcher<std::pair<std::vector<GitCore::FileStatus>,
                             std::vector<GitCore::BranchInfo>>> m_watcher;
};
