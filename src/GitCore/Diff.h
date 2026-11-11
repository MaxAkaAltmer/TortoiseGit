#pragma once

#include <string>
#include <vector>

namespace GitCore
{
    class Repository;

    struct FileDiff
    {
        std::string oldPath;
        std::string newPath;
        char status = ' '; // A/M/D/R/T/C/X
    };

    // Lists files changed between two revisions (see `git diff rev1 rev2`).
    // When `rev2` is empty the changes are computed against the working
    // tree (index + untracked modifications), like `git diff rev1`.
    // `fileFilter` optionally restricts the diff to a single path.
    std::vector<FileDiff> GetChangedFiles(const Repository& repo,
                                          const std::string& rev1,
                                          const std::string& rev2 = {},
                                          const std::string& fileFilter = {});

    // Unified diff text between two revisions (same semantics as
    // GetChangedFiles). Empty string when there are no changes.
    std::string GetUnifiedPatch(const Repository& repo,
                                const std::string& rev1,
                                const std::string& rev2 = {},
                                const std::string& fileFilter = {});
}
