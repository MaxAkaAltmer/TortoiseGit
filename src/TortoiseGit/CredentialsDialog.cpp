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
#include "CredentialsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMessageBox>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>
#include <utility>

namespace
{
    // A private key next to the ones ssh itself tries first, so the field is
    // rarely empty for the common setups.
    QString DefaultKeyFile()
    {
        const QDir sshDir(QDir::homePath() + QStringLiteral("/.ssh"));
        for (const char* name : {"id_ed25519", "id_ecdsa", "id_rsa", "identity"})
        {
            const QString file = sshDir.filePath(QString::fromLatin1(name));
            if (QFile::exists(file))
                return file;
        }
        return QString();
    }
}

std::vector<CredentialsDialog::Mode>
CredentialsDialog::ModesFor(const GitCore::CredentialRequest& request)
{
    // libgit2 says what it would accept; when it names nothing, offer the type
    // it asked for first.
    std::vector<Mode> modes;
    if (request.allowsUsernamePassword || request.kind == GitCore::CredentialKind::UsernamePassword)
        modes.push_back(Mode::UserPassword);
    if (request.allowsSshKey || request.kind == GitCore::CredentialKind::SshKey)
        modes.push_back(Mode::SshKey);
    if (request.allowsAgent || request.kind == GitCore::CredentialKind::SshAgent)
        modes.push_back(Mode::SshAgent);
    if (request.kind == GitCore::CredentialKind::UsernameOnly)
        modes.push_back(Mode::UserNameOnly);
    if (modes.empty())
        modes.push_back(Mode::UserPassword);
    return modes;
}

CredentialsDialog::Mode CredentialsDialog::ModeOf(GitCore::CredentialKind kind)
{
    switch (kind)
    {
    case GitCore::CredentialKind::SshKey:      return Mode::SshKey;
    case GitCore::CredentialKind::SshAgent:    return Mode::SshAgent;
    case GitCore::CredentialKind::UsernameOnly: return Mode::UserNameOnly;
    case GitCore::CredentialKind::UsernamePassword:
    default:                                   return Mode::UserPassword;
    }
}

CredentialsDialog::CredentialsDialog(QWidget* parent, const GitCore::CredentialRequest& request)
    : QDialog(parent)
    , m_request(request)
{
    setWindowTitle(tr("Git Credentials"));
    setMinimumWidth(420);

    const QString url = QString::fromStdString(request.url).toHtmlEscaped();
    switch (ModeOf(request.kind))
    {
    case Mode::SshKey:
        m_prompt = new QLabel(tr("Enter the passphrase of the SSH key for<br><b>%1</b>").arg(url), this);
        break;
    case Mode::SshAgent:
        m_prompt = new QLabel(tr("The remote <b>%1</b> asks for credentials.<br>"
                                 "They will be taken from ssh-agent.").arg(url), this);
        break;
    case Mode::UserNameOnly:
        m_prompt = new QLabel(tr("Enter the user name for <b>%1</b>").arg(url), this);
        break;
    case Mode::UserPassword:
    default:
        m_prompt = new QLabel(tr("Enter the credentials for <b>%1</b>").arg(url), this);
        break;
    }
    m_prompt->setTextFormat(Qt::RichText);
    m_prompt->setWordWrap(true);

    m_mode = new QComboBox(this);
    for (const Mode mode : ModesFor(request))
    {
        switch (mode)
        {
        case Mode::SshKey:       m_mode->addItem(tr("SSH key file")); break;
        case Mode::SshAgent:     m_mode->addItem(tr("SSH agent")); break;
        case Mode::UserNameOnly: m_mode->addItem(tr("User name only")); break;
        case Mode::UserPassword:
        default:                 m_mode->addItem(tr("User name and password")); break;
        }
        m_mode->setItemData(m_mode->count() - 1, static_cast<int>(mode));
    }
    // The transport accepts one type only in most cases; nothing to choose then.
    m_mode->setVisible(m_mode->count() > 1);

    m_userLabel = new QLabel(tr("User name"), this);
    m_user = new QLineEdit(QString::fromStdString(request.userName), this);

    m_secretLabel = new QLabel(tr("Password or access token"), this);
    m_secret = new QLineEdit(this);
    m_secret->setEchoMode(QLineEdit::Password);

    m_keyLabel = new QLabel(tr("Private key file"), this);
    m_keyFile = new QLineEdit(DefaultKeyFile(), this);
    m_keyFile->setPlaceholderText(QStringLiteral("~/.ssh/id_ed25519"));
    auto* browseButton = new QPushButton(tr("&Browse..."), this);

    auto* keyRow = new QHBoxLayout();
    keyRow->addWidget(m_keyFile, 1);
    keyRow->addWidget(browseButton);
    m_keyRow = new QWidget(this);
    m_keyRow->setLayout(keyRow);

    auto* form = new QFormLayout();
    form->addRow(tr("Credential type"), m_mode);
    form->addRow(m_userLabel, m_user);
    form->addRow(m_secretLabel, m_secret);
    form->addRow(m_keyLabel, m_keyRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_prompt);
    layout->addLayout(form);
    layout->addSpacing(8);
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, [this] {
        const QString start = m_keyFile->text().isEmpty()
                                  ? QDir::homePath() + QStringLiteral("/.ssh")
                                  : QFileInfo(m_keyFile->text()).absolutePath();
        const QString file = QFileDialog::getOpenFileName(this, tr("Private Key File"), start);
        if (!file.isEmpty())
            m_keyFile->setText(file);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &CredentialsDialog::TryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] { UpdateFields(); });

    UpdateFields();
}

