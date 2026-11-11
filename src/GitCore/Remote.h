#pragma once

#include <functional>
#include <string>
#include <vector>

#include "GitCore/Credentials.h"
#include "GitCore/Repository.h"

namespace GitCore
{
    // Options shared by every operation that talks to a remote
    // (clone, fetch, push).
    struct RemoteOptions
    {
        // Human-readable progress lines of the transport.
        std::function<void(const std::string&)> progress;
        // Supplies credentials when the remote requires them. When empty,
        // libgit2 uses its own defaults (anonymous, ssh-agent, helpers).
        CredentialProvider credentials;
        // Name of the remote to use; "origin" unless stated otherwise.
        std::string remoteName = "origin";
    };

    struct RemoteInfo
    {
        std::string name;
        std::string url;    // fetch URL
        std::string pushUrl; // equals url unless a push URL is configured
    };

    // Remotes configured in the repository, in the order libgit2 lists them.
    std::vector<RemoteInfo> ListRemotes(const Repository& repo);

    // Fetch URL of `remoteName`; throws GitException (GIT_ENOTFOUND) when the
    // remote is not configured.
    std::string RemoteUrl(const Repository& repo, const std::string& remoteName = "origin");

    // Downloads objects and refs from the remote. An empty `refspecs` list
    // means the fetch refspec configured for the remote.
    void Fetch(Repository& repo, const std::vector<std::string>& refspecs,
               const RemoteOptions& options = {});

    // Publishes refs to the remote. Refspecs use the git syntax, e.g.
    // "refs/heads/main" or ":refs/heads/obsolete" for a deletion. An empty
    // list pushes the current branch according to push.default.
    void Push(Repository& repo, const std::vector<std::string>& refspecs,
              const RemoteOptions& options = {});
}
