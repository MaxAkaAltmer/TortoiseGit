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
#include "LogDialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include "AppSettings.h"
#include "DiffDialog.h"
#include "GitCore/GitException.h"

namespace
{
    QString ShortOid(const std::string& id)
    {
        return QString::fromStdString(id).left(7);
    }

    QString FormatTime(std::int64_t secondsSinceEpoch)
    {
        return QDateTime::fromSecsSinceEpoch(secondsSinceEpoch).toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    }

    QTableWidgetItem* MakeItem(const QString& text)
    {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return item;
    }
}

LogDialog::LogDialog(const GitCore::Repository* repo, QWidget* parent)
    : QDialog(parent)
    , m_repo(repo)
{
    setWindowTitle(tr("Git Log"));
    resize(900, 600);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(tr("Show:"), this));
    m_limitCombo = new QComboBox(this);
    m_limitCombo->addItem(QStringLiteral("50"), 50);
    m_limitCombo->addItem(QStringLiteral("100"), 100);
    m_limitCombo->addItem(QStringLiteral("250"), 250);
    m_limitCombo->addItem(QStringLiteral("1000"), 1000);
    m_limitCombo->addItem(tr("All"), 0);
    const int savedLimit = AppSettings::LogLimit();
    const int limitIndex = m_limitCombo->findData(savedLimit);
    m_limitCombo->setCurrentIndex(limitIndex >= 0 ? limitIndex : 1); // default: 100
    topRow->addWidget(m_limitCombo);

    m_reloadButton = new QPushButton(tr("&Reload"), this);
    topRow->addWidget(m_reloadButton);
    topRow->addStretch();
    m_countLabel = new QLabel(this);
    topRow->addWidget(m_countLabel);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("Hash"), tr("Date"), tr("Author"), tr("Message")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_table);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(splitter);

    connect(m_limitCombo, &QComboBox::activated, this, &LogDialog::Reload);
    connect(m_reloadButton, &QPushButton::clicked, this, &LogDialog::Reload);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &LogDialog::OnCurrentRowChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &LogDialog::OnCommitActivated);
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &LogDialog::OnLoadFinished);

    Reload();
}

LogDialog::~LogDialog()
{
    // The worker reads from the repository owned by MainWindow; make sure it
    // is done before this dialog (and possibly the window) goes away.
    if (m_watcher.isRunning())
        m_watcher.future().waitForFinished();
}

void LogDialog::Reload()
{
    if (m_watcher.isRunning())
        return;
    const std::size_t limit = static_cast<std::size_t>(m_limitCombo->currentData().toInt());
    const GitCore::Repository* repo = m_repo;
    m_reloadButton->setEnabled(false);
    m_countLabel->setText(tr("loading..."));
    m_watcher.setFuture(QtConcurrent::run(
        [repo, limit] { return GitCore::GetLog(*repo, {}, limit, 0, {}); }));
}

void LogDialog::OnLoadFinished()
{
    m_reloadButton->setEnabled(true);

    std::vector<GitCore::CommitInfo> commits;
    try
    {
        commits = m_watcher.result();
    }
    catch (const GitCore::GitException& e)
    {
        m_countLabel->clear();
        QMessageBox::warning(this, tr("Git Log"),
                             tr("Failed to load the log:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }

    m_commits = std::move(commits);
    m_table->setRowCount(static_cast<int>(m_commits.size()));
    for (int row = 0; row < static_cast<int>(m_commits.size()); ++row)
    {
        const auto& commit = m_commits[static_cast<std::size_t>(row)];
        m_table->setItem(row, 0, MakeItem(ShortOid(commit.id)));
        m_table->setItem(row, 1, MakeItem(FormatTime(commit.authorTime)));
        m_table->setItem(row, 2, MakeItem(QString::fromStdString(commit.authorName)));
        m_table->setItem(row, 3, MakeItem(QString::fromStdString(commit.summary)));
    }
    m_countLabel->setText(tr("%n commit(s)", "", static_cast<int>(m_commits.size())));

    if (!m_commits.empty())
    {
        m_table->selectRow(0);
        OnCurrentRowChanged();
    }
    else
    {
        m_details->setPlainText(tr("No commits found."));
    }
}

void LogDialog::OnCurrentRowChanged()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_commits.size()))
        return;
    const auto& commit = m_commits[static_cast<std::size_t>(row)];

    QString details;
    details += tr("Commit:  %1\n").arg(QString::fromStdString(commit.id));
    details += tr("Author:  %1 <%2>  (%3, UTC%4%5)\n")
                   .arg(QString::fromStdString(commit.authorName),
                        QString::fromStdString(commit.authorEmail),
                        FormatTime(commit.authorTime),
                        commit.authorOffsetMinutes >= 0 ? QStringLiteral("+") : QStringLiteral("-"),
                        QStringLiteral("%1:%2")
                            .arg(std::abs(commit.authorOffsetMinutes) / 60, 2, 10, QLatin1Char('0'))
                            .arg(std::abs(commit.authorOffsetMinutes) % 60, 2, 10, QLatin1Char('0')));
    details += tr("Committer: %1 <%2>  (%3)\n")
                   .arg(QString::fromStdString(commit.committerName),
                        QString::fromStdString(commit.committerEmail),
                        FormatTime(commit.commitTime));
    if (!commit.parentIds.empty())
    {
        QStringList parents;
        for (const auto& parent : commit.parentIds)
            parents << ShortOid(parent);
        details += tr("Parents: %1\n").arg(parents.join(QStringLiteral(", ")));
    }
    details += QStringLiteral("\n");
    details += QString::fromStdString(commit.message);
    m_details->setPlainText(details);
}

void LogDialog::OnCommitActivated()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_commits.size()))
        return;
    const auto& commit = m_commits[static_cast<std::size_t>(row)];

    if (commit.parentIds.empty())
    {
        QMessageBox::information(this, tr("Git Log"),
                                 tr("The initial commit has no parent - showing a diff "
                                    "against an empty tree is not supported yet."));
        return;
    }

    DiffDialog dialog(m_repo, QString::fromStdString(commit.parentIds.front()),
                      QString::fromStdString(commit.id), this);
    dialog.exec();
}
