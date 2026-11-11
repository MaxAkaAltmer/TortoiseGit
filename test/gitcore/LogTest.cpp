#include <gtest/gtest.h>

#include "GitCore/Log.h"
#include "GitCore/Repository.h"
#include "TestRepo.h"

namespace
{
    // Three commits: "first" (a.txt), "second" (b.txt), "third" (a.txt again).
    struct ThreeCommits : TestRepo
    {
        ThreeCommits()
            : TestRepo("log")
        {
            Write("a.txt", "a1\n");
            Commit("first");
            Write("b.txt", "b1\n");
            Commit("second");
            Write("a.txt", "a2\n");
            Commit("third");
        }
    };
}

TEST(LogTest, WalksHeadNewestFirst)
{
    ThreeCommits src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto log = GitCore::GetLog(repo, {}, 0);
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0].summary, "third");
    EXPECT_EQ(log[1].summary, "second");
    EXPECT_EQ(log[2].summary, "first");

    EXPECT_EQ(log[0].id, repo.HeadOid());
    EXPECT_EQ(log[0].authorName, "Test User");
    EXPECT_EQ(log[0].authorEmail, "test@example.com");
    EXPECT_EQ(log[0].parentIds.size(), 1u);
    EXPECT_EQ(log[0].parentIds[0], log[1].id);
    EXPECT_TRUE(log[2].parentIds.empty());

    // strictly increasing timestamps -> strictly ordered walk
    EXPECT_GT(log[0].commitTime, log[1].commitTime);
    EXPECT_GT(log[1].commitTime, log[2].commitTime);
}

TEST(LogTest, LimitAndSkip)
{
    ThreeCommits src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto limited = GitCore::GetLog(repo, {}, 2);
    ASSERT_EQ(limited.size(), 2u);
    EXPECT_EQ(limited[0].summary, "third");
    EXPECT_EQ(limited[1].summary, "second");

    const auto skipped = GitCore::GetLog(repo, {}, 0, 1);
    ASSERT_EQ(skipped.size(), 2u);
    EXPECT_EQ(skipped[0].summary, "second");
    EXPECT_EQ(skipped[1].summary, "first");
}

TEST(LogTest, PathFilter)
{
    ThreeCommits src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto log = GitCore::GetLog(repo, "a.txt", 0);
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0].summary, "third");
    EXPECT_EQ(log[1].summary, "first");

    const auto none = GitCore::GetLog(repo, "missing.txt", 0);
    EXPECT_TRUE(none.empty());
}

TEST(LogTest, FromRevspec)
{
    ThreeCommits src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto log = GitCore::GetLog(repo, {}, 0, 0, "HEAD~1");
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0].summary, "second");
    EXPECT_EQ(log[1].summary, "first");
}

TEST(LogTest, LookupCommitResolvesArbitraryRevs)
{
    ThreeCommits src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto commit = GitCore::LookupCommit(repo, "HEAD~2");
    EXPECT_EQ(commit.summary, "first");
    EXPECT_EQ(commit.message, "first");

    const auto byBranch = GitCore::LookupCommit(repo, "main");
    EXPECT_EQ(byBranch.id, repo.HeadOid());
}

TEST(LogTest, CommitTouchesPath)
{
    ThreeCommits src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto log = GitCore::GetLog(repo, {}, 0);
    const std::string second = log[1].id;

    EXPECT_TRUE(GitCore::CommitTouchesPath(repo, second, "b.txt"));
    EXPECT_FALSE(GitCore::CommitTouchesPath(repo, second, "a.txt"));
}
