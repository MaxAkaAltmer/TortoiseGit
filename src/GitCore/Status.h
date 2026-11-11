#pragma once

#include <string>
#include <vector>

namespace GitCore
{
    class Repository;

    struct FileStatus
    {
        std::string path;         // workdir-relative, slash separated
        char indexStatus = ' ';   // HEAD -> index:   A/M/D/R/C/T or ' '
        char worktreeStatus = ' ';// index -> workdir: M/D/T/?/X or ' '
    };

    // Collects the working tree status (similar to `git status --porcelain`).
    std::vector<FileStatus> GetStatus(const Repository& repo,
                                      bool includeUntracked = true);
}
