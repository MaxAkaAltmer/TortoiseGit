#include "GitCore/Credentials.h"

#include <git2.h>

#include <cstddef>

#include "GitCore/CredentialsInternal.h"
#include "GitCore/GitCoreUtil.h"

namespace GitCore
{
    namespace
    {
        // "https://host/x" -> "https"; "" for local paths.
        std::string SchemeOf(const std::string& url)
        {
            const auto separator = url.find("://");
            if (separator == std::string::npos || separator == 0)
                return {};
            // A '/' before the separator means a path, not a scheme.
            if (url.find('/') < separator)
                return {};
            return url.substr(0, separator);
        }

        // The scp-like syntax accepted by git: "git@host:path", without "://".
        bool IsScpLikeUrl(const std::string& url)
        {
            if (url.find("://") != std::string::npos)
                return false;
            const auto colon = url.find(':');
            if (colon == std::string::npos || colon == 0)
                return false;
            const auto slash = url.find('/');
            if (slash != std::string::npos && slash < colon)
                return false; // an ordinary unix path
            // "C:/dir" is a Windows drive, not a host name.
            if (colon == 1 && url.find('@') == std::string::npos)
                return false;
            return true;
        }

        std::string LastErrorText()
        {
            const git_error* error = git_error_last();
            if (error && error->message)
                return detail::TrimRight(error->message);
            return {};
        }

        CredentialKind PreferredKind(unsigned int allowedTypes)
        {
            if (allowedTypes & GIT_CREDENTIAL_USERPASS_PLAINTEXT)
                return CredentialKind::UsernamePassword;
            if (allowedTypes & (GIT_CREDENTIAL_SSH_KEY | GIT_CREDENTIAL_SSH_MEMORY))
                return CredentialKind::SshKey;
            if (allowedTypes & GIT_CREDENTIAL_DEFAULT)
                return CredentialKind::SshAgent;
            return CredentialKind::UsernameOnly;
        }

        // Creates the libgit2 credential described by `credential`, or leaves
        // *out empty and returns GIT_PASSTHROUGH when the type is unwanted.
        int CreateCredential(git_credential** out, const Credential& credential,
                             const CredentialRequest& request, std::string& errorText)
        {
            const std::string userName =
                !credential.userName.empty() ? credential.userName : request.userName;

            switch (credential.kind)
            {
            case CredentialKind::UsernamePassword:
                if (userName.empty())
                {
                    errorText = "no user name for '" + request.url + "'";
                    return GIT_EUSER;
                }
                if (git_credential_userpass_plaintext_new(out, userName.c_str(),
                                                          credential.password.c_str()) != 0)
                {
                    errorText = LastErrorText();
                    return GIT_EUSER;
                }
                return 0;

            case CredentialKind::SshKey:
            {
                if (userName.empty())
                {
                    errorText = "no user name for '" + request.url + "'";
                    return GIT_EUSER;
                }
                if (credential.privateKeyFile.empty())
                {
                    errorText = "no private key file selected";
                    return GIT_EUSER;
                }
                std::string publicKey = credential.publicKeyFile;
                if (publicKey.empty())
                    publicKey = credential.privateKeyFile + ".pub";
                if (git_credential_ssh_key_new(out, userName.c_str(), publicKey.c_str(),
                                               credential.privateKeyFile.c_str(),
                                               credential.passphrase.empty() ? nullptr
                                                                             : credential.passphrase.c_str()) != 0)
                {
                    errorText = LastErrorText();
                    return GIT_EUSER;
                }
                return 0;
            }

            case CredentialKind::SshAgent:
                if (git_credential_default_new(out) != 0)
                {
                    errorText = LastErrorText();
                    return GIT_EUSER;
                }
                return 0;

            case CredentialKind::UsernameOnly:
                if (userName.empty())
                {
                    errorText = "no user name for '" + request.url + "'";
                    return GIT_EUSER;
                }
                if (git_credential_username_new(out, userName.c_str()) != 0)
                {
                    errorText = LastErrorText();
                    return GIT_EUSER;
                }
                return 0;
            }
            return GIT_PASSTHROUGH;
        }

