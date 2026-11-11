#include <gtest/gtest.h>

#include <algorithm>

#include "GitCore/Repository.h"
#include "GitCore/Status.h"
#include "TestRepo.h"

namespace
{
    // Finds the entry for `path` or returns nullptr.
    const GitCore::FileStatus* Find(const std::vector<GitCore::FileStatus>& status,
                                    const std::string& path)
    {
        const auto it = std::find_if(status.begin(), status.end(),
                                     [&](const GitCore::FileStatus& f) {
                                         return f.path == path;
                                     });
        return it == status.end() ? nullptr : &*it;
    }
}

TEST(StatusTest, EmptyForCleanRepository)
{
    TestRepo src("statusclean");
    src.Write("tracked.txt", "one\n");
    src.Commit("initial");

    auto repo = GitCore::Repository::Open(src.root.string());
    EXPECT_TRUE(GitCore::GetStatus(repo).empty());
}

TEST(StatusTest, ModifiedUntrackedStagedDeleted)
{
    TestRepo src("statusmix");
    src.Write("tracked.txt", "one\n");
    src.Commit("initial");

    // modify a tracked file, add a new one, delete nothing yet
    src.Write("tracked.txt", "two\n");
    src.Write("new.txt", "fresh\n");

    {
        auto repo = GitCore::Repository::Open(src.root.string());
        const auto status = GitCore::GetStatus(repo);
        ASSERT_EQ(status.size(), 2u);

        const auto* modified = Find(status, "tracked.txt");
        ASSERT_TRUE(modified != nullptr);
        EXPECT_EQ(modified->indexStatus, ' ');
        EXPECT_EQ(modified->worktreeStatus, 'M');

        const auto* untracked = Find(status, "new.txt");
        ASSERT_TRUE(untracked != nullptr);
        EXPECT_EQ(untracked->indexStatus, ' ');
        EXPECT_EQ(untracked->worktreeStatus, '?');
    }

    // stage everything: tracked.txt becomes M in the index, new.txt A
    src.RequireGit("add -A");
    {
        auto repo = GitCore::Repository::Open(src.root.string());
        const auto status = GitCore::GetStatus(repo);
        ASSERT_EQ(status.size(), 2u);

        const auto* modified = Find(status, "tracked.txt");
        ASSERT_TRUE(modified != nullptr);
        EXPECT_EQ(modified->indexStatus, 'M');
        EXPECT_EQ(modified->worktreeStatus, ' ');

        const auto* added = Find(status, "new.txt");
        ASSERT_TRUE(added != nullptr);
        EXPECT_EQ(added->indexStatus, 'A');
    }

    // delete a staged-then-committed file from the worktree
    src.Commit("staged");
    src.Delete("tracked.txt");
    {
        auto repo = GitCore::Repository::Open(src.root.string());
        const auto status = GitCore::GetStatus(repo);

        const auto* deleted = Find(status, "tracked.txt");
        ASSERT_TRUE(deleted != nullptr);
        EXPECT_EQ(deleted->indexStatus, ' ');
        EXPECT_EQ(deleted->worktreeStatus, 'D');
    }
}

TEST(StatusTest, UntrackedCanBeExcluded)
{
    TestRepo src("statusuntracked");
    src.Write("tracked.txt", "one\n");
    src.Commit("initial");
    src.Write("new.txt", "fresh\n");

    auto repo = GitCore::Repository::Open(src.root.string());
    const auto status = GitCore::GetStatus(repo, false);
    EXPECT_TRUE(status.empty());
}

TEST(StatusTest, NestedPathsUseForwardSlashes)
{
    TestRepo src("statusnested");
    src.Write("tracked.txt", "one\n");
    src.Commit("initial");
    src.Write("sub/dir/other.txt", "text\n");

    auto repo = GitCore::Repository::Open(src.root.string());
    const auto status = GitCore::GetStatus(repo);
    ASSERT_EQ(status.size(), 1u);
    EXPECT_EQ(status[0].path, "sub/dir/other.txt");
}
