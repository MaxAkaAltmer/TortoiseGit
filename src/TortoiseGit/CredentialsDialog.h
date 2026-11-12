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
#include <QObject>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "GitCore/Credentials.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QWidget;

// Asks for the credentials a remote demands: an HTTPS user name with a
// password or access token, or the passphrase of an SSH key.
//
// Credentials are never stored anywhere. CredentialsRequester below keeps the
// answer in memory for the duration of a single transfer, and libgit2 falls
// back to ssh-agent and the credential helpers when the dialog is cancelled.
class CredentialsDialog : public QDialog
{
    Q_OBJECT

public:
    // How the credentials are supplied; mirrors CredentialKind, but with the
    // order the combo box shows.
    enum class Mode
    {
        UserPassword,
        SshKey,
        SshAgent,
        UserNameOnly,
    };

    CredentialsDialog(QWidget* parent, const GitCore::CredentialRequest& request);

    // The credential built from the fields; meaningful only once accepted.
    const GitCore::Credential& Credential() const { return m_credential; }

private:
    static std::vector<Mode> ModesFor(const GitCore::CredentialRequest& request);
    static Mode ModeOf(GitCore::CredentialKind kind);

    Mode        CurrentMode() const;
    void        UpdateFields();
    void        TryAccept();

    GitCore::CredentialRequest m_request;
    GitCore::Credential        m_credential;

    QLabel*    m_prompt     = nullptr;
    QComboBox* m_mode       = nullptr;
    QLabel*    m_userLabel  = nullptr;
    QLineEdit* m_user       = nullptr;
    QLabel*    m_secretLabel = nullptr;
    QLineEdit* m_secret     = nullptr; // password, token or key passphrase
    QLabel*    m_keyLabel   = nullptr;
    QWidget*   m_keyRow     = nullptr; // key field plus its browse button
    QLineEdit* m_keyFile    = nullptr;
};

// Turns the modal dialog into a GitCore::CredentialProvider.
//
// libgit2 calls the provider from the thread running the transfer, so the
// question is handed to the GUI thread and the worker waits for the answer
// (see the note in GitCore/Credentials.h). Answers are cached per URL and
// credential type, because a single transfer asks again after a redirect or a
// retry; a cancelled dialog stops further questions for the same operation.
class CredentialsRequester : public QObject
{
    Q_OBJECT

public:
    explicit CredentialsRequester(QWidget* parentWidget);

    // Provider for GitCore::RemoteOptions. The transfer has to finish before
    // this object is destroyed.
    GitCore::CredentialProvider Provider();

private:
    bool Request(const GitCore::CredentialRequest& request, GitCore::Credential& credential);
    // Shows the dialog; may only run in the GUI thread.
    bool Ask(const GitCore::CredentialRequest& request, GitCore::Credential& credential);

    static std::string CacheKey(const GitCore::CredentialRequest& request);

    QWidget* m_parentWidget = nullptr;

    std::mutex m_mutex; // guards m_cache and m_cancelled, which the worker touches
    std::map<std::string, GitCore::Credential> m_cache;
    bool m_cancelled = false;
};
