#include "GitCore/Repository.h"

#include <git2.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"

namespace GitCore
{
    void Repository::RepoDeleter::operator()(git_repository* repo) const
    {
        if (repo)
            git_repository_free(repo);
    }

    Repository::Repository(Handle&& repo)
        : m_repo(std::move(repo)), m_info(LoadInfo())
    {
    }

    Repository::Repository(Repository&&) noexcept = default;
    Repository& Repository::operator=(Repository&&) noexcept = default;
    Repository::~Repository() = default;

    Repository Repository::FromHandle(Handle&& repo)
    {
        return Repository(std::move(repo));
    }

    Repository Repository::Open(const std::string& path)
    {
        detail::EnsureInit();
        git_repository* raw = nullptr;
        int res = git_repository_open_ext(&raw, path.c_str(), 0, nullptr);
        if (res != 0)
            GitException::Raise("cannot open repository at '" + path + "'", res);
        return Repository(Handle(raw));
    }

    std::optional<Repository> Repository::TryOpen(const std::string& path)
    {
        try
        {
            return Open(path);
        }
        catch (const GitException& e)
        {
            if (e.ErrorCode() == GIT_ENOTFOUND)
                return std::nullopt;
            throw;
        }
    }

    Repository Repository::Init(const std::string& path, bool bare)
    {
        detail::EnsureInit();
        git_repository* raw = nullptr;
        unsigned int flags = bare ? GIT_REPOSITORY_INIT_BARE : 0;
        int res = git_repository_init(&raw, path.c_str(), flags);
        if (res != 0)
            GitException::Raise("cannot initialize repository at '" + path + "'", res);
        return Repository(Handle(raw));
    }

    namespace
    {
        int CollectSafeDirectory(const git_config_entry* entry, void* payload)
        {
            auto* data = static_cast<std::pair<std::vector<std::string>*, int>*>(payload);
            if (entry && entry->value)
                data->first->push_back(entry->value);
            return 0;
        }
    }

    void Repository::AddSafeDirectory(const std::string& path)
    {
        detail::EnsureInit();

        // libgit2 compares safe.directory entries against the resolved path
        std::error_code ec;
        std::string absolute = std::filesystem::weakly_canonical(path, ec).string();
        if (ec)
            absolute = path;

        detail::ConfigPtr cfg;
        int res = git_config_open_default(cfg.releaseSlot());
        if (res != 0)
            GitException::Raise("cannot open the global git config", res);

        std::pair<std::vector<std::string>*, int> ctx;
        std::vector<std::string> existing;
        ctx.first = &existing;
        ctx.second = 0;
        res = git_config_get_multivar_foreach(cfg.get(), "safe.directory",
                                              nullptr, CollectSafeDirectory, &ctx);
        if (res != 0 && res != GIT_ENOTFOUND)
            GitException::Raise("cannot read safe.directory entries", res);

        if (std::find(existing.begin(), existing.end(), absolute) != existing.end())
            return; // already trusted

        // safe.directory is a multivar: use a regexp that cannot match any
        // real path so that the entry is appended instead of replacing others
        res = git_config_set_multivar(cfg.get(), "safe.directory",
                                      "^<tgit-safe-directory>$", absolute.c_str());
        if (res != 0)
            GitException::Raise("cannot write safe.directory to the global git config", res);
    }

    std::string Repository::HeadOid() const
    {
        git_reference* head = nullptr;
        int res = git_repository_head(&head, m_repo.get());
        if (res == GIT_EUNBORNBRANCH)
            return {};
        if (res != 0)
            GitException::Raise("cannot resolve HEAD", res);
        detail::ReferencePtr ref(head);
        return detail::OidToHex(git_reference_target(ref.get()));
    }

    RepositoryInfo Repository::LoadInfo() const
    {
        RepositoryInfo info;
        info.isBare = git_repository_is_bare(m_repo.get()) == 1;
        info.isEmpty = git_repository_is_empty(m_repo.get()) == 1;
        info.isHeadDetached = git_repository_head_detached(m_repo.get()) == 1;

        if (const char* workdir = git_repository_workdir(m_repo.get()))
            info.workDir = workdir;
        if (const char* gitdir = git_repository_path(m_repo.get()))
            info.gitDir = gitdir;

        if (!info.isEmpty && !info.isHeadDetached)
        {
            git_reference* head = nullptr;
            if (git_repository_head(&head, m_repo.get()) == 0)
            {
                detail::ReferencePtr ref(head);
                const char* name = nullptr;
                if (git_branch_name(&name, ref.get()) == 0 && name)
                    info.headBranch = name;
            }
            else
            {
                detail::ClearError();
            }
        }
        return info;
    }
}
