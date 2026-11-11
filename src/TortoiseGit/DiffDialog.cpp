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
#include "DiffDialog.h"

#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include "DiffViewerWidget.h"
#include "GitCore/GitException.h"

namespace
{
    constexpr int RoleOldPath = Qt::UserRole;
    constexpr int RoleNewPath = Qt::UserRole + 1;
}

DiffDialog::DiffDialog(const GitCore::Repository* repo,
                       const QString& rev1, const QString& rev2, QWidget* parent)
    : QDialog(parent)
    , m_repo(repo)
{
    setWindowTitle(tr("Diff"));
    resize(1000, 650);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(tr("From:"), this));
    m_rev1Edit = new QLineEdit(rev1, this);
    m_rev1Edit->setPlaceholderText(QStringLiteral("HEAD"));
    topRow->addWidget(m_rev1Edit);
    topRow->addWidget(new QLabel(tr("To:"), this));
    m_rev2Edit = new QLineEdit(rev2, this);
    m_rev2Edit->setPlaceholderText(tr("(working tree)"));
    topRow->addWidget(m_rev2Edit);
    m_reloadButton = new QPushButton(tr("&Reload"), this);
    topRow->addWidget(m_reloadButton);

    m_filesList = new QListWidget(this);
    m_filesList->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_patchView = new DiffViewerWidget(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_filesList);
    splitter->addWidget(m_patchView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(splitter);

    connect(m_reloadButton, &QPushButton::clicked, this, &DiffDialog::Reload);
    connect(m_rev1Edit, &QLineEdit::returnPressed, this, &DiffDialog::Reload);
    connect(m_rev2Edit, &QLineEdit::returnPressed, this, &DiffDialog::Reload);
    connect(m_filesList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem*, QListWidgetItem*) { OnFileSelected(); });
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &DiffDialog::OnLoadFinished);

    Reload();
}

DiffDialog::~DiffDialog()
{
    // The worker reads from the repository owned by MainWindow - do not let
    // it outlive this dialog.
    if (m_watcher.isRunning())
        m_watcher.future().waitForFinished();
}

void DiffDialog::Reload()
{
    if (m_watcher.isRunning())
        return;

    const QString rev1 = m_rev1Edit->text().trimmed();
    if (rev1.isEmpty())
    {
        QMessageBox::warning(this, tr("Diff"), tr("The first revision must not be empty."));
        return;
    }
    m_activeRev1 = rev1;
    m_activeRev2 = m_rev2Edit->text().trimmed();

    setWindowTitle(m_activeRev2.isEmpty()
                       ? tr("Diff - %1 vs working tree").arg(m_activeRev1)
                       : tr("Diff - %1 vs %2").arg(m_activeRev1, m_activeRev2));

    const GitCore::Repository* repo = m_repo;
    const std::string from = m_activeRev1.toStdString();
    const std::string to = m_activeRev2.toStdString();
    m_reloadButton->setEnabled(false);
    m_filesList->clear();
    m_patchView->Clear();
    m_watcher.setFuture(QtConcurrent::run(
        [repo, from, to] { return GitCore::GetChangedFiles(*repo, from, to, {}); }));
}

void DiffDialog::OnLoadFinished()
{
    m_reloadButton->setEnabled(true);

    std::vector<GitCore::FileDiff> files;
    try
    {
        files = m_watcher.result();
    }
    catch (const GitCore::GitException& e)
    {
        QMessageBox::warning(this, tr("Diff"),
                             tr("Failed to compute the diff:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }

    for (const auto& file : files)
    {
        const QString oldPath = QString::fromStdString(file.oldPath);
        const QString newPath = QString::fromStdString(file.newPath);
        QString text = QStringLiteral("%1  ").arg(QChar(file.status == ' ' ? '-' : file.status));
        text += (file.status == 'R' && !oldPath.isEmpty() && !newPath.isEmpty())
                    ? tr("%1 -> %2").arg(oldPath, newPath)
                    : (newPath.isEmpty() ? oldPath : newPath);

        auto* item = new QListWidgetItem(text, m_filesList);
        item->setData(RoleOldPath, oldPath);
        item->setData(RoleNewPath, newPath);
    }

    if (files.empty())
        m_patchView->SetPatch(tr("No changes."));
}

void DiffDialog::OnFileSelected()
{
    auto* item = m_filesList->currentItem();
    if (!item)
    {
        m_patchView->Clear();
        return;
    }

    GitCore::FileDiff file;
    file.oldPath = item->data(RoleOldPath).toString().toStdString();
    file.newPath = item->data(RoleNewPath).toString().toStdString();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    try
    {
        const std::string patch = GitCore::GetUnifiedPatch(*m_repo,
                                                           m_activeRev1.toStdString(),
                                                           m_activeRev2.toStdString(),
                                                           DiffPath(file).toStdString());
        m_patchView->SetPatch(patch.empty()
                                  ? tr("(no textual changes)")
                                  : QString::fromUtf8(patch.data(),
                                                      static_cast<qint64>(patch.size())));
    }
    catch (const GitCore::GitException& e)
    {
        m_patchView->SetPatch(QString::fromUtf8(e.what()));
    }
    QApplication::restoreOverrideCursor();
}

QString DiffDialog::DiffPath(const GitCore::FileDiff& file) const
{
    // deleted files only have an old path, added ones only a new path
    return file.newPath.empty() ? QString::fromStdString(file.oldPath)
                                : QString::fromStdString(file.newPath);
}
