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
#include "CommitDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>
#include <exception>
#include <utility>

#include "CrossPlatform/Logging.h"
#include "GitCore/GitException.h"
#include "GitStatusText.h"

namespace
{
    // Status of a file in the index: everything but "unmodified" means the
    // file will be part of the next commit.
    bool IsStaged(const GitCore::FileStatus& file)
    {
        return file.indexStatus != ' ';
    }

    QString DescribeIdentity(const GitCore::Identity& identity)
    {
        if (identity.name.empty() || identity.email.empty())
            return QObject::tr("<b>No commit identity configured.</b> Set "
                               "<tt>user.name</tt> and <tt>user.email</tt> in the "
                               "repository or global git configuration first.");
        return QObject::tr("Committing as <b>%1</b> &lt;%2&gt;")
            .arg(QString::fromStdString(identity.name).toHtmlEscaped(),
                 QString::fromStdString(identity.email).toHtmlEscaped());
    }
}

CommitDialog::CommitDialog(GitCore::Repository* repo, QWidget* parent)
    : QDialog(parent)
    , m_repo(repo)
{
    setWindowTitle(tr("Git Commit"));
    // The repository is owned by the main window; blocking it keeps the user
    // from closing the repository while a staging or commit operation runs.
    setWindowModality(Qt::WindowModal);
    resize(850, 600);

    m_refreshButton    = new QPushButton(tr("&Refresh"), this);
    m_stageAllButton   = new QPushButton(tr("Stage &All"), this);
    m_unstageAllButton = new QPushButton(tr("Unstage A&ll"), this);
    m_stageButton      = new QPushButton(tr("Stage &Selected"), this);
    m_unstageButton    = new QPushButton(tr("&Unstage Selected"), this);
    m_commitButton     = new QPushButton(tr("&Commit"), this);
    auto* cancelButton = new QPushButton(tr("Cancel"), this);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(m_refreshButton);
    topRow->addWidget(m_stageAllButton);
    topRow->addWidget(m_unstageAllButton);
    topRow->addStretch();
    m_summaryLabel = new QLabel(this);
    topRow->addWidget(m_summaryLabel);

    m_files = new QTreeWidget(this);
    m_files->setColumnCount(4);
    m_files->setHeaderLabels({tr("Staged"), tr("File"), tr("In Index"), tr("In Worktree")});
    m_files->setRootIsDecorated(false);
    m_files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_files->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_files->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_files->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_files->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    auto* selectionRow = new QHBoxLayout();
    selectionRow->addWidget(m_stageButton);
    selectionRow->addWidget(m_unstageButton);
    selectionRow->addStretch();

    m_identityLabel = new QLabel(this);
    m_identityLabel->setTextFormat(Qt::RichText);
    m_identityLabel->setWordWrap(true);

    m_message = new QPlainTextEdit(this);
    m_message->setPlaceholderText(tr("Commit message (Ctrl+Return commits)"));

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(m_commitButton);
    bottomRow->addWidget(cancelButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_files, 3);
    layout->addLayout(selectionRow);
    layout->addWidget(m_identityLabel);
    layout->addWidget(m_message, 2);
    layout->addLayout(bottomRow);

    m_commitButton->setDefault(true);

    connect(m_refreshButton, &QPushButton::clicked, this, &CommitDialog::Refresh);
    connect(m_stageAllButton, &QPushButton::clicked, this, [this] { ApplyStaging({}, true); });
    connect(m_unstageAllButton, &QPushButton::clicked, this, [this] { ApplyStaging({}, false); });
    connect(m_stageButton, &QPushButton::clicked, this, &CommitDialog::StageSelected);
    connect(m_unstageButton, &QPushButton::clicked, this, &CommitDialog::UnstageSelected);
    connect(m_commitButton, &QPushButton::clicked, this, &CommitDialog::Commit);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_files, &QTreeWidget::itemChanged, this, &CommitDialog::OnItemChanged);
    connect(m_files, &QTreeWidget::itemSelectionChanged, this, [this] {
        const bool hasSelection = !m_files->selectedItems().isEmpty();
        m_stageButton->setEnabled(hasSelection && !m_busy);
        m_unstageButton->setEnabled(hasSelection && !m_busy);
    });
    connect(m_message, &QPlainTextEdit::textChanged, this, &CommitDialog::UpdateCommitButton);

    connect(&m_loadWatcher, &QFutureWatcherBase::finished, this, &CommitDialog::OnRefreshFinished);
    connect(&m_opWatcher, &QFutureWatcherBase::finished, this, &CommitDialog::OnOperationFinished);

    // The TortoiseGit shortcut: Ctrl+Return commits from the message field.
    auto* commitShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(commitShortcut, &QShortcut::activated, this, [this] {
        if (m_commitButton->isEnabled())
            Commit();
    });

    Refresh();
}