        int SidebandProgress(const char* text, int len, void* payload)
        {
            auto* remote = static_cast<detail::RemoteCallbackPayload*>(payload);
            if (remote && remote->progress && text && len > 0)
                remote->progress(std::string(text, static_cast<std::size_t>(len)));
            return 0;
        }

        int CredentialCallback(git_credential** out, const char* url, const char* userNameFromUrl,
                               unsigned int allowedTypes, void* payload)
        {
            auto* remote = static_cast<detail::RemoteCallbackPayload*>(payload);
            *out = nullptr;
            if (!remote || !remote->credentials.provider)
                return GIT_PASSTHROUGH;
            auto* context = &remote->credentials;

            CredentialRequest request;
            request.url                   = url ? url : "";
            request.userName              = userNameFromUrl ? userNameFromUrl : "";
            request.allowsUsernamePassword = (allowedTypes & GIT_CREDENTIAL_USERPASS_PLAINTEXT) != 0;
            request.allowsSshKey = (allowedTypes & (GIT_CREDENTIAL_SSH_KEY | GIT_CREDENTIAL_SSH_MEMORY)) != 0;
            request.allowsAgent            = (allowedTypes & GIT_CREDENTIAL_DEFAULT) != 0;
            request.kind                   = PreferredKind(allowedTypes);

            Credential credential;
            if (!context->provider(request, credential))
                return GIT_PASSTHROUGH; // libgit2 falls back to its own sources

            return CreateCredential(out, credential, request, context->errorText);
        }
    }

    Credential Credential::FromUserPassword(const std::string& userName, const std::string& password)
    {
        Credential credential;
        credential.kind     = CredentialKind::UsernamePassword;
        credential.userName = userName;
        credential.password = password;
        return credential;
    }

    Credential Credential::FromSshKey(const std::string& userName, const std::string& privateKeyFile,
                                      const std::string& passphrase)
    {
        Credential credential;
        credential.kind           = CredentialKind::SshKey;
        credential.userName       = userName;
        credential.privateKeyFile = privateKeyFile;
        credential.passphrase     = passphrase;
        return credential;
    }

    Credential Credential::FromAgent(const std::string& userName)
    {
        Credential credential;
        credential.kind     = CredentialKind::SshAgent;
        credential.userName = userName;
        return credential;
    }

    bool SupportsHttps()
    {
        detail::EnsureInit();
        return (git_libgit2_features() & GIT_FEATURE_HTTPS) != 0;
    }

    bool SupportsSsh()
    {
        detail::EnsureInit();
        return (git_libgit2_features() & GIT_FEATURE_SSH) != 0;
    }

    bool IsTransportSupported(const std::string& url)
    {
        if (IsScpLikeUrl(url))
            return SupportsSsh();

        const std::string scheme = SchemeOf(url);
        if (scheme == "https")
            return SupportsHttps();
        if (scheme == "ssh" || scheme == "git+ssh")
            return SupportsSsh();
        // http://, git://, file:// and plain paths need no compiled backend.
        return true;
    }

    std::string MissingTransportHint(const std::string& url)
    {
        if (IsTransportSupported(url))
            return {};

        if (SchemeOf(url) == "https")
            return "this build cannot use https:// remotes - libgit2 was built "
                   "without an HTTPS backend (install the OpenSSL development package and "
                   "configure with TGIT_WITH_HTTPS=ON)";
        return "this build cannot use ssh remotes - libgit2 was built without an SSH "
               "backend (install the libssh2 development package and configure with "
               "TGIT_WITH_SSH=LIBSSH2, or use TGIT_WITH_SSH=EXEC to call the ssh client)";
    }

    namespace detail
    {
        void InstallRemoteCallbacks(git_remote_callbacks& callbacks, RemoteCallbackPayload& payload)
        {
            if (payload.progress)
                callbacks.sideband_progress = SidebandProgress;
            if (payload.credentials.provider)
                callbacks.credentials = CredentialCallback;
            if (!callbacks.sideband_progress && !callbacks.credentials)
                return;
            // A single payload backs both callbacks (see RemoteCallbackPayload).
            callbacks.payload = &payload;
        }
    }
}
