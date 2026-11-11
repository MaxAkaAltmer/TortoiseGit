#include "GitCore/Commit.h"

#include <git2.h>

#include <cstddef>
#include <vector>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"

namespace GitCore
{
    namespace
    {
        detail::IndexPtr OpenIndex(git_repository* repo)
        {
            git_index* raw = nullptr;
            const int res = git_repository_index(&raw, repo);
            if (res != 0)
                GitException::Raise("cannot open the index", res);
            return detail::IndexPtr(raw);
        }

        void WriteIndex(git_index* index)
        {
            const int res = git_index_write(index);
            if (res != 0)
                GitException::Raise("cannot save the index", res);
        }

        // HEAD as a commit object; an empty pointer means an unborn branch.
        detail::ObjectPtr HeadCommitObject(git_repository* repo, bool* unborn)
        {
            detail::ReferencePtr head;
            const int res = git_repository_head(head.releaseSlot(), repo);
            if (res == GIT_EUNBORNBRANCH || res == GIT_ENOTFOUND)
            {
                detail::ClearError();
                *unborn = true;
                return {};
            }
            if (res != 0)
                GitException::Raise("cannot read HEAD", res);

            *unborn = false;
            git_object* raw = nullptr;
            const int lookup = git_object_lookup(&raw, repo, git_reference_target(head.get()),
                                                 GIT_OBJECT_COMMIT);
            if (lookup != 0)
                GitException::Raise("cannot read the commit HEAD points to", lookup);
            return detail::ObjectPtr(raw);
        }

        detail::SignaturePtr ResolveSignature(git_repository* repo, const Identity& author)
        {
            detail::SignaturePtr signature;

            const int res = author.empty()
                                ? git_signature_default(signature.releaseSlot(), repo)
                                : git_signature_now(signature.releaseSlot(), author.name.c_str(),
                                                    author.email.c_str());
            if (res != 0)
            {
                if (author.empty())
                    throw GitException("no committer is configured - set user.name and "
                                       "user.email in the git configuration",
                                       res);
                GitException::Raise("cannot create the commit signature", res);
            }

            if (!signature->email || !*signature->email)
                throw GitException("the configured committer has no e-mail address - "
                                   "set user.email in the git configuration");
            return signature;
        }
    }

    Identity DefaultIdentity(const Repository& repo)
    {
        detail::EnsureInit();

        Identity identity;
        detail::SignaturePtr signature;
        if (git_signature_default(signature.releaseSlot(), const_cast<Repository&>(repo).Get()) != 0)
        {
            detail::ClearError();
            return identity;
        }
        if (signature->name)
            identity.name = signature->name;
        if (signature->email)
            identity.email = signature->email;
        return identity;
    }

    void Stage(Repository& repo, const std::vector<std::string>& paths)
    {
        detail::EnsureInit();

        detail::IndexPtr index = OpenIndex(repo.Get());
        const detail::Strings pathspec(paths);

        // Literal paths (no globbing), so a name with special characters
        // behaves like on the command line; a directory stages its content.
        const int res = git_index_add_all(index.get(), pathspec.get(),
                                          GIT_INDEX_ADD_DISABLE_PATHSPEC_MATCH, nullptr, nullptr);
        if (res != 0)
        {
            GitException::Raise(paths.empty() ? "cannot stage the changes"
                                              : "cannot stage the selected files",
                                res);
        }
        WriteIndex(index.get());
    }

    void Unstage(Repository& repo, const std::vector<std::string>& paths)
    {
        detail::EnsureInit();

        detail::IndexPtr index = OpenIndex(repo.Get());

        bool unborn = false;
        const detail::ObjectPtr head = HeadCommitObject(repo.Get(), &unborn);
        if (unborn)
        {
            // There is no commit to reset to: unstaging means dropping the
            // entries from the index again.
            if (paths.empty())
            {
                const int res = git_index_clear(index.get());
                if (res != 0)
                    GitException::Raise("cannot reset the index", res);
            }
            else
            {
                for (const std::string& path : paths)
                {
                    const int res = git_index_remove_bypath(index.get(), path.c_str());
                    if (res != 0 && res != GIT_ENOTFOUND)
                        GitException::Raise("cannot unstage '" + path + "'", res);
                }
            }
        }
        else if (!paths.empty())
        {
            const detail::Strings pathspec(paths);
            const int res = git_reset_default(repo.Get(), head.get(), pathspec.get());
            if (res != 0)
                GitException::Raise("cannot unstage the selected files", res);
        }
        else
        {
            // git_reset_default() insists on at least one path, so resetting
            // the whole index is done by reading the tree of HEAD into it.
            // Same result as `git restore --staged .`, and HEAD stays put.
            git_tree* rawTree = nullptr;
            const int treeRes = git_commit_tree(&rawTree,
                                                reinterpret_cast<const git_commit*>(head.get()));
            if (treeRes != 0)
                GitException::Raise("cannot read the tree of HEAD", treeRes);
            const detail::TreePtr headTree(rawTree);

            const int res = git_index_read_tree(index.get(), headTree.get());
            if (res != 0)
                GitException::Raise("cannot reset the index", res);
        }
        WriteIndex(index.get());
    }

    bool HasStagedChanges(const Repository& repo)
    {
        detail::EnsureInit();

        bool unborn = false;
        const detail::ObjectPtr head = HeadCommitObject(const_cast<Repository&>(repo).Get(), &unborn);

        detail::TreePtr headTree;
        if (!unborn && head)
        {
            git_tree* raw = nullptr;
            const int res = git_commit_tree(&raw, reinterpret_cast<const git_commit*>(head.get()));
            if (res != 0)
                GitException::Raise("cannot read the tree of HEAD", res);
            headTree.reset(raw);
        }

        detail::DiffPtr diff;
        const int res = git_diff_tree_to_index(diff.releaseSlot(), const_cast<Repository&>(repo).Get(),
                                              headTree.get(), nullptr, nullptr);
        if (res != 0)
            GitException::Raise("cannot compare the index with HEAD", res);

        return git_diff_num_deltas(diff.get()) > 0;
    }

    std::string CreateCommit(Repository& repo, const std::string& message, const Identity& author)
    {
        detail::EnsureInit();

        if (message.empty())
            throw GitException("a commit message is required");
        if (!HasStagedChanges(repo))
            throw GitException("nothing staged to commit");

        const detail::SignaturePtr signature = ResolveSignature(repo.Get(), author);

        detail::IndexPtr index = OpenIndex(repo.Get());
        git_oid treeId;
        const int treeResult = git_index_write_tree(&treeId, index.get());
        if (treeResult != 0)
            GitException::Raise("cannot write the tree of the index", treeResult);

        detail::TreePtr tree;
        const int lookupResult = git_tree_lookup(tree.releaseSlot(), repo.Get(), &treeId);
        if (lookupResult != 0)
            GitException::Raise("cannot read the new tree", lookupResult);

        // HEAD is the single parent, unless the branch is still unborn.
        bool unborn = false;
        const detail::ObjectPtr head = HeadCommitObject(repo.Get(), &unborn);
        std::vector<const git_commit*> parents;
        if (!unborn && head)
            parents.push_back(reinterpret_cast<const git_commit*>(head.get()));

        git_oid commitId;
        const int res = git_commit_create(&commitId, repo.Get(), "HEAD", signature.get(),
                                          signature.get(), nullptr, message.c_str(), tree.get(),
                                          parents.size(), parents.empty() ? nullptr : parents.data());
        if (res != 0)
            GitException::Raise("cannot create the commit", res);

        return detail::OidToHex(&commitId);
    }
}
