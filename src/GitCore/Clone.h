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
    //
    // Cloning a repository without commits leaves the clone on
    // init.defaultBranch ("master" unless configured otherwise): an unborn
    // HEAD is not advertised by the remote, so neither libgit2 nor the caller
    // can learn which branch the source has in mind. Cloning a repository that
    // has commits checks out the branch the remote HEAD names.
    Repository Clone(const std::string& url, const std::string& localPath,
                     const RemoteOptions& options = {});
}
