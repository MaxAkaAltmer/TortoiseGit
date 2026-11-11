#include "GitCore/GitException.h"

#include <git2.h>

#include <sstream>

#include "GitCore/GitCoreUtil.h"

namespace GitCore
{
    GitException::GitException(const std::string& message, int errorCode)
        : std::runtime_error(message), m_errorCode(errorCode)
    {
    }

    void GitException::Raise(const std::string& context, int gitResult)
    {
        std::ostringstream oss;
        oss << context;

        const git_error* err = git_error_last();
        if (err && err->message)
            oss << ": " << detail::TrimRight(err->message);
        if (gitResult != 0)
            oss << " [git error " << gitResult << ']';
        if (gitResult == GIT_EOWNER)
            oss << " - the repository is owned by another user."
                   " Add it to safe.directory (Repository::AddSafeDirectory"
                   " or 'tgit-cli trust') to allow access";

        throw GitException(oss.str(), gitResult);
    }
}
