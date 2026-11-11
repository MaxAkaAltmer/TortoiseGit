#include <gtest/gtest.h>

#include <algorithm>

#include "GitCore/Branches.h"
#include "GitCore/Repository.h"
#include "TestRepo.h"

namespace
{
    const GitCore::BranchInfo* Find(const std::vector<GitCore::BranchInfo>& branches,
                                    const std::string& name)
    {
        const auto it = std::find_if(branches.begin(), branches.end(),
                                     [&](const GitCore::BranchInfo& b) {
                                         return b.name == name;
                                     });
        return it == branches.end() ? nullptr : &*it;
    }
}

TEST(BranchesTest, ListsLocalBranchesAndHead)
{
    TestRepo src("branches");
    src.Write("file.txt", "one\n");
    src.Commit("initial");
    src.CreateBranch("feature");
    src.CreateBranch("release");

    auto repo = GitCore::Repository::Open(src.root.string());
    const auto branches = GitCore::GetBranches(repo, false);
    ASSERT_EQ(branches.size(), 3u);

    const auto* main = Find(branches, "main");
    ASSERT_TRUE(main != nullptr);
    EXPECT_TRUE(main->isHead);
    EXPECT_FALSE(main->isRemote);
    EXPECT_EQ(main->oid, repo.HeadOid());

    const auto* feature = Find(branches, "feature");
    ASSERT_TRUE(feature != nullptr);
    EXPECT_FALSE(feature->isHead);
    EXPECT_EQ(feature->oid, repo.HeadOid());

    ASSERT_TRUE(Find(branches, "release") != nullptr);
}

TEST(BranchesTest, RemoteBranchesFlagged)
{
    TestRepo src("remotes");
    src.Write("file.txt", "one\n");
    src.Commit("initial");

    // "clone" the fixture repository locally to obtain a remote-tracking ref
    const auto clonePath = src.root.parent_path() /
                           ("tgitcore_test_remotes_clone_" +
                            std::to_string(::getpid()));
    std::filesystem::remove_all(clonePath);
    src.RequireGit("clone -q '" + src.root.string() + "' '" + clonePath.string() + "'");

    auto repo = GitCore::Repository::Open(clonePath.string());
    const auto withRemote = GitCore::GetBranches(repo, true);

    const auto* remote = Find(withRemote, "origin/main");
    ASSERT_TRUE(remote != nullptr);
    EXPECT_TRUE(remote->isRemote);
    EXPECT_FALSE(remote->isHead);

    const auto localOnly = GitCore::GetBranches(repo, false);
    EXPECT_EQ(localOnly.size(), withRemote.size() - 1u);

    std::filesystem::remove_all(clonePath);
}
