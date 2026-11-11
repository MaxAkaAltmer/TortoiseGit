#pragma once

#include <string>

#include "GitCore/Remote.h"
#include "GitCore/Repository.h"

namespace GitCore
{
    // Clones `url` into `localPath` and returns the opened clone. `options`
    // carries the progress callback and the credentials provider; the clone
    // always registers the source as the remote "origin".
    // https:// and ssh URLs need the matching libgit2 backend, see
    // MissingTransportHint() for a readable explanation of what is missing.
    Repository Clone(const std::string& url, const std::string& localPath,
                     const RemoteOptions& options = {});
}