CommitDialog::~CommitDialog()
{
    // The workers use the repository owned by MainWindow - do not let this
    // dialog outlive them.
    if (m_loadWatcher.isRunning())
        m_loadWatcher.future().waitForFinished();
    if (m_opWatcher.isRunning())
        m_opWatcher.future().waitForFinished();
}

void CommitDialog::Refresh()
{
    if (m_loadWatcher.isRunning() || m_busy)
        return;

    m_refreshButton->setEnabled(false);
    m_summaryLabel->setText(tr("loading..."));

    GitCore::Repository* repo = m_repo;
    m_loadWatcher.setFuture(QtConcurrent::run([repo] {
        LoadResult result;
        try
        {
            result.files    = GitCore::GetStatus(*repo);
            result.identity = GitCore::DefaultIdentity(*repo);
        }
        catch (const std::exception& e)
        {
            result.error = QString::fromUtf8(e.what());
        }
        return result;
    }));
}

void CommitDialog::OnRefreshFinished()
{
    m_refreshButton->setEnabled(!m_busy);

    // QtConcurrent does not carry exceptions for every result type, so the
    // worker already turned the failure into a message.
    const LoadResult result = m_loadWatcher.result();
    if (!result.error.isEmpty())
    {
        m_summaryLabel->clear();
        QMessageBox::warning(this, tr("Git Commit"),
                             tr("Failed to load the status:\n%1").arg(result.error));
        return;
    }

    m_identity = result.identity;
    FillTree(result.files);

    int staged = 0;
    int changed = 0;
    int untracked = 0;
    for (const auto& file : result.files)
    {
        if (IsStaged(file))
            ++staged;
        if (file.worktreeStatus == '?')
            ++untracked;
        else if (file.worktreeStatus != ' ')
            ++changed;
    }
    m_summaryLabel->setText(tr("%1 staged, %2 changed, %3 untracked")
                                .arg(staged)
                                .arg(changed)
                                .arg(untracked));

    m_identityLabel->setText(DescribeIdentity(m_identity));
    UpdateCommitButton();
}

void CommitDialog::FillTree(const std::vector<GitCore::FileStatus>& files)
{
    // The check states are set programmatically, so the toggled signal of the
    // tree must be ignored until the last item is in place.
    m_updatingTree = true;
    m_files->clear();
    for (const auto& file : files)
    {
        auto* item = new QTreeWidgetItem(m_files);
        item->setText(1, QString::fromStdString(file.path));
        item->setText(2, GitStatusText::Describe(file.indexStatus));
        item->setText(3, GitStatusText::Describe(file.worktreeStatus));
        item->setCheckState(0, IsStaged(file) ? Qt::Checked : Qt::Unchecked);
    }
    m_updatingTree = false;
}

void CommitDialog::OnItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_updatingTree || m_busy || column != 0)
        return;

    // The user toggled the checkbox: stage or unstage just this file.
    const QString path = item->text(1);
    if (path.isEmpty())
        return;

    ApplyStaging({path.toStdString()}, item->checkState(0) == Qt::Checked);
}

