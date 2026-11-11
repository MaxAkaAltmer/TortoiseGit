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
#include "StatusDialog.h"

#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include "GitCore/GitException.h"

namespace
{
    QString DescribeStatus(char status)
    {
        switch (status)
        {
        case 'A': return QObject::tr("Added");
        case 'M': return QObject::tr("Modified");
        case 'D': return QObject::tr("Deleted");
        case 'R': return QObject::tr("Renamed");
        case 'C': return QObject::tr("Copied");
        case 'T': return QObject::tr("Type change");
        case '?': return QObject::tr("Untracked");
        case 'X': return QObject::tr("Conflicted");
        default:  return QObject::tr("Unchanged");
        }
    }
}

StatusDialog::StatusDialog(const GitCore::Repository* repo, QWidget* parent)
    : QDialog(parent)
    , m_repo(repo)
{
    setWindowTitle(tr("Git Status"));
    resize(900, 550);

    auto* topRow = new QHBoxLayout();
    m_reloadButton = new QPushButton(tr("&Refresh"), this);
    topRow->addWidget(m_reloadButton);
    topRow->addStretch();
    m_summaryLabel = new QLabel(this);
    topRow->addWidget(m_summaryLabel);

    m_filesTree = new QTreeWidget(this);
    m_filesTree->setColumnCount(3);
    m_filesTree->setHeaderLabels({tr("File"), tr("In Index"), tr("In Worktree")});
    m_filesTree->setRootIsDecorated(false);
    m_filesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_filesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_filesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_branchesTree = new QTreeWidget(this);
    m_branchesTree->setColumnCount(2);
    m_branchesTree->setHeaderLabels({tr("Branch"), tr("Commit")});
    m_branchesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_branchesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_filesTree);
    splitter->addWidget(m_branchesTree);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(splitter);

    connect(m_reloadButton, &QPushButton::clicked, this, &StatusDialog::Reload);
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &StatusDialog::OnLoadFinished);

    Reload();
}

StatusDialog::~StatusDialog()
{
    // The worker reads from the repository owned by MainWindow - do not let
    // it outlive this dialog.
    if (m_watcher.isRunning())
        m_watcher.future().waitForFinished();
}

void StatusDialog::Reload()
{
    if (m_watcher.isRunning())
        return;
    const GitCore::Repository* repo = m_repo;
    m_reloadButton->setEnabled(false);
    m_summaryLabel->setText(tr("loading..."));
    m_watcher.setFuture(QtConcurrent::run([repo] {
        return std::make_pair(GitCore::GetStatus(*repo), GitCore::GetBranches(*repo));
    }));
}

void StatusDialog::OnLoadFinished()
{
    m_reloadButton->setEnabled(true);

    std::pair<std::vector<GitCore::FileStatus>, std::vector<GitCore::BranchInfo>> data;
    try
    {
        data = m_watcher.result();
    }
    catch (const GitCore::GitException& e)
    {
        m_summaryLabel->clear();
        QMessageBox::warning(this, tr("Git Status"),
                             tr("Failed to load the status:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }

    m_filesTree->clear();
    for (const auto& file : data.first)
    {
        auto* item = new QTreeWidgetItem(m_filesTree);
        item->setText(0, QString::fromStdString(file.path));
        item->setText(1, DescribeStatus(file.indexStatus));
        item->setText(2, DescribeStatus(file.worktreeStatus));
    }

    m_branchesTree->clear();
    auto* localRoot  = new QTreeWidgetItem(m_branchesTree, {tr("Local branches")});
    auto* remoteRoot = new QTreeWidgetItem(m_branchesTree, {tr("Remote branches")});
    QFont headFont = m_branchesTree->font();
    headFont.setBold(true);
    for (const auto& branch : data.second)
    {
        auto* parent = branch.isRemote ? remoteRoot : localRoot;
        auto* item = new QTreeWidgetItem(parent);
        QString name = QString::fromStdString(branch.name);
        if (branch.isHead)
            name += QStringLiteral(" (HEAD)");
        item->setText(0, name);
        item->setText(1, QString::fromStdString(branch.oid).left(7));
        if (branch.isHead)
            item->setFont(0, headFont);
    }
    localRoot->setExpanded(true);
    remoteRoot->setExpanded(true);
    if (localRoot->childCount() == 0)
        new QTreeWidgetItem(localRoot, {tr("(none)")});
    if (remoteRoot->childCount() == 0)
        remoteRoot->setHidden(true);

    m_summaryLabel->setText(tr("%1 changed entries, %2 branches")
                                .arg(data.first.size())
                                .arg(data.second.size()));
}
