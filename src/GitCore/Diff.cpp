#include "GitCore/Diff.h"

#include <git2.h>

#include "GitCore/GitCoreUtil.h"
#include "GitCore/GitException.h"
#include "GitCore/Repository.h"

namespace GitCore
{
    namespace
    {
        using namespace GitCore::detail;

        char DeltaChar(git_delta_t status)
        {
            switch (status)
            {
            case GIT_DELTA_ADDED:      return 'A';
            case GIT_DELTA_DELETED:    return 'D';
            case GIT_DELTA_MODIFIED:   return 'M';
            case GIT_DELTA_RENAMED:    return 'R';
            case GIT_DELTA_TYPECHANGE: return 'T';
            case GIT_DELTA_CONFLICTED: return 'C';
            case GIT_DELTA_UNREADABLE: return 'X';
            default:                   return ' ';
            }
        }

        TreePtr ResolveTree(git_repository* raw, const std::string& revspec)
        {
            git_object* obj = nullptr;
            int res = git_revparse_single(&obj, raw, revspec.c_str());
            if (res != 0)
                GitException::Raise("cannot resolve revision '" + revspec + "'", res);
            ObjectPtr objHolder(obj);

            git_object* treeObj = nullptr;
            res = git_object_peel(&treeObj, objHolder.get(), GIT_OBJECT_TREE);
            if (res != 0)
                GitException::Raise("revision '" + revspec + "' has no tree", res);
            // git_object_peel() guarantees a tree object when it succeeds
            return TreePtr(reinterpret_cast<git_tree*>(treeObj));
        }

        DiffPtr BuildDiff(const Repository& repo, const std::string& rev1,
                          const std::string& rev2, const std::string& fileFilter)
        {
            git_repository* raw = repo.Get();
            TreePtr tree1 = ResolveTree(raw, rev1);

            Pathspec spec(fileFilter);
            git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
            opts.pathspec = spec.get();

            git_diff* diff = nullptr;
            int res = 0;
            if (rev2.empty())
            {
                // diff rev1..workdir (via index)
                res = git_diff_tree_to_workdir_with_index(&diff, raw, tree1.get(), &opts);
                if (res != 0)
                    GitException::Raise("cannot diff '" + rev1 + "' against the working tree", res);
            }
            else
            {
                TreePtr tree2 = ResolveTree(raw, rev2);
                res = git_diff_tree_to_tree(&diff, raw, tree1.get(), tree2.get(), &opts);
                if (res != 0)
                    GitException::Raise("cannot diff '" + rev1 + "' against '" + rev2 + "'", res);
            }
            return DiffPtr(diff);
        }

        int PrintDiffLine(const git_diff_delta* /*delta*/, const git_diff_hunk* /*hunk*/,
                          const git_diff_line* line, void* payload)
        {
            auto* out = static_cast<std::string*>(payload);
            if (!line || !line->content)
                return 0;
            // For body lines the leading column character lives in line->origin
            // and is not part of content (same rule as the built-in printers).
            if (line->origin == GIT_DIFF_LINE_ADDITION ||
                line->origin == GIT_DIFF_LINE_DELETION ||
                line->origin == GIT_DIFF_LINE_CONTEXT)
                out->push_back(line->origin);
            if (line->content_len > 0)
                out->append(line->content, static_cast<std::size_t>(line->content_len));
            return 0;
        }
    }

    std::vector<FileDiff> GetChangedFiles(const Repository& repo,
                                          const std::string& rev1,
                                          const std::string& rev2,
                                          const std::string& fileFilter)
    {
        detail::EnsureInit();
        DiffPtr diff = BuildDiff(repo, rev1, rev2, fileFilter);

        std::vector<FileDiff> result;
        const std::size_t count = git_diff_num_deltas(diff.get());
        for (std::size_t i = 0; i < count; ++i)
        {
            const git_diff_delta* delta = git_diff_get_delta(diff.get(), i);
            if (!delta)
                continue;
            FileDiff file;
            file.status = DeltaChar(delta->status);
            if (delta->old_file.path)
                file.oldPath = delta->old_file.path;
            if (delta->new_file.path)
                file.newPath = delta->new_file.path;
            result.push_back(file);
        }
        return result;
    }

    std::string GetUnifiedPatch(const Repository& repo,
                                const std::string& rev1,
                                const std::string& rev2,
                                const std::string& fileFilter)
    {
        detail::EnsureInit();
        DiffPtr diff = BuildDiff(repo, rev1, rev2, fileFilter);

        std::string out;
        int res = git_diff_print(diff.get(), GIT_DIFF_FORMAT_PATCH, PrintDiffLine, &out);
        if (res != 0)
            GitException::Raise("cannot render diff patch", res);
        return out;
    }
}