void CommitDialog::StageSelected()
{
    const std::vector<std::string> paths = SelectedPaths();
    if (paths.empty())
        return;
    ApplyStaging(paths, true);
}

void CommitDialog::UnstageSelected()
{
    const std::vector<std::string> paths = SelectedPaths();
    if (paths.empty())
        return;
    ApplyStaging(paths, false);
}

std::vector<std::string> CommitDialog::SelectedPaths() const
{
    std::vector<std::string> paths;
    for (const QTreeWidgetItem* item : m_files->selectedItems())
    {
        const QString path = item->text(1);
        if (!path.isEmpty())
            paths.push_back(path.toStdString());
    }
    return paths;
}

void CommitDialog::ApplyStaging(const std::vector<std::string>& paths, bool stage)
{
    Run(stage ? tr("Staging changes...") : tr("Unstaging changes..."),
        [paths, stage](GitCore::Repository& repo) {
            OperationResult result;
            if (stage)
                GitCore::Stage(repo, paths);
            else
                GitCore::Unstage(repo, paths);
            return result;
        });
}

void CommitDialog::Commit()
{
    const QString message = m_message->toPlainText().trimmed();
    if (message.isEmpty())
    {
        QMessageBox::warning(this, tr("Git Commit"), tr("Enter a commit message."));
        m_message->setFocus();
        return;
    }
    // Whether anything is staged is decided by GitCore::CreateCommit, which
    // looks at the live index - a value cached from the last refresh would be
    // stale right after the user ticked a file.

    const std::string text = message.toStdString();
    Run(tr("Committing..."), [text](GitCore::Repository& repo) {
        OperationResult result;
        result.commitId = QString::fromStdString(GitCore::CreateCommit(repo, text));
        return result;
    });
}

void CommitDialog::Run(const QString& what,
                       std::function<OperationResult (GitCore::Repository&)> operation)
{
    if (m_busy)
        return;

    SetBusy(true, what);
    GitCore::Repository* repo = m_repo;
    m_opWatcher.setFuture(QtConcurrent::run([repo, operation = std::move(operation)] {
        OperationResult result;
        try
        {
            result = operation(*repo);
        }
        catch (const std::exception& e)
        {
            result = OperationResult();
            result.error = QString::fromUtf8(e.what());
        }
        return result;
    }));
}

void CommitDialog::SetBusy(bool busy, const QString& what)
{
    m_busy = busy;
    // Only the message field stays editable: the tree must not be toggled
    // while the index is being rewritten.
    const bool hasSelection = !busy && !m_files->selectedItems().isEmpty();
    m_files->setEnabled(!busy);
    m_refreshButton->setEnabled(!busy);
    m_stageAllButton->setEnabled(!busy);
    m_unstageAllButton->setEnabled(!busy);
    m_stageButton->setEnabled(hasSelection);
    m_unstageButton->setEnabled(hasSelection);
    if (busy)
        m_summaryLabel->setText(what); // Refresh() writes the counts back
    UpdateCommitButton();
}

void CommitDialog::OnOperationFinished()
{
    const OperationResult result = m_opWatcher.result();
    SetBusy(false, QString());

    if (!result.error.isEmpty())
    {
        QMessageBox::warning(this, tr("Git Commit"), result.error);
        Refresh();
        return;
    }

    if (result.commitId.isEmpty())
    {
        Refresh();
        return;
    }

    // TortoiseGit closes the commit dialog once the commit is through.
    CLogging::LogMessage(CLogging::LogLevel::Info,
                         "Created commit " + result.commitId.toStdString());
    accept();
}

void CommitDialog::UpdateCommitButton()
{
    const bool identityOk = !m_identity.name.empty() && !m_identity.email.empty();
    const bool hasMessage = !m_message->toPlainText().trimmed().isEmpty();

    // An empty commit is possible in git but rarely wanted here; the message
    // and the identity are checked here, the staged state only on click,
    // because only GitCore can tell it from the live index.
    m_commitButton->setEnabled(!m_busy && identityOk && hasMessage);
}
