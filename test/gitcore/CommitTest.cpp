#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

#include "GitCore/Commit.h"
#include "GitCore/GitException.h"
#include "GitCore/Log.h"
#include "GitCore/Repository.h"
#include "GitCore/Status.h"
#include "TestRepo.h"

namespace
{
    const GitCore::Identity TestIdentity{"Test User", "test@example.com"};

    // Reads the whole file; used to check that unstaging leaves the working
    // tree alone.
    std::string ReadFile(const std::filesystem::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // Output of a git command, for checking results against ground truth that
    // does not go through libgit2.
    std::string GitOutput(const std::filesystem::path& directory, const std::string& arguments)
    {
        const std::string command =
            "cd '" + directory.string() + "' && git " + arguments + " 2> /dev/null";
        std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe)
            return {};

        std::string output;
        char        buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
            output += buffer;
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
            output.pop_back();
        return output;
    }

    // Looks a file up in the status list; returns its two status letters as
    // "XY" (X: HEAD -> index, Y: index -> worktree, so " M" is an unstaged
    // edit and " ?" an untracked file), or an empty string when the path is
    // not listed at all.
    std::string StatusOf(const std::vector<GitCore::FileStatus>& status, const std::string& path)
    {
        for (const GitCore::FileStatus& entry : status)
        {
            if (entry.path == path)
                return std::string(1, entry.indexStatus) + entry.worktreeStatus;
        }
        return {};
    }
}

