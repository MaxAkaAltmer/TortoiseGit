#pragma once

#include <stdexcept>
#include <string>

namespace GitCore
{
    // Exception thrown by all gitcore operations. Carries the libgit2 return
    // code (when known) and the details of the last libgit2 error, if any.
    class GitException : public std::runtime_error
    {
    public:
        GitException(const std::string& message, int errorCode = 0);

        // libgit2 return code (e.g. GIT_ENOTFOUND), 0 if not applicable
        int ErrorCode() const noexcept { return m_errorCode; }

        // Throws a GitException for a failed libgit2 call. Appends the
        // git_error_last() message when present. `gitResult` must be != 0.
        [[noreturn]] static void Raise(const std::string& context, int gitResult);

    private:
        int m_errorCode;
    };
}