CredentialsDialog::Mode CredentialsDialog::CurrentMode() const
{
    return static_cast<Mode>(m_mode->currentData().toInt());
}

void CredentialsDialog::UpdateFields()
{
    const Mode mode = CurrentMode();

    m_userLabel->setVisible(mode != Mode::SshAgent);
    m_user->setVisible(mode != Mode::SshAgent);
    m_secretLabel->setVisible(mode == Mode::UserPassword || mode == Mode::SshKey);
    m_secret->setVisible(mode == Mode::UserPassword || mode == Mode::SshKey);
    m_keyLabel->setVisible(mode == Mode::SshKey);
    m_keyRow->setVisible(mode == Mode::SshKey);

    if (mode == Mode::SshKey)
        m_secretLabel->setText(tr("Key passphrase"));
    else
        m_secretLabel->setText(tr("Password or access token"));

    // A key without a passphrase is common, so an empty passphrase is not an
    // error - unlike an empty password for HTTPS.
    m_secret->setPlaceholderText(mode == Mode::SshKey ? tr("(none)") : QString());

    if (mode == Mode::UserPassword || mode == Mode::UserNameOnly)
        m_user->setFocus();
    else if (mode == Mode::SshKey)
        m_secret->setFocus();
}

void CredentialsDialog::TryAccept()
{
    GitCore::Credential credential;
    credential.userName = m_user->text().trimmed().toStdString();

    switch (CurrentMode())
    {
    case Mode::SshKey:
    {
        const QString keyFile = m_keyFile->text().trimmed();
        if (keyFile.isEmpty() || !QFile::exists(keyFile))
        {
            QMessageBox::warning(this, tr("Git Credentials"),
                                 tr("Select the private key file to use."));
            return;
        }
        credential.kind           = GitCore::CredentialKind::SshKey;
        credential.privateKeyFile = keyFile.toStdString();
        credential.passphrase     = m_secret->text().toStdString();
        break;
    }
    case Mode::SshAgent:
        credential.kind = GitCore::CredentialKind::SshAgent;
        break;
    case Mode::UserNameOnly:
        if (credential.userName.empty())
        {
            QMessageBox::warning(this, tr("Git Credentials"),
                                 tr("Enter the user name to announce to the remote."));
            return;
        }
        credential.kind = GitCore::CredentialKind::UsernameOnly;
        break;
    case Mode::UserPassword:
    default:
        if (m_secret->text().isEmpty())
        {
            QMessageBox::warning(this, tr("Git Credentials"),
                                 tr("Enter the password or access token.\n"
                                    "For GitHub and similar hosts use a personal access token."));
            return;
        }
        credential.kind     = GitCore::CredentialKind::UsernamePassword;
        credential.password = m_secret->text().toStdString();
        break;
    }

    m_credential = std::move(credential);
    accept();
}

CredentialsRequester::CredentialsRequester(QWidget* parentWidget)
    : QObject(parentWidget)
    , m_parentWidget(parentWidget)
{
}

GitCore::CredentialProvider CredentialsRequester::Provider()
{
    return [this](const GitCore::CredentialRequest& request, GitCore::Credential& credential) {
        return Request(request, credential);
    };
}

std::string CredentialsRequester::CacheKey(const GitCore::CredentialRequest& request)
{
    return request.url + '\n' + std::to_string(static_cast<int>(request.kind));
}

bool CredentialsRequester::Request(const GitCore::CredentialRequest& request,
                                   GitCore::Credential& credential)
{
    const std::string key = CacheKey(request);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cancelled)
            return false;
        const auto cached = m_cache.find(key);
        if (cached != m_cache.end())
        {
            credential = cached->second;
            return true;
        }
    }

    // The transfer thread waits here while the user answers. A transfer that
    // runs in the GUI thread (no QtConcurrent involved) asks directly, since
    // BlockingQueuedConnection would deadlock in that case.
    bool answered = false;
    if (QThread::currentThread() == thread())
        answered = Ask(request, credential);
    else
        QMetaObject::invokeMethod(this,
                                  [this, &request, &credential, &answered] {
                                      answered = Ask(request, credential);
                                  },
                                  Qt::BlockingQueuedConnection);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!answered)
    {
        // The user declined: asking again would only repeat the same dialog,
        // so the rest of the transfer has to do without credentials.
        m_cancelled = true;
        return false;
    }
    m_cache[key] = credential;
    return true;
}

bool CredentialsRequester::Ask(const GitCore::CredentialRequest& request,
                               GitCore::Credential& credential)
{
    CredentialsDialog dialog(m_parentWidget, request);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    credential = dialog.Credential();
    return true;
}