TEST(CommitTest, StagesWholeTreeAndCommitsIt)
{
    TestRepo repo("commitall"); // branch main, no commits yet
    repo.Write("a.txt", "a\n");
    repo.Write("sub/b.txt", "b\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    EXPECT_FALSE(GitCore::HasStagedChanges(git)); // untracked files are not staged

    GitCore::Stage(git);
    EXPECT_TRUE(GitCore::HasStagedChanges(git));

    const std::string id = GitCore::CreateCommit(git, "initial commit", TestIdentity);
    EXPECT_EQ(id.size(), 40u);

    const GitCore::CommitInfo commit = GitCore::LookupCommit(git, "HEAD");
    EXPECT_EQ(commit.id, id);
    EXPECT_EQ(commit.summary, "initial commit");
    EXPECT_EQ(commit.authorName, "Test User");
    EXPECT_EQ(commit.authorEmail, "test@example.com");
    EXPECT_TRUE(commit.parentIds.empty());
    EXPECT_FALSE(GitCore::HasStagedChanges(git));

    EXPECT_EQ(StatusOf(GitCore::GetStatus(git), "a.txt"), "");
    EXPECT_EQ(StatusOf(GitCore::GetStatus(git), "sub/b.txt"), "");
}

TEST(CommitTest, FollowsTheHistoryOfTheBranch)
{
    TestRepo repo("commithistory");
    repo.Write("a.txt", "a\n");
    repo.Write("gone.txt", "x\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git);
    const std::string first = GitCore::CreateCommit(git, "first", TestIdentity);

    // One commit with a modification, an addition and a deletion at once.
    repo.Write("a.txt", "changed\n");
    repo.Delete("gone.txt");
    repo.Write("c.txt", "c\n");
    GitCore::Stage(git);
    const std::string second = GitCore::CreateCommit(git, "second", TestIdentity);

    EXPECT_EQ(GitOutput(repo.root, "ls-tree -r --name-only HEAD"), "a.txt\nc.txt");

    const GitCore::CommitInfo commit = GitCore::LookupCommit(git, "HEAD");
    ASSERT_EQ(commit.parentIds.size(), 1u);
    EXPECT_EQ(commit.parentIds[0], first);
    EXPECT_EQ(GitCore::LookupCommit(git, "HEAD~1").summary, "first");
    EXPECT_NE(first, second);

    // A second commit on a clean repository has nothing to record.
    EXPECT_THROW(GitCore::CreateCommit(git, "third", TestIdentity), GitCore::GitException);
}

TEST(CommitTest, StagesDeletionsOfTheWholeTree)
{
    TestRepo repo("commitdelete");
    repo.Write("a.txt", "a\n");
    repo.Write("b.txt", "b\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git);
    GitCore::CreateCommit(git, "two files", TestIdentity);

    repo.Delete("b.txt");
    GitCore::Stage(git);
    GitCore::CreateCommit(git, "one file left", TestIdentity);

    // The commit is clean, and the deletion is not pending in the index.
    EXPECT_TRUE(GitCore::GetStatus(git).empty());
    EXPECT_EQ(GitOutput(repo.root, "ls-tree -r --name-only HEAD"), "a.txt");
}

TEST(CommitTest, StagesOnlyTheRequestedPaths)
{
    TestRepo repo("commitpaths");
    repo.Write("a.txt", "a\n");
    repo.Write("b.txt", "b\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git, {"a.txt"});
    GitCore::CreateCommit(git, "only a", TestIdentity);

    const auto status = GitCore::GetStatus(git);
    EXPECT_EQ(StatusOf(status, "a.txt"), "");  // committed
    EXPECT_EQ(StatusOf(status, "b.txt"), " ?"); // still untracked
}

TEST(CommitTest, UnstageKeepsTheWorkingTreeChange)
{
    TestRepo repo("commitunstage");
    repo.Write("a.txt", "a\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git);
    GitCore::CreateCommit(git, "first", TestIdentity);

    repo.Write("a.txt", "changed\n");
    GitCore::Stage(git);
    EXPECT_TRUE(GitCore::HasStagedChanges(git));

    GitCore::Unstage(git);
    EXPECT_FALSE(GitCore::HasStagedChanges(git));
    EXPECT_EQ(ReadFile(repo.root / "a.txt"), "changed\n");
    EXPECT_EQ(StatusOf(GitCore::GetStatus(git), "a.txt"), " M");
}

TEST(CommitTest, UnstageSinglePathOnly)
{
    TestRepo repo("commitunstageone");
    repo.Write("a.txt", "a\n");
    repo.Write("b.txt", "b\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git);
    GitCore::CreateCommit(git, "first", TestIdentity);

    repo.Write("a.txt", "changed\n");
    repo.Write("b.txt", "changed\n");
    GitCore::Stage(git);
    GitCore::Unstage(git, {"a.txt"});

    const auto status = GitCore::GetStatus(git);
    EXPECT_EQ(StatusOf(status, "a.txt"), " M"); // back to unstaged
    EXPECT_EQ(StatusOf(status, "b.txt"), "M "); // still staged
}

TEST(CommitTest, UnstageOfAnUnbornBranchDropsTheIndex)
{
    TestRepo repo("commitunstageunborn");
    repo.Write("a.txt", "a\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git);
    EXPECT_TRUE(GitCore::HasStagedChanges(git));

    GitCore::Unstage(git);
    EXPECT_FALSE(GitCore::HasStagedChanges(git));
    EXPECT_EQ(ReadFile(repo.root / "a.txt"), "a\n");
    EXPECT_EQ(StatusOf(GitCore::GetStatus(git), "a.txt"), " ?");
}

TEST(CommitTest, EmptyMessageOrEmptyIndexIsRejected)
{
    TestRepo repo("commitempty");
    repo.Write("a.txt", "a\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    EXPECT_THROW(GitCore::CreateCommit(git, "nothing staged", TestIdentity),
                 GitCore::GitException);

    GitCore::Stage(git);
    EXPECT_THROW(GitCore::CreateCommit(git, "", TestIdentity), GitCore::GitException);
    EXPECT_NO_THROW(GitCore::CreateCommit(git, "committed", TestIdentity));
}

TEST(CommitTest, IdentityWithoutEmailIsRejected)
{
    TestRepo repo("commitemail");
    repo.Write("a.txt", "a\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    GitCore::Stage(git);
    EXPECT_THROW(GitCore::CreateCommit(git, "no e-mail", GitCore::Identity{"No Mail", ""}),
                 GitCore::GitException);
}

TEST(CommitTest, IdentityComesFromTheRepositoryConfiguration)
{
    TestRepo repo("commitconfig");
    repo.RequireGit("config user.name 'Repo User'");
    repo.RequireGit("config user.email 'repo@example.com'");
    repo.Write("a.txt", "a\n");

    GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
    const GitCore::Identity identity = GitCore::DefaultIdentity(git);
    EXPECT_EQ(identity.name, "Repo User");
    EXPECT_EQ(identity.email, "repo@example.com");

    GitCore::Stage(git);
    GitCore::CreateCommit(git, "configured author"); // no explicit identity
    const GitCore::CommitInfo commit = GitCore::LookupCommit(git, "HEAD");
    EXPECT_EQ(commit.authorName, "Repo User");
    EXPECT_EQ(commit.committerEmail, "repo@example.com");
}

TEST(CommitTest, MissingIdentityIsReportedClearly)
{
    // Point git and libgit2 at empty configuration files so that the identity
    // of whoever runs the tests cannot leak into the repository.
    const auto tempDir = std::filesystem::temp_directory_path() /
                         ("tgitcore_test_noconfig_" + std::to_string(::getpid()));
    std::filesystem::create_directories(tempDir);
    const std::string emptyConfig = (tempDir / "empty").string();
    {
        std::ofstream create(emptyConfig, std::ios::binary | std::ios::trunc);
    }

    ASSERT_EQ(::setenv("GIT_CONFIG_GLOBAL", emptyConfig.c_str(), 1), 0);
    ASSERT_EQ(::setenv("GIT_CONFIG_SYSTEM", emptyConfig.c_str(), 1), 0);
    ASSERT_EQ(::setenv("XDG_CONFIG_HOME", tempDir.string().c_str(), 1), 0);

    {
        TestRepo repo("commitnoidentity");
        repo.Write("a.txt", "a\n");
        GitCore::Repository git = GitCore::Repository::Open(repo.root.string());
        GitCore::Stage(git);

        EXPECT_TRUE(GitCore::DefaultIdentity(git).empty());

        EXPECT_THROW(GitCore::CreateCommit(git, "no identity"), GitCore::GitException);
        try
        {
            GitCore::CreateCommit(git, "no identity");
        }
        catch (const GitCore::GitException& e)
        {
            // The point of the custom message: the user learns what to set.
            EXPECT_NE(std::string(e.what()).find("user.name"), std::string::npos);
        }

        // An identity given explicitly works without any configuration.
        GitCore::CreateCommit(git, "explicit identity", TestIdentity);
        EXPECT_EQ(GitCore::LookupCommit(git, "HEAD").authorName, "Test User");
    }

    ::unsetenv("GIT_CONFIG_GLOBAL");
    ::unsetenv("GIT_CONFIG_SYSTEM");
    ::unsetenv("XDG_CONFIG_HOME");
    std::filesystem::remove_all(tempDir);
}
