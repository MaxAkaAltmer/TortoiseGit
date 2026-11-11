#pragma once

#include <functional>
#include <string>

#include "GitCore/Repository.h"

namespace GitCore
{
    // Clones `url` into `localPath` and returns the opened clone.
    // `progress` (optional) receives human-readable progress lines.
    // Phase 2 MVP supports local paths and git://; authenticated remotes
    // (https/ssh) are not implemented yet.
    Repository Clone(const std::string& url, const std::string& localPath,
                     const std::function<void(const std::string&)>& progress = {});
}
