#pragma once

// Internal helper shared by the gitcore files that talk to remotes.
// Not a part of the public gitcore API.

#include <git2.h>

#include <functional>
#include <string>

#include "GitCore/Credentials.h"

namespace GitCore
{
    namespace detail
    {
        // State behind the libgit2 credentials callback.
        struct CredentialCallbackContext
        {
            CredentialProvider provider;
            // Filled in when credential creation fails or when the caller
            // reported a problem; appended to the exception message.
            std::string errorText;
        };

        // libgit2 passes one payload pointer to every callback of
        // git_remote_callbacks, so transport progress and credentials share it.
        struct RemoteCallbackPayload
        {
            std::function<void(const std::string&)> progress;
            CredentialCallbackContext credentials;
        };

        // Installs the callbacks backed by `payload`. Callbacks whose data is
        // empty are left unset, so libgit2 keeps its own defaults (anonymous
        // access, ssh-agent, credential helper programs).
        void InstallRemoteCallbacks(git_remote_callbacks& callbacks, RemoteCallbackPayload& payload);
    }
}
