#pragma once

#include <string>
#include <vector>

namespace GitCore
{
    class Repository;

    struct BranchInfo
    {
        std::string name;   // "main" for local, "origin/main" for remote branches
        std::string oid;    // hex id of the tip commit, empty if unresolvable
        bool isHead = false;
        bool isRemote = false;
    };

    std::vector<BranchInfo> GetBranches(const Repository& repo,
                                        bool includeRemote = true);
}
