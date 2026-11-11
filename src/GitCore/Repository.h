#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

struct git_repository;

namespace GitCore
{
    struct RepositoryInfo
    {
        std::string workDir;    // empty for bare repositories
        std::string gitDir;
        std::string headBranch; // empty when HEAD is detached or unborn
        bool isBare = false;
        bool isEmpty = false;   // no commit on HEAD yet
        bool isHeadDetached = false;
    };

    // A cross-platform handle on an opened git repository (libgit2 backend).
    class Repository
    {
    public:
        struct RepoDeleter
        {
            void operator()(git_repository* repo) const;
        };
        using Handle = std::unique_ptr<git_repository, RepoDeleter>;

        Repository(const Repository&) = delete;
        Repository& operator=(const Repository&) = delete;
        Repository(Repository&&) noexcept;
        Repository& operator=(Repository&&) noexcept;
        ~Repository();

        // Opens the repository at `path` or in the closest parent directory.
        // Throws GitException if there is none (see ErrorCode()/GIT_ENOTFOUND)
        // or if the repository is owned by another user (GIT_EOWNER).
        static Repository Open(const std::string& path);
        // Like Open, but returns std::nullopt when `path` is not inside
        // a repository. Ownership (safe.directory) problems still throw.
        static std::optional<Repository> TryOpen(const std::string& path);
        // Initializes a new repository in `path` (created if missing).
        static Repository Init(const std::string& path, bool bare = false);

        // Adds `path` as an exception to safe.directory in the global git
        // config (~/.gitconfig, created on demand). No-op when already listed.
        static void AddSafeDirectory(const std::string& path);

        git_repository* Get() const { return m_repo.get(); }
        const RepositoryInfo& Info() const { return m_info; }

        // Hex id of the HEAD commit; empty string when HEAD is unborn.
        std::string HeadOid() const;

        // Takes ownership of a raw libgit2 repository pointer (internal use,
        // e.g. by Clone).
        static Repository FromHandle(Handle&& repo);

    private:
        explicit Repository(Handle&& repo);
        RepositoryInfo LoadInfo() const;

        Handle m_repo;
        RepositoryInfo m_info;
    };
}
