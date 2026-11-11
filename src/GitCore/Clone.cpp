#include "GitCore/Clone.h"

#include <git2.h>

#include <string>

#include "GitCore/CredentialsInternal.h"
#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"

namespace GitCore
{
    Repository Clone(const std::string& url, const std::string& localPath,
                     const RemoteOptions& options)
    {
        detail::EnsureInit();

        // Fail early with a hint about the missing backend instead of letting
        // libgit2 report an opaque "unsupported URL" error.
        if (const std::string hint = MissingTransportHint(url); !hint.empty())
            throw GitException("cannot clone '" + url + "': " + hint);

        detail::RemoteCallbackPayload payload;
        payload.progress                 = options.progress;
        payload.credentials.provider     = options.credentials;

        git_clone_options cloneOptions = detail::DefaultCloneOptions();
        detail::InstallRemoteCallbacks(cloneOptions.fetch_opts.callbacks, payload);

        git_repository* raw = nullptr;
        const int res = git_clone(&raw, url.c_str(), localPath.c_str(), &cloneOptions);
        if (res != 0)
        {
            std::string context = "cannot clone '" + url + "' into '" + localPath + "'";
            if (!payload.credentials.errorText.empty())
                context += ": " + payload.credentials.errorText;
            GitException::Raise(context, res);
        }

        return Repository::FromHandle(Repository::Handle(raw));
    }
}
