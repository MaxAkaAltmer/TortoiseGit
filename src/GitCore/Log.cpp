#include "GitCore/Log.h"

#include <git2.h>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"
#include "GitCore/Repository.h"

namespace GitCore
{
    namespace
    {
        using namespace GitCore::detail;

        CommitInfo ReadCommit(git_commit* commit)
        {
            CommitInfo info;
            info.id = OidToHex(git_commit_id(commit));

            if (const char* text = git_commit_summary(commit))
                info.summary = text;
            if (const char* text = git_commit_message(commit))
                info.message = detail::TrimRight(text); // git_commit_message keeps the trailing newline

            if (const git_signature* sig = git_commit_author(commit))
            {
                info.authorName = sig->name ? sig->name : "";
                info.authorEmail = sig->email ? sig->email : "";
                info.authorTime = static_cast<std::int64_t>(sig->when.time);
                info.authorOffsetMinutes = sig->when.offset;
            }
            if (const git_signature* sig = git_commit_committer(commit))
            {
                info.committerName = sig->name ? sig->name : "";
                info.committerEmail = sig->email ? sig->email : "";
                info.commitTime = static_cast<std::int64_t>(sig->when.time);
            }

            const unsigned int parentCount = git_commit_parentcount(commit);
            for (unsigned int i = 0; i < parentCount; ++i)
            {
                const git_oid* parentId = git_commit_parent_id(commit, i);
                if (parentId)
                    info.parentIds.push_back(OidToHex(parentId));
            }
            return info;
        }

        bool TouchesPath(git_repository* raw, git_commit* commit, const std::string& path)
        {
            git_tree* commitTree = nullptr;
            int res = git_commit_tree(&commitTree, commit);
            if (res != 0)
                GitException::Raise("cannot load commit tree", res);
            GitPtr<git_tree, git_tree_free> commitTreeHolder(commitTree);

            git_tree* parentTree = nullptr;
            GitPtr<git_tree, git_tree_free> parentTreeHolder;
            if (git_commit_parentcount(commit) > 0)
            {
                git_commit* parentCommit = nullptr;
                res = git_commit_parent(&parentCommit, commit, 0);
                if (res != 0)
                    GitException::Raise("cannot load parent commit", res);
                CommitPtr parentHolder(parentCommit);
                res = git_commit_tree(&parentTree, parentCommit);
                if (res != 0)
                    GitException::Raise("cannot load parent commit tree", res);
                parentTreeHolder.reset(parentTree);
            }

            Pathspec spec(path);
            git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
            opts.pathspec = spec.get();

            git_diff* diff = nullptr;
            res = git_diff_tree_to_tree(&diff, raw, parentTreeHolder.get(),
                                        commitTreeHolder.get(), &opts);
            if (res != 0)
                GitException::Raise("cannot diff commit trees", res);
            DiffPtr diffHolder(diff);

            return git_diff_num_deltas(diff) > 0;
        }
    }

    std::vector<CommitInfo> GetLog(const Repository& repo,
                                   const std::string& pathFilter,
                                   std::size_t limit,
                                   std::size_t skip,
                                   const std::string& fromRevspec)
    {
        detail::EnsureInit();
        git_repository* raw = repo.Get();
        std::vector<CommitInfo> result;

        detail::RevwalkPtr walk;
        int res = git_revwalk_new(walk.releaseSlot(), raw);
        if (res != 0)
            GitException::Raise("cannot create revision walker", res);
        git_revwalk_sorting(walk.get(), GIT_SORT_TIME);

        ObjectPtr fromObj;
        if (fromRevspec.empty())
        {
            res = git_revwalk_push_head(walk.get());
            if (res == GIT_EUNBORNBRANCH)
                return result;
            if (res != 0)
                GitException::Raise("cannot start history at HEAD", res);
        }
        else
        {
            git_object* obj = nullptr;
            res = git_revparse_single(&obj, raw, fromRevspec.c_str());
            if (res != 0)
                GitException::Raise("cannot resolve revision '" + fromRevspec + "'", res);
            fromObj.reset(obj);
            res = git_revwalk_push(walk.get(), git_object_id(fromObj.get()));
            if (res != 0)
                GitException::Raise("cannot walk from revision '" + fromRevspec + "'", res);
        }

        git_oid oid;
        while (git_revwalk_next(&oid, walk.get()) == 0)
        {
            git_commit* commit = nullptr;
            res = git_commit_lookup(&commit, raw, &oid);
            if (res != 0)
                GitException::Raise("cannot load commit " + detail::OidToHex(&oid), res);
            CommitPtr commitHolder(commit);

            if (!pathFilter.empty() && !TouchesPath(raw, commit, pathFilter))
                continue;

            if (skip > 0)
            {
                --skip;
                continue;
            }

            result.push_back(ReadCommit(commit));
            if (limit != 0 && result.size() >= limit)
                break;
        }
        return result;
    }

    CommitInfo LookupCommit(const Repository& repo, const std::string& revspec)
    {
        detail::EnsureInit();
        git_object* obj = nullptr;
        int res = git_revparse_single(&obj, repo.Get(), revspec.c_str());
        if (res != 0)
            GitException::Raise("cannot resolve revision '" + revspec + "'", res);
        detail::ObjectPtr objHolder(obj);

        git_object* peeled = nullptr;
        res = git_object_peel(&peeled, objHolder.get(), GIT_OBJECT_COMMIT);
        if (res != 0)
            GitException::Raise("revision '" + revspec + "' is not a commit", res);
        // git_object_peel() guarantees a commit object
        detail::ObjectPtr peeledHolder(peeled);
        return ReadCommit(reinterpret_cast<git_commit*>(peeled));
    }

    bool CommitTouchesPath(const Repository& repo, const std::string& commitOid,
                           const std::string& path)
    {
        detail::EnsureInit();
        git_oid oid;
        int res = git_oid_fromstrp(&oid, commitOid.c_str());
        if (res != 0)
            GitException::Raise("invalid commit id '" + commitOid + "'", res);

        git_commit* commit = nullptr;
        res = git_commit_lookup(&commit, repo.Get(), &oid);
        if (res != 0)
            GitException::Raise("cannot load commit " + commitOid, res);
        detail::CommitPtr commitHolder(commit);

        return TouchesPath(repo.Get(), commit, path);
    }
}
