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
#include <vector>

#include "GitCore/Diff.h"

class DiffViewerWidget;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace GitCore
{
    class Repository;
}

// Shows the files changed between two revisions (GitCore::GetChangedFiles)
// and the unified patch of the selected file (GitCore::GetUnifiedPatch).
// An empty second revision means "working tree" (like `git diff rev1`).
class DiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiffDialog(const GitCore::Repository* repo,
                        const QString& rev1 = QStringLiteral("HEAD"),
                        const QString& rev2 = QString(),
                        QWidget* parent = nullptr);
    ~DiffDialog() override;

private slots:
    void Reload();
    void OnLoadFinished();
    void OnFileSelected();

private:
    QString DiffPath(const GitCore::FileDiff& file) const;

    const GitCore::Repository* m_repo;
    // revisions actually used by the last (re)load, used to fetch patches
    QString m_activeRev1;
    QString m_activeRev2;
    QLineEdit* m_rev1Edit     = nullptr;
    QLineEdit* m_rev2Edit     = nullptr;
    QPushButton* m_reloadButton = nullptr;
    QListWidget* m_filesList  = nullptr;
    DiffViewerWidget* m_patchView = nullptr;
    QFutureWatcher<std::vector<GitCore::FileDiff>> m_watcher;
};
