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
#include <QString>
#include <optional>

#include "GitCore/Repository.h"

class QAction;
class QDialog;
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

    // The same repository for the dialogs that write to it (commit, push,
    // pull); nullptr when none is open.
    GitCore::Repository* MutableRepo();

    // Runs the dialog belonging to `action` ("log", "status", "diff", ...)
    // modally. Returns false for unknown actions - the caller then shows the
    // main window instead. Used for the `--action` command line mode.
    bool ExecAction(const QString& action, const QString& revision = QString());

    // Handles a command forwarded by a second instance: opens `path` when it
    // is given, brings the window to the front and shows the dialog of
    // `action` non-modally. An empty `action` only activates the window.
    void HandleCommand(const QString& action, const QString& path,
                       const QString& url, const QString& revision);

private slots:
    void ChooseAndOpenRepository();
    void ShowLogDialog();
    void ShowStatusDialog();
    void ShowDiffDialog();
    void ShowCommitDialog();
    void ShowPushDialog();
    void ShowPullDialog();
    void ShowSettingsDialog();
    void ShowAboutDialog();

private:
    void UpdateUiForRepo();
    // Creates the owned dialog for `action` (parented to this window), or
    // nullptr when the action is not implemented.
    QDialog* CreateActionDialog(const QString& action, const QString& revision);

    std::optional<GitCore::Repository> m_repo;
    QLabel* m_hintLabel       = nullptr;
    QLabel* m_repoStatusLabel = nullptr;
    QAction* m_closeAction  = nullptr;
    QAction* m_logAction    = nullptr;
    QAction* m_statusAction = nullptr;
    QAction* m_diffAction   = nullptr;
    QAction* m_commitAction = nullptr;
    QAction* m_pushAction   = nullptr;
    QAction* m_pullAction   = nullptr;
};
