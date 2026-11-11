#pragma once

#include <functional>
#include <string>

namespace GitCore
{
    // A credential the caller is able to provide. Mirrors the subset of
    // libgit2's git_credential_t that the port supports.
    enum class CredentialKind
    {
        UsernamePassword, // https: user name + password or access token
        SshKey,           // ssh: private key file, optionally with a passphrase
        SshAgent,         // ssh: delegate to ssh-agent / the default credential
        UsernameOnly,     // only announce the user name, the rest is automatic
    };

    // What the transport asks for. `kind` is the type libgit2 would like to
    // receive first; the `allows*` flags list everything it would accept.
    struct CredentialRequest
    {
        std::string url;
        std::string userName; // user name parsed from the URL, may be empty
        CredentialKind kind = CredentialKind::UsernamePassword;
        bool allowsUsernamePassword = false;
        bool allowsSshKey = false;
        bool allowsAgent = false;
    };

    // The credential returned in response to a CredentialRequest.
    struct Credential
    {
        CredentialKind kind = CredentialKind::UsernamePassword;
        std::string userName;
        std::string password; // or a personal access token for https
        std::string privateKeyFile;
        std::string publicKeyFile; // defaults to privateKeyFile + ".pub"
        std::string passphrase;

        // Builds a credential from command line / configuration style values.
        static Credential FromUserPassword(const std::string& userName, const std::string& password);
        static Credential FromSshKey(const std::string& userName, const std::string& privateKeyFile,
                                     const std::string& passphrase = std::string());
        static Credential FromAgent(const std::string& userName = std::string());
    };

    // Supplies credentials for clone/fetch/push/pull. Returning false lets
    // libgit2 try its own sources (ssh-agent, credential helpers, netrc).
    // The provider may be called from a worker thread, but never from the
    // GUI thread - see TortoiseGit/CredentialsDialog.h for the Qt bridge.
    using CredentialProvider = std::function<bool (const CredentialRequest&, Credential&)>;

    // Transport support compiled into the libgit2 actually linked in.
    bool SupportsHttps();
    bool SupportsSsh();

    // True when the library can speak the transport `url` requests. Local
    // paths, file:// and git:// are always considered supported.
    bool IsTransportSupported(const std::string& url);

    // Explains which backend has to be enabled to use `url` ("" when the
    // transport is available). Used in error messages of the GUI and the CLI.
    std::string MissingTransportHint(const std::string& url);
}
