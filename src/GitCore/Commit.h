#pragma once

#include <string>
#include <vector>

#include "GitCore/Repository.h"

namespace GitCore
{
    // Author of a commit. Leave both fields empty to take the identity from
    // the repository or the global git configuration.
    struct Identity
    {
        std::string name;
        std::string email;

        bool empty() const { return name.empty() && email.empty(); }
    };

    // The identity a commit would use. Both fields stay empty when nothing is
    // configured, which is what the dialogs show as a hint to the user.
    Identity DefaultIdentity(const Repository& repo);

    // Moves working tree changes into the index (like `git add`). Without
    // `paths` everything is staged, deletions included; a directory path
    // stages its contents.
    void Stage(Repository& repo, const std::vector<std::string>& paths = {});

    // Puts the index back to HEAD for the given paths without touching the
    // working tree (like `git restore --staged`). Without `paths` the whole
    // index is reset.
    void Unstage(Repository& repo, const std::vector<std::string>& paths = {});

    // True when the index differs from HEAD, i.e. when CreateCommit would
    // record changes instead of an empty commit.
    bool HasStagedChanges(const Repository& repo);

    // Commits the index and moves HEAD to the new commit; returns its hex id.
    // Throws GitException when nothing is staged or when no identity is
    // available (see DefaultIdentity).
    std::string CreateCommit(Repository& repo, const std::string& message,
                             const Identity& author = {});
}
