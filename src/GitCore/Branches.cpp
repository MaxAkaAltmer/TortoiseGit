#include "GitCore/Branches.h"

#include <git2.h>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"
#include "GitCore/Repository.h"

namespace GitCore
{
    namespace
    {
        void CollectBranchesOfType(git_repository* raw, git_branch_t type,
                                   bool isRemote, std::vector<BranchInfo>& out)
        {
            git_branch_iterator* it = nullptr;
            int res = git_branch_iterator_new(&it, raw, type);
            if (res != 0)
                GitException::Raise("cannot enumerate branches", res);
            detail::BranchEnumPtr itHolder(it);

            git_reference* ref = nullptr;
            git_branch_t refType = GIT_BRANCH_LOCAL;
            while (git_branch_next(&ref, &refType, itHolder.get()) == 0)
            {
                detail::ReferencePtr refHolder(ref);

                // Skip symbolic pointers such as origin/HEAD - they are
                // aliases, not listable branches.
                if (git_reference_type(refHolder.get()) == GIT_REFERENCE_SYMBOLIC)
                    continue;

                BranchInfo branch;
                branch.isRemote = isRemote;

                const char* name = nullptr;
                if (git_branch_name(&name, refHolder.get()) == 0 && name)
                    branch.name = name;
                else
                    continue;

                branch.isHead = git_branch_is_head(refHolder.get()) == 1;

                git_reference* resolved = nullptr;
                if (git_reference_resolve(&resolved, refHolder.get()) == 0)
                {
                    detail::ReferencePtr resolvedHolder(resolved);
                    const git_oid* target = git_reference_target(resolvedHolder.get());
                    if (target)
                        branch.oid = detail::OidToHex(target);
                }
                else
                {
                    detail::ClearError(); // e.g. broken ref - report without an id
                }

                out.push_back(branch);
            }
            detail::ClearError(); // git_branch_next ends the loop with an error code
        }
    }

    std::vector<BranchInfo> GetBranches(const Repository& repo, bool includeRemote)
    {
        detail::EnsureInit();
        std::vector<BranchInfo> result;
        CollectBranchesOfType(repo.Get(), GIT_BRANCH_LOCAL, false, result);
        if (includeRemote)
            CollectBranchesOfType(repo.Get(), GIT_BRANCH_REMOTE, true, result);
        return result;
    }
}
