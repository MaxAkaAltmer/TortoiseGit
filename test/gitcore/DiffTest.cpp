#include <gtest/gtest.h>

#include "GitCore/Diff.h"
#include "GitCore/Repository.h"
#include "TestRepo.h"

namespace
{
    struct TwoRevs : TestRepo
    {
        TwoRevs()
            : TestRepo("diff")
        {
            Write("a.txt", "line1\nline2\n");
            Commit("first");
            Write("a.txt", "line1\nline2 changed\n");
            Write("b.txt", "new file\n");
            Commit("second");
        }
    };
}

TEST(DiffTest, ChangedFilesBetweenRevisions)
{
    TwoRevs src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto files = GitCore::GetChangedFiles(repo, "HEAD~1", "HEAD");
    ASSERT_EQ(files.size(), 2u);

    // sorted by path: a.txt modified, b.txt added
    EXPECT_EQ(files[0].newPath, "a.txt");
    EXPECT_EQ(files[0].status, 'M');
    // libgit2 fills both paths even for added/deleted files (unlike the
    // /dev/null convention of git-diff plumbing output)
    EXPECT_EQ(files[1].newPath, "b.txt");
    EXPECT_EQ(files[1].oldPath, "b.txt");
    EXPECT_EQ(files[1].status, 'A');
}

TEST(DiffTest, FileFilter)
{
    TwoRevs src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const auto files = GitCore::GetChangedFiles(repo, "HEAD~1", "HEAD", "b.txt");
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].newPath, "b.txt");
}

TEST(DiffTest, DeletedFileUsesOldPath)
{
    TestRepo src("diffdel");
    src.Write("a.txt", "one\n");
    src.Write("b.txt", "two\n");
    src.Commit("initial");
    src.Delete("b.txt");
    src.Commit("removed b");

    auto repo = GitCore::Repository::Open(src.root.string());
    const auto files = GitCore::GetChangedFiles(repo, "HEAD~1", "HEAD");
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].status, 'D');
    EXPECT_EQ(files[0].oldPath, "b.txt");
}

TEST(DiffTest, AgainstWorkingTree)
{
    TwoRevs src;
    src.Write("a.txt", "line1\nlocal modification\n");

    auto repo = GitCore::Repository::Open(src.root.string());
    const auto files = GitCore::GetChangedFiles(repo, "HEAD");
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].newPath, "a.txt");
    EXPECT_EQ(files[0].status, 'M');
}

TEST(DiffTest, UnifiedPatchContent)
{
    TwoRevs src;
    auto repo = GitCore::Repository::Open(src.root.string());

    const std::string patch = GitCore::GetUnifiedPatch(repo, "HEAD~1", "HEAD");
    EXPECT_NE(patch.find("diff --git a/a.txt b/a.txt"), std::string::npos);
    EXPECT_NE(patch.find("+++ b/b.txt"), std::string::npos);
    EXPECT_NE(patch.find("+new file"), std::string::npos);
    EXPECT_NE(patch.find("-line2\n+line2 changed"), std::string::npos);

    const std::string filtered = GitCore::GetUnifiedPatch(repo, "HEAD~1", "HEAD", "a.txt");
    EXPECT_NE(filtered.find("a/a.txt"), std::string::npos);
    EXPECT_EQ(filtered.find("b.txt"), std::string::npos);
}

TEST(DiffTest, EmptyPatchWhenNoChanges)
{
    TwoRevs src;
    auto repo = GitCore::Repository::Open(src.root.string());

    EXPECT_TRUE(GitCore::GetUnifiedPatch(repo, "HEAD", "HEAD").empty());
    EXPECT_TRUE(GitCore::GetChangedFiles(repo, "HEAD", "HEAD").empty());
}
