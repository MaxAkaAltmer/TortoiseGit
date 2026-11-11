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

#include <QMainWindow>
#include <optional>

#include "GitCore/Repository.h"

class QAction;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Opens the repository at `path` (or in the closest parent directory).
    // Shows an error dialog and returns false when this fails.
    bool OpenRepository(const QString& path);
    void CloseRepository();

    // The repository opened in this window, nullptr when none is open.
    const GitCore::Repository* Repo() const;

private slots:
    void ChooseAndOpenRepository();
    void ShowLogDialog();
    void ShowStatusDialog();
    void ShowDiffDialog();
    void ShowSettingsDialog();
    void ShowAboutDialog();

private:
    void UpdateUiForRepo();

    std::optional<GitCore::Repository> m_repo;
    QLabel* m_hintLabel       = nullptr;
    QLabel* m_repoStatusLabel = nullptr;
    QAction* m_closeAction  = nullptr;
    QAction* m_logAction    = nullptr;
    QAction* m_statusAction = nullptr;
    QAction* m_diffAction   = nullptr;
};
