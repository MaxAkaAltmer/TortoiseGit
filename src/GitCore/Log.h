#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GitCore
{
    class Repository;

    struct CommitInfo
    {
        std::string id; // hex object id
        std::string summary;   // first line of the message
        std::string message;   // full message
        std::string authorName;
        std::string authorEmail;
        std::int64_t authorTime = 0;      // seconds since epoch
        int authorOffsetMinutes = 0;
        std::string committerName;
        std::string committerEmail;
        std::int64_t commitTime = 0;      // seconds since epoch
        std::vector<std::string> parentIds;
    };

    // Walks history from HEAD (or from `fromRevspec` when given).
    // `pathFilter` (optional) keeps only commits that touch the given file.
    // Results are ordered newest first; `limit` 0 means "no limit".
    std::vector<CommitInfo> GetLog(const Repository& repo,
                                   const std::string& pathFilter = {},
                                   std::size_t limit = 50,
                                   std::size_t skip = 0,
                                   const std::string& fromRevspec = {});

    // Resolves an arbitrary revision (id, branch, tag, HEAD~2, ...) to a commit.
    CommitInfo LookupCommit(const Repository& repo, const std::string& revspec);

    // True when `commit` introduced a change to `path` compared to its
    // first parent (root commits are compared to the empty tree).
    bool CommitTouchesPath(const Repository& repo, const std::string& commitOid,
                           const std::string& path);
}
