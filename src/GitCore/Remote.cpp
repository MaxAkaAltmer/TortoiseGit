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
        const detail::Strings specs(refspecs);

        git_push_options pushOptions = detail::DefaultPushOptions();
        detail::InstallRemoteCallbacks(pushOptions.callbacks, session.Payload());

        const int res = git_remote_push(session.Get(), specs.get(), &pushOptions);
        if (res != 0)
            session.Fail("cannot push to '" + options.remoteName + "'", res);
    }
}
