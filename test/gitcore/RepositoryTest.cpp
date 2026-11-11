#include <git2.h>
#include <gtest/gtest.h>

#include "GitCore/GitException.h"
#include "GitCore/Repository.h"
#include "TestRepo.h"

namespace
{
    std::string StripSlash(std::string s)
    {
        while (!s.empty() && s.back() == '/')
            s.pop_back();
        return s;
    }
}

TEST(RepositoryTest, InitCreatesEmptyRepository)
{
    TestRepo src("init");
    const auto path = src.root / "fresh";
    auto repo = GitCore::Repository::Init(path.string());

    EXPECT_TRUE(repo.Info().isEmpty);
    EXPECT_FALSE(repo.Info().isBare);
    EXPECT_FALSE(repo.Info().isHeadDetached);
    EXPECT_TRUE(repo.HeadOid().empty());
    EXPECT_EQ(StripSlash(repo.Info().gitDir), (path / ".git").string());
}

TEST(RepositoryTest, InitBare)
{
    TestRepo src("initbare");
    const auto path = src.root / "bare.git";
    auto repo = GitCore::Repository::Init(path.string(), true);

    EXPECT_TRUE(repo.Info().isBare);
    EXPECT_TRUE(repo.Info().workDir.empty());
}

TEST(RepositoryTest, TryOpenWalksUpToParentRepository)
{
    TestRepo src("discover");
    src.Write("dir/file.txt", "content");
    src.Commit("initial");

    const auto nested = src.root / "dir";
    auto repo = GitCore::Repository::TryOpen(nested.string());
    ASSERT_TRUE(repo.has_value());
    EXPECT_FALSE(repo->Info().isEmpty);
    EXPECT_EQ(repo->Info().headBranch, "main");
    EXPECT_EQ(repo->HeadOid().size(), 40u);
}

TEST(RepositoryTest, TryOpenOutsideRepositoryReturnsNullopt)
{
    TestRepo src("notarepo");
    std::filesystem::remove_all(src.root / ".git");

    EXPECT_FALSE(GitCore::Repository::TryOpen(src.root.string()).has_value());
}

TEST(RepositoryTest, OpenThrowsNotFound)
{
    TestRepo src("opthrow");
    std::filesystem::remove_all(src.root / ".git");

    try
    {
        GitCore::Repository::Open(src.root.string());
        FAIL() << "expected GitException";
    }
    catch (const GitCore::GitException& e)
    {
        EXPECT_EQ(e.ErrorCode(), GIT_ENOTFOUND);
    }
}


