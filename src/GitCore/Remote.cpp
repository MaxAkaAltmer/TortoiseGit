#include "GitCore/Remote.h"

#include <git2.h>

#include <cstddef>
#include <utility>

#include "GitCore/CredentialsInternal.h"
#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"

namespace GitCore
{
    namespace
    {
        // Owns a named remote together with the payload of its callbacks.
        // libgit2 keeps no copy of the payload, so it has to outlive the
        // fetch/push call - keeping it here makes that impossible to forget.
        class RemoteSession
        {
        public:
            RemoteSession(Repository& repo, const RemoteOptions& options)
            {
                detail::EnsureInit();
                m_payload.progress               = options.progress;
                m_payload.credentials.provider   = options.credentials;

                git_remote* raw = nullptr;
                const int res = git_remote_lookup(&raw, repo.Get(), options.remoteName.c_str());
                if (res != 0)
                    GitException::Raise("cannot open remote '" + options.remoteName + "'", res);
                m_remote.reset(raw);
            }

            RemoteSession(const RemoteSession&) = delete;
            RemoteSession& operator=(const RemoteSession&) = delete;

            git_remote* Get() const { return m_remote.get(); }
            detail::RemoteCallbackPayload& Payload() { return m_payload; }

            // Throws for a failed remote operation, adding what the
            // credentials callback reported (wrong password, no such key...).
            [[noreturn]] void Fail(const std::string& context, int gitResult) const
            {
                std::string message = context;
                if (!m_payload.credentials.errorText.empty())
                    message += ": " + m_payload.credentials.errorText;
                GitException::Raise(message, gitResult);
            }

        private:
            detail::RemoteCallbackPayload m_payload;
            detail::RemotePtr m_remote;
        };

        struct HeadBranch
        {
            std::string fullName;  // refs/heads/main
            std::string shortName; // main
        };

        // The checked out branch. Reading HEAD as a reference instead of using
        // git_repository_head() is what makes this work for an unborn branch,
        // where the latter reports GIT_EUNBORNBRANCH. `action` only appears in
        // the error messages of the caller.
        HeadBranch CurrentBranch(git_repository* repo, const char* action)
        {
            detail::ReferencePtr head;
            const int res = git_reference_lookup(head.releaseSlot(), repo, "HEAD");
            if (res != 0)
                GitException::Raise("cannot read HEAD", res);

            const char* target = git_reference_type(head.get()) == GIT_REFERENCE_SYMBOLIC
                                     ? git_reference_symbolic_target(head.get())
                                     : nullptr;
            if (!target)
                throw GitException(std::string("cannot ") + action +
                                   " with a detached HEAD - check out a branch first");

            static const std::string prefix = "refs/heads/";
            HeadBranch branch;
            branch.fullName = target;
            if (branch.fullName.compare(0, prefix.size(), prefix) != 0 ||
                branch.fullName.size() <= prefix.size())
                throw GitException(std::string("cannot ") + action +
                                   ": HEAD does not point to a local branch");

            branch.shortName = branch.fullName.substr(prefix.size());
            return branch;
        }
    }

