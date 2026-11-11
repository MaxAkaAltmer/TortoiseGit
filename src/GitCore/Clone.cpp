#include "GitCore/Clone.h"

#include <git2.h>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"

namespace GitCore
{
    namespace
    {
        int SidebandProgress(const char* text, int len, void* payload)
        {
            auto* progress = static_cast<std::function<void(const std::string&)>*>(payload);
            if (progress && *progress && text && len > 0)
                (*progress)(std::string(text, static_cast<std::size_t>(len)));
            return 0;
        }
    }

    Repository Clone(const std::string& url, const std::string& localPath,
                     const std::function<void(const std::string&)>& progress)
    {
        detail::EnsureInit();

        git_clone_options options = GIT_CLONE_OPTIONS_INIT;
        if (progress)
        {
            options.fetch_opts.callbacks.sideband_progress = SidebandProgress;
            options.fetch_opts.callbacks.payload =
                const_cast<std::function<void(const std::string&)>*>(&progress);
        }

        git_repository* raw = nullptr;
        int res = git_clone(&raw, url.c_str(), localPath.c_str(), &options);
        if (res != 0)
            GitException::Raise("cannot clone '" + url + "' into '" + localPath + "'", res);

        return Repository::FromHandle(Repository::Handle(raw));
    }
}
