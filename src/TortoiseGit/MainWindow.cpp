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
#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <memory>

#include "AppSettings.h"
#include "CrossPlatform/Logging.h"
#include "DiffDialog.h"
#include "GitCore/GitException.h"
#include "LogDialog.h"
#include "SettingsDialog.h"
#include "StatusDialog.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("TortoiseGit"));
    resize(900, 600);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setTextFormat(Qt::RichText);
    m_hintLabel->setWordWrap(true);
    setCentralWidget(m_hintLabel);

    m_repoStatusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_repoStatusLabel);

    // Note: QAction* are created explicitly (instead of the convenience
    // overloads of QMenu::addAction with a shortcut) to stay compatible
    // with Qt 6.2, where those overloads do not exist yet.
    auto* openAction = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                   tr("&Open Repository..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::ChooseAndOpenRepository);

    m_closeAction = new QAction(tr("&Close Repository"), this);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::CloseRepository);

    auto* settingsAction = new QAction(tr("&Settings..."), this);
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::ShowSettingsDialog);

    auto* exitAction = new QAction(style()->standardIcon(QStyle::SP_DialogCloseButton),
                                   tr("E&xit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(m_closeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(settingsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    m_logAction = new QAction(tr("Show &Log"), this);
    m_logAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    connect(m_logAction, &QAction::triggered, this, &MainWindow::ShowLogDialog);

    m_statusAction = new QAction(tr("Show &Status"), this);
    m_statusAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(m_statusAction, &QAction::triggered, this, &MainWindow::ShowStatusDialog);

    m_diffAction = new QAction(tr("Show &Diff"), this);
    m_diffAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(m_diffAction, &QAction::triggered, this, &MainWindow::ShowDiffDialog);

    auto* gitMenu = menuBar()->addMenu(tr("&Git"));
    gitMenu->addAction(m_logAction);
    gitMenu->addAction(m_statusAction);
    gitMenu->addAction(m_diffAction);

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About TortoiseGit"), this, &MainWindow::ShowAboutDialog);

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(openAction);
    toolbar->addAction(m_logAction);
    toolbar->addAction(m_statusAction);
    toolbar->addAction(m_diffAction);

    UpdateUiForRepo();
}

bool MainWindow::OpenRepository(const QString& path)
{
    try
    {
        m_repo = GitCore::Repository::Open(QDir(path).absolutePath().toStdString());
    }
    catch (const GitCore::GitException& e)
    {
        QString message = QString::fromUtf8(e.what());
        // libgit2 reports "owned by another user" for safe.directory problems
        if (message.contains(QStringLiteral("owned by another")))
            message += QStringLiteral("\n\nTip: run `tgit-cli trust \"%1\"` to mark this "
                                      "repository as safe.").arg(path);
        QMessageBox::warning(this, tr("Open Repository"),
                             tr("Cannot open '%1':\n%2").arg(path, message));
        return false;
    }
    UpdateUiForRepo();
    return true;
}

void MainWindow::CloseRepository()
{
    m_repo.reset();
    UpdateUiForRepo();
}

const GitCore::Repository* MainWindow::Repo() const
{
    return m_repo ? &*m_repo : nullptr;
}

void MainWindow::ChooseAndOpenRepository()
{
    QString startDir = QString::fromStdString(AppSettings::DefaultRepoPath());
    if (startDir.isEmpty() || !QDir(startDir).exists())
        startDir = QDir::homePath();

    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Open Repository"), startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty())
        OpenRepository(dir);
}

void MainWindow::ShowLogDialog()
{
    ExecAction(QStringLiteral("log"));
}

void MainWindow::ShowStatusDialog()
{
    ExecAction(QStringLiteral("status"));
}

void MainWindow::ShowDiffDialog()
{
    ExecAction(QStringLiteral("diff"));
}

QDialog* MainWindow::CreateActionDialog(const QString& action, const QString& revision)
{
    if (!Repo())
        return nullptr;

    if (action == QLatin1String("log"))
        return new LogDialog(Repo(), this);
    if (action == QLatin1String("status"))
        return new StatusDialog(Repo(), this);
    if (action == QLatin1String("diff"))
    {
        // HEAD is the default "from" side; an explicit --revision overrides it
        const QString rev1 = revision.isEmpty() ? QStringLiteral("HEAD") : revision;
        return new DiffDialog(Repo(), rev1, QString(), this);
    }
    return nullptr;
}

bool MainWindow::ExecAction(const QString& action, const QString& revision)
{
    std::unique_ptr<QDialog> dialog(CreateActionDialog(action, revision));
    if (!dialog)
        return false;
    dialog->exec();
    return true;
}

void MainWindow::HandleCommand(const QString& action, const QString& path,
                               const QString& url, const QString& revision)
{
    CLogging::LogMessage(CLogging::LogLevel::Info,
                         "Handling command '" + action.toStdString() + "' for path '" +
                             path.toStdString() + "'");

    if (!path.isEmpty() && !OpenRepository(path))
        return;

    if (!url.isEmpty())
        CLogging::LogMessage(CLogging::LogLevel::Debug,
                             "No action uses a url yet, ignoring '" + url.toStdString() + "'");

    show();
    raise();
    activateWindow();

    if (action.isEmpty())
        return;

    QDialog* dialog = CreateActionDialog(action, revision);
    if (!dialog)
    {
        CLogging::LogMessage(CLogging::LogLevel::Warning,
                             "Unknown action '" + action.toStdString() + "'");
        return;
    }
    // Commands from other instances open their dialog modelessly
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::ShowSettingsDialog()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::ShowAboutDialog()
{
    QMessageBox::about(this, tr("About TortoiseGit"),
                       tr("<b>TortoiseGit %1</b><br>"
                          "Linux GUI for Git (port of TortoiseGit on top of libgit2 and Qt).<br><br>"
                          "This is an early development preview: the Log, Status and Diff "
                          "dialogs are functional; commit, push/pull and shell integration "
                          "are planned.")
                           .arg(QApplication::applicationVersion()));
}

void MainWindow::UpdateUiForRepo()
{
    const bool hasRepo = Repo() != nullptr;
    m_closeAction->setEnabled(hasRepo);
    m_logAction->setEnabled(hasRepo);
    m_statusAction->setEnabled(hasRepo);
    m_diffAction->setEnabled(hasRepo);

    if (!hasRepo)
    {
        setWindowTitle(QStringLiteral("TortoiseGit"));
        m_repoStatusLabel->clear();
        m_hintLabel->setText(tr("<h3>No repository open</h3>"
                                "Use <i>File &rarr; Open Repository...</i> (Ctrl+O) to open "
                                "a working copy."));
        return;
    }

    const auto& info = m_repo->Info();
    const QString workDir = QString::fromStdString(info.workDir);
    QString repoName = QDir(workDir.isEmpty() ? QString::fromStdString(info.gitDir) : workDir).dirName();
    setWindowTitle(QStringLiteral("TortoiseGit - %1").arg(repoName));

    QString branch;
    if (info.isHeadDetached)
        branch = tr("(detached HEAD)");
    else if (!info.headBranch.empty())
        branch = QString::fromStdString(info.headBranch);
    else
        branch = tr("(no commits yet)");

    m_repoStatusLabel->setText(QStringLiteral("%1 [%2]").arg(QDir::toNativeSeparators(workDir), branch));

    m_hintLabel->setText(tr("<h3>%1</h3>"
                            "<p>%2</p>"
                            "<p>Branch: <b>%3</b>%4</p>"
                            "<p>Use the <i>Git</i> menu to show the commit log, the working "
                            "tree status or the current diff.</p>")
                             .arg(repoName.toHtmlEscaped(),
                                  QDir::toNativeSeparators(workDir).toHtmlEscaped(),
                                  branch.toHtmlEscaped(),
                                  info.isBare ? QStringLiteral(" <i>(bare)</i>") : QString()));
}