    std::vector<RemoteInfo> ListRemotes(const Repository& repo)
    {
        detail::EnsureInit();

        detail::StrArrayContents names;
        const int res = git_remote_list(names.releaseSlot(), repo.Get());
        if (res != 0)
            GitException::Raise("cannot list remotes", res);

        std::vector<RemoteInfo> result;
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            RemoteInfo info;
            info.name = names.at(i);
            if (info.name.empty())
                continue;

            detail::RemotePtr remote;
            if (git_remote_lookup(remote.releaseSlot(), repo.Get(), info.name.c_str()) != 0)
            {
                // The remote is configured but cannot be parsed; report its
                // name only, the details are not needed to list it.
                detail::ClearError();
                result.push_back(info);
                continue;
            }

            if (const char* url = git_remote_url(remote.get()))
                info.url = url;
            if (const char* pushUrl = git_remote_pushurl(remote.get()))
                info.pushUrl = pushUrl;
            if (info.pushUrl.empty())
                info.pushUrl = info.url;

            result.push_back(info);
        }
        return result;
    }

    std::string RemoteUrl(const Repository& repo, const std::string& remoteName)
    {
        detail::EnsureInit();

        detail::RemotePtr remote;
        const int res = git_remote_lookup(remote.releaseSlot(), repo.Get(), remoteName.c_str());
        if (res != 0)
            GitException::Raise("cannot open remote '" + remoteName + "'", res);

        const char* url = git_remote_url(remote.get());
        return url ? std::string(url) : std::string();
    }

    void Fetch(Repository& repo, const std::vector<std::string>& refspecs, const RemoteOptions& options)
    {
        RemoteSession session(repo, options);
        const detail::Strings specs(refspecs);

        git_fetch_options fetchOptions = detail::DefaultFetchOptions();
        detail::InstallRemoteCallbacks(fetchOptions.callbacks, session.Payload());

        const int res = git_remote_fetch(session.Get(), specs.get(), &fetchOptions, nullptr);
        if (res != 0)
            session.Fail("cannot fetch from '" + options.remoteName + "'", res);
    }

    void Push(Repository& repo, const std::vector<std::string>& refspecs, const RemoteOptions& options)
    {
        RemoteSession session(repo, options);

        // Without explicit refspecs, push the checked out branch to the branch
        // of the same name, which is what `git push <remote>` does. Passing an
        // empty list to libgit2 would instead use the configured push refspec
        // and, for a branch without an upstream, send nothing at all while
        // reporting success.
        std::vector<std::string> effectiveRefspecs = refspecs;
        if (effectiveRefspecs.empty())
        {
            const std::string branch = CurrentBranch(repo.Get(), "push").fullName;
            effectiveRefspecs.push_back(branch + ":" + branch);
        }
        const detail::Strings specs(effectiveRefspecs);

        git_push_options pushOptions = detail::DefaultPushOptions();
        detail::InstallRemoteCallbacks(pushOptions.callbacks, session.Payload());

        const int res = git_remote_push(session.Get(), specs.get(), &pushOptions);
        if (res != 0)
            session.Fail("cannot push to '" + options.remoteName + "'", res);
    }

    bool Pull(Repository& repo, const RemoteOptions& options)
    {
        detail::EnsureInit();

        if (git_repository_is_bare(repo.Get()))
            throw GitException("cannot pull into a bare repository - it has no working tree");

        const HeadBranch branch = CurrentBranch(repo.Get(), "pull");
        const std::string upstreamRefName =
            "refs/remotes/" + options.remoteName + "/" + branch.shortName;

        // Fetch exactly the branch that is being pulled (the refspec `git pull`
        // uses), so that repositories without a configured fetch refspec work
        // as well.
        Fetch(repo, {"+" + branch.fullName + ":" + upstreamRefName}, options);

        detail::ReferencePtr upstream;
        const int upstreamRes = git_reference_lookup(upstream.releaseSlot(), repo.Get(),
                                                     upstreamRefName.c_str());
        if (upstreamRes == GIT_ENOTFOUND)
        {
            // The remote does not have this branch, so there is nothing to
            // integrate - which is the same result as being up to date.
            detail::ClearError();
            return true;
        }
        if (upstreamRes != 0)
            GitException::Raise("cannot read '" + upstreamRefName + "'", upstreamRes);

        const git_oid* upstreamId = git_reference_target(upstream.get());
        if (!upstreamId)
            throw GitException("'" + upstreamRefName + "' does not point to a commit");

        detail::ReferencePtr localRef;
        detail::CommitPtr    localCommit;
        bool                 unborn = false;
        const int localRes = git_reference_lookup(localRef.releaseSlot(), repo.Get(),
                                                  branch.fullName.c_str());
        if (localRes == GIT_ENOTFOUND)
        {
            detail::ClearError();
            unborn = true;
        }
        else if (localRes != 0)
        {
            GitException::Raise("cannot read '" + branch.fullName + "'", localRes);
        }
        else
        {
            const int commitRes = git_commit_lookup(localCommit.releaseSlot(), repo.Get(),
                                                    git_reference_target(localRef.get()));
            if (commitRes != 0)
                GitException::Raise("cannot read the commit of '" + branch.fullName + "'", commitRes);
        }

        std::size_t ahead  = 0;
        std::size_t behind = 0;
        if (unborn)
        {
            behind = 1; // the entire history of the remote is new
        }
        else
        {
            const int res = git_graph_ahead_behind(&ahead, &behind, repo.Get(),
                                                   git_commit_id(localCommit.get()), upstreamId);
            if (res != 0)
                GitException::Raise("cannot compare '" + branch.shortName + "' with '" +
                                        upstreamRefName + "'",
                                    res);
        }

        if (ahead > 0 && behind > 0)
            return false; // diverged - merging or rebasing is the user's decision
        if (behind == 0)
            return true;  // already up to date

        detail::CommitPtr target;
        const int targetRes = git_commit_lookup(target.releaseSlot(), repo.Get(), upstreamId);
        if (targetRes != 0)
            GitException::Raise("cannot read the commit to integrate", targetRes);

        // The working tree is updated before the branch is moved, so that a
        // failing checkout can never leave the ref ahead of the files.
        git_checkout_options checkoutOptions = detail::DefaultCheckoutOptions();
        checkoutOptions.checkout_strategy    = GIT_CHECKOUT_SAFE;
        const int checkoutRes = git_checkout_tree(
            repo.Get(), reinterpret_cast<const git_object*>(target.get()), &checkoutOptions);
        if (checkoutRes != 0)
            GitException::Raise(
                "cannot update the working tree - commit or restore your changes first",
                checkoutRes);

        if (unborn)
        {
            detail::ReferencePtr created;
            const int res = git_branch_create(created.releaseSlot(), repo.Get(),
                                              branch.shortName.c_str(), target.get(), 1);
            if (res != 0)
                GitException::Raise("cannot create branch '" + branch.shortName + "'", res);
        }
        else
        {
            detail::ReferencePtr updated;
            const int res = git_reference_set_target(updated.releaseSlot(), localRef.get(),
                                                     upstreamId, "pull");
            if (res != 0)
                GitException::Raise("cannot move branch '" + branch.shortName + "'", res);
        }
        return true;
    }
}
