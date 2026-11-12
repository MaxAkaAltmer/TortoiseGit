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
#include <functional>
#include <vector>

#include "GitCore/Commit.h"
#include "GitCore/Status.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace GitCore
{
    class Repository;
}

// The commit dialog: the working tree with a checkbox per file (checked means
// "in the index", i.e. it will be committed) and the message below, like the
// commit dialog of TortoiseGit.
//
// Staging, unstaging and committing run on a worker thread (QtConcurrent) -
// staging a big working tree takes longer than a paint interval. The dialogs
// of this phase are window-modal so that the repository cannot be closed from
// the main window while such an operation is running.
class CommitDialog : public QDialog
{
    Q_OBJECT

public:
    // `repo` is not const: staging writes the index, committing moves HEAD.
    explicit CommitDialog(GitCore::Repository* repo, QWidget* parent = nullptr);
    ~CommitDialog() override;

private slots:
    void Refresh();
    void OnRefreshFinished();
    void OnItemChanged(QTreeWidgetItem* item, int column);
    void StageSelected();
    void UnstageSelected();
    void Commit();
    void OnOperationFinished();

private:
    // Worker threads cannot throw through a QFuture<void>, so they report the
    // failure as a message instead.
    struct LoadResult
    {
        QString error;
        std::vector<GitCore::FileStatus> files;
        GitCore::Identity identity;
    };
    struct OperationResult
    {
        QString error;
        QString commitId; // set after a successful commit
    };

    // Runs `operation` on a worker thread; it must not touch any widget.
    void Run(const QString& what, std::function<OperationResult (GitCore::Repository&)> operation);
    void SetBusy(bool busy, const QString& what);
    void FillTree(const std::vector<GitCore::FileStatus>& files);
    void ApplyStaging(const std::vector<std::string>& paths, bool stage);
    std::vector<std::string> SelectedPaths() const;
    void UpdateCommitButton();

    GitCore::Repository* m_repo = nullptr;
    // Identity as of the last refresh; it decides together with the message
    // whether the Commit button can be used.
    GitCore::Identity m_identity;

    QTreeWidget*    m_files         = nullptr;
    QPlainTextEdit* m_message       = nullptr;
    QLabel*         m_identityLabel = nullptr;
    QLabel*         m_summaryLabel  = nullptr;
    QPushButton*    m_refreshButton  = nullptr;
    QPushButton*    m_stageButton    = nullptr;
    QPushButton*    m_unstageButton  = nullptr;
    QPushButton*    m_stageAllButton = nullptr;
    QPushButton*    m_unstageAllButton = nullptr;
    QPushButton*    m_commitButton   = nullptr;

    // Guards itemChanged() while the tree is being filled programmatically.
    bool m_updatingTree = false;
    bool m_busy         = false;

    QFutureWatcher<LoadResult>      m_loadWatcher;
    QFutureWatcher<OperationResult> m_opWatcher;
};
