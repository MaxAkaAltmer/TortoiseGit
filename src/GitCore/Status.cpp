#include "GitCore/Status.h"

#include <git2.h>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"
#include "GitCore/Repository.h"

namespace GitCore
{
    namespace
    {
        // git_status_t is a combined bitfield: GIT_STATUS_INDEX_* describes
        // HEAD -> index, GIT_STATUS_WT_* describes index -> workdir.
        char IndexStatusChar(unsigned int status)
        {
            if (status & GIT_STATUS_CONFLICTED)
                return 'C';
            if (status & GIT_STATUS_INDEX_NEW)
                return 'A';
            if (status & GIT_STATUS_INDEX_MODIFIED)
                return 'M';
            if (status & GIT_STATUS_INDEX_DELETED)
                return 'D';
            if (status & GIT_STATUS_INDEX_RENAMED)
                return 'R';
            if (status & GIT_STATUS_INDEX_TYPECHANGE)
                return 'T';
            return ' ';
        }

        char WorktreeStatusChar(unsigned int status)
        {
            if (status & GIT_STATUS_CONFLICTED)
                return 'C';
            if (status & GIT_STATUS_WT_NEW)
                return '?';
            if (status & GIT_STATUS_WT_MODIFIED)
                return 'M';
            if (status & GIT_STATUS_WT_DELETED)
                return 'D';
            if (status & GIT_STATUS_WT_RENAMED)
                return 'R';
            if (status & GIT_STATUS_WT_TYPECHANGE)
                return 'T';
            if (status & GIT_STATUS_WT_UNREADABLE)
                return 'X';
            return ' ';
        }
    }

    std::vector<FileStatus> GetStatus(const Repository& repo, bool includeUntracked)
    {
        detail::EnsureInit();
        std::vector<FileStatus> result;

        git_status_options opts = GIT_STATUS_OPTIONS_INIT;
        opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
        // Report individual untracked files, not collapsed directories
        opts.flags = GIT_STATUS_OPT_SORT_CASE_SENSITIVELY |
                     GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
        if (includeUntracked)
            opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;

        git_status_list* list = nullptr;
        int res = git_status_list_new(&list, repo.Get(), &opts);
        if (res != 0)
            GitException::Raise("cannot collect repository status", res);
        detail::StatusListPtr listHolder(list);

        const std::size_t count = git_status_list_entrycount(listHolder.get());
        for (std::size_t i = 0; i < count; ++i)
        {
            const git_status_entry* entry = git_status_byindex(listHolder.get(), i);
            if (!entry)
                continue;

            FileStatus file;
            // Any of the deltas may carry the path; deleted files have an
            // empty new_file.path, so fall back to old_file.path.
            auto deltaPath = [](const git_diff_delta* delta) -> const char* {
                if (!delta)
                    return nullptr;
                if (delta->new_file.path && *delta->new_file.path)
                    return delta->new_file.path;
                if (delta->old_file.path && *delta->old_file.path)
                    return delta->old_file.path;
                return nullptr;
            };
            const char* path = deltaPath(entry->index_to_workdir);
            if (!path)
                path = deltaPath(entry->head_to_index);
            if (!path)
                continue;
            file.path = path;

            file.indexStatus = IndexStatusChar(entry->status);
            file.worktreeStatus = WorktreeStatusChar(entry->status);
            result.push_back(file);
        }
        return result;
    }
}
