#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "GitCore/Clone.h"
#include "GitCore/Credentials.h"
#include "GitCore/GitException.h"
#include "GitCore/Log.h"
#include "GitCore/Remote.h"
#include "GitCore/Repository.h"
#include "TestRepo.h"

namespace
{
    std::filesystem::path TempPath(const std::string& name)
    {
        return std::filesystem::temp_directory_path() /
               ("tgitcore_test_" + name + "_" + std::to_string(::getpid()));
    }

    void RemoveTree(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    // Runs the system git in another directory; used to prepare the state of a
    // clone without a commit API in gitcore (the tests must not use the code
    // under test to create their own input).
    void RunGit(const std::string& directory, const std::string& arguments)
    {
        const std::string command = "cd '" + directory + "' && git"
                                    " -c user.name='Test User' -c user.email='test@example.com' " +
                                    arguments + " > /dev/null 2>&1";
        if (std::system(command.c_str()) != 0)
            throw std::runtime_error("git " + arguments + " failed in " + directory);
    }

    void WriteFile(const std::filesystem::path& file, const std::string& content)
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << content;
    }



    // A bare repository that can receive pushes; its HEAD points at main so
    // that the pushed branch is the one gitcore reads back.
    class BareRemote
    {
    public:
        explicit BareRemote(const std::string& name) : path(TempPath(name + "_remote"))
        {
            RemoveTree(path);
            GitCore::Repository::Init(path.string(), true);
            if (std::system(("git --git-dir='" + path.string() +
                             "' symbolic-ref HEAD refs/heads/main > /dev/null 2>&1")
                                .c_str()) != 0)
                throw std::runtime_error("cannot point the test remote at main");
        }

        ~BareRemote() { RemoveTree(path); }

        BareRemote(const BareRemote&) = delete;
        BareRemote& operator=(const BareRemote&) = delete;

        std::filesystem::path path;
    };
}

TEST(RemoteTest, CloneRegistersOriginAndListsIt)
{
    TestRepo source("remotelist");
    source.Write("a.txt", "a\n");
    source.Commit("first");

    const auto target = TempPath("remotelist_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(source.root.string(), target.string());

    const auto remotes = GitCore::ListRemotes(clone);
    ASSERT_EQ(remotes.size(), 1u);
    EXPECT_EQ(remotes[0].name, "origin");
    EXPECT_EQ(GitCore::RemoteUrl(clone), source.root.string());
    // A plain remote has no separate push URL.
    EXPECT_EQ(remotes[0].pushUrl, remotes[0].url);

    EXPECT_THROW(GitCore::RemoteUrl(clone, "no-such-remote"), GitCore::GitException);
    RemoveTree(target);
}

TEST(RemoteTest, UnknownRemoteThrows)
{
    TestRepo source("remoteunknown");
    source.Write("a.txt", "a\n");
    source.Commit("first");

    GitCore::Repository repo = GitCore::Repository::Open(source.root.string());
    EXPECT_THROW(GitCore::Fetch(repo, {}, GitCore::RemoteOptions{}), GitCore::GitException);
    EXPECT_THROW(GitCore::Push(repo, {}, GitCore::RemoteOptions{}), GitCore::GitException);
}

TEST(RemoteTest, FetchUpdatesRemoteTrackingBranch)
{
    TestRepo source("remotefetch");
    source.Write("a.txt", "a\n");
    source.Commit("first");

    const auto target = TempPath("remotefetch_target");
    RemoveTree(target);
    auto clone = GitCore::Clone(source.root.string(), target.string());

    // The new commit appears only after Fetch.
    source.Write("b.txt", "b\n");
    source.Commit("second");

    std::vector<std::string> progress;
    GitCore::RemoteOptions options;
    options.progress = [&](const std::string& line) { progress.push_back(line); };
    GitCore::Fetch(clone, {}, options);

    EXPECT_EQ(GitCore::LookupCommit(clone, "origin/main").summary, "second");
    // The local branch stays where the clone left it until it is merged.
    EXPECT_EQ(GitCore::LookupCommit(clone, "HEAD").summary, "first");
    RemoveTree(target);
}

TEST(RemoteTest, PushPublishesCommitsToTheRemote)
{
    BareRemote remote("remotepush");
    const auto target = TempPath("remotepush_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    WriteFile(target / "a.txt", "a\n");
    RunGit(target.string(), "add -A");
    RunGit(target.string(), "commit -q -m 'from clone'");

    GitCore::Push(clone, {"HEAD:refs/heads/main"});

    GitCore::Repository pushed = GitCore::Repository::Open(remote.path.string());
    const auto log = GitCore::GetLog(pushed, {}, 1);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].summary, "from clone");
    RemoveTree(target);
}

// The local transport reports no sideband messages, so a progress callback
// stays silent here. What the test guards is that installing the callback
// (which shares its payload with the credentials callback) does not disturb
// the transfer.
TEST(RemoteTest, ProgressCallbackDoesNotDisturbTheTransfer)
{
    BareRemote remote("remoteprog");
    const auto target = TempPath("remoteprog_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    WriteFile(target / "a.txt", "a\n");
    RunGit(target.string(), "add -A");
    RunGit(target.string(), "commit -q -m 'progress'");

    GitCore::RemoteOptions options;
    options.progress = [](const std::string&) {};
    GitCore::Push(clone, {"HEAD:refs/heads/main"}, options);

    GitCore::Repository pushed = GitCore::Repository::Open(remote.path.string());
    const auto log = GitCore::GetLog(pushed, {}, 1);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].summary, "progress");
    RemoveTree(target);
}

TEST(CredentialsTest, LocalAndGitUrlsNeedNoBackend)
{
    EXPECT_TRUE(GitCore::IsTransportSupported("/srv/git/repo.git"));
    EXPECT_TRUE(GitCore::IsTransportSupported("file:///srv/git/repo.git"));
    EXPECT_TRUE(GitCore::IsTransportSupported("git://host/repo.git"));
    EXPECT_TRUE(GitCore::IsTransportSupported("./relative/repo"));
    // A Windows drive letter is a path, not a host name.
    EXPECT_TRUE(GitCore::IsTransportSupported("C:/repos/project.git"));
    EXPECT_TRUE(GitCore::MissingTransportHint("/srv/git/repo.git").empty());
}

TEST(CredentialsTest, UrlsMatchTheCompiledInBackends)
{
    EXPECT_EQ(GitCore::IsTransportSupported("https://github.com/tortoisegit/tortoisegit.git"),
              GitCore::SupportsHttps());
    EXPECT_EQ(GitCore::IsTransportSupported("ssh://git@example.com/repo.git"), GitCore::SupportsSsh());
    EXPECT_EQ(GitCore::IsTransportSupported("git@example.com:repo.git"), GitCore::SupportsSsh());

    if (!GitCore::SupportsHttps())
    {
        const std::string hint = GitCore::MissingTransportHint("https://example.com/repo.git");
        EXPECT_FALSE(hint.empty());
        EXPECT_NE(hint.find("https"), std::string::npos);
    }
    if (!GitCore::SupportsSsh())
    {
        const std::string hint = GitCore::MissingTransportHint("git@example.com:repo.git");
        EXPECT_FALSE(hint.empty());
        EXPECT_NE(hint.find("ssh"), std::string::npos);
    }
}

TEST(CredentialsTest, CredentialFactoriesFillTheirFields)
{
    const GitCore::Credential userPassword = GitCore::Credential::FromUserPassword("bob", "secret");
    EXPECT_EQ(userPassword.kind, GitCore::CredentialKind::UsernamePassword);
    EXPECT_EQ(userPassword.userName, "bob");
    EXPECT_EQ(userPassword.password, "secret");

    const GitCore::Credential key =
        GitCore::Credential::FromSshKey("git", "/home/bob/.ssh/id_ed25519", "phrase");
    EXPECT_EQ(key.kind, GitCore::CredentialKind::SshKey);
    EXPECT_EQ(key.userName, "git");
    EXPECT_EQ(key.privateKeyFile, "/home/bob/.ssh/id_ed25519");
    EXPECT_EQ(key.passphrase, "phrase");
    EXPECT_TRUE(key.publicKeyFile.empty()); // defaults to <private key>.pub

    const GitCore::Credential agent = GitCore::Credential::FromAgent("git");
    EXPECT_EQ(agent.kind, GitCore::CredentialKind::SshAgent);
}

TEST(CredentialsTest, LocalRemoteDoesNotAskForCredentials)
{
    TestRepo source("remotecred");
    source.Write("a.txt", "a\n");
    source.Commit("first");

    const auto target = TempPath("remotecred_target");
    RemoveTree(target);
    auto clone = GitCore::Clone(source.root.string(), target.string());

    source.Write("b.txt", "b\n");
    source.Commit("second");

    bool asked = false;
    GitCore::RemoteOptions options;
    options.credentials = [&](const GitCore::CredentialRequest&, GitCore::Credential&) {
        asked = true;
        return false;
    };
    GitCore::Fetch(clone, {}, options);

    EXPECT_FALSE(asked);
    EXPECT_EQ(GitCore::LookupCommit(clone, "origin/main").summary, "second");
    RemoveTree(target);
}

// --- Pull -----------------------------------------------------------------
//
// Pull is tested against a bare remote plus a second clone that publishes the
// commits, so the local repository only ever sees them through the wire.

namespace
{
    // Publishes one commit on main to the bare remote from a throw-away clone.
    void Publish(const std::filesystem::path& remotePath, const std::string& name,
                 const std::string& fileName, const std::string& message)
    {
        const std::filesystem::path workdir = TempPath(name);
        RemoveTree(workdir);

        auto publisher = GitCore::Clone(remotePath.string(), workdir.string());
        WriteFile(workdir / fileName, message + "\n");
        RunGit(workdir.string(), "add -A");
        RunGit(workdir.string(), "commit -q -m '" + message + "'");
        GitCore::Push(publisher, {"HEAD:refs/heads/main"});

        RemoveTree(workdir);
    }

    // Commits a new file in an existing clone with the system git.
    void CommitLocalFile(const std::filesystem::path& workdir, const std::string& fileName,
                         const std::string& message)
    {
        WriteFile(workdir / fileName, message + "\n");
        RunGit(workdir.string(), "add -A");
        RunGit(workdir.string(), "commit -q -m '" + message + "'");
    }

    // Cloning an empty repository starts the clone on init.defaultBranch
    // ("master"), because an unborn HEAD is not advertised by the remote and
    // libgit2 therefore cannot learn which branch git would have used. The
    // tests compare against main, so they name the branch the way
    // `git init -b main` does.
    void UseMainBranch(const std::filesystem::path& workdir)
    {
        RunGit(workdir.string(), "symbolic-ref HEAD refs/heads/main");
    }
}

TEST(RemoteTest, PullFastForwardsBranchAndWorkingTree)
{
    BareRemote remote("remotepull");
    const auto target = TempPath("remotepull_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    CommitLocalFile(target, "a.txt", "first");
    GitCore::Push(clone, {"HEAD:refs/heads/main"});

    Publish(remote.path, "remotepull_publish", "b.txt", "second");

    EXPECT_TRUE(GitCore::Pull(clone));

    // Both the branch and the working tree moved to the remote commit.
    GitCore::Repository reopened = GitCore::Repository::Open(target.string());
    EXPECT_EQ(GitCore::LookupCommit(reopened, "HEAD").summary, "second");
    EXPECT_EQ(GitCore::LookupCommit(reopened, "origin/main").summary, "second");
    EXPECT_TRUE(std::filesystem::exists(target / "b.txt"));
    RemoveTree(target);
}

TEST(RemoteTest, PullUpToDateBranchChangesNothing)
{
    BareRemote remote("remoteuptodate");
    const auto target = TempPath("remoteuptodate_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    CommitLocalFile(target, "a.txt", "only");
    GitCore::Push(clone, {"HEAD:refs/heads/main"});

    EXPECT_TRUE(GitCore::Pull(clone));

    GitCore::Repository reopened = GitCore::Repository::Open(target.string());
    EXPECT_EQ(GitCore::LookupCommit(reopened, "HEAD").summary, "only");

    // Without a checked out branch there is nothing to integrate.
    RunGit(target.string(), "checkout -q --detach HEAD");
    GitCore::Repository detached = GitCore::Repository::Open(target.string());
    EXPECT_THROW(GitCore::Pull(detached), GitCore::GitException);
    RemoveTree(target);
}

TEST(RemoteTest, PullReportsDivergenceInsteadOfMerging)
{
    BareRemote remote("remotediverge");
    const auto target = TempPath("remotediverge_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    CommitLocalFile(target, "a.txt", "base");
    GitCore::Push(clone, {"HEAD:refs/heads/main"});

    Publish(remote.path, "remotediverge_publish", "theirs.txt", "theirs");

    // A commit of its own makes the local branch diverge from the remote one.
    CommitLocalFile(target, "ours.txt", "ours");

    GitCore::Repository before = GitCore::Repository::Open(target.string());
    EXPECT_FALSE(GitCore::Pull(before));

    // The remote commit was fetched, but neither the branch nor the working
    // tree was touched: merging is left to the user.
    GitCore::Repository reopened = GitCore::Repository::Open(target.string());
    EXPECT_EQ(GitCore::LookupCommit(reopened, "HEAD").summary, "ours");
    EXPECT_EQ(GitCore::LookupCommit(reopened, "origin/main").summary, "theirs");
    EXPECT_FALSE(std::filesystem::exists(target / "theirs.txt"));
    RemoveTree(target);
}

TEST(RemoteTest, PullBootstrapsAnUnbornBranch)
{
    BareRemote remote("remoteunborn");
    const auto target = TempPath("remoteunborn_target");
    RemoveTree(target);

    // A clone of an empty remote has a branch without any commit.
    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    Publish(remote.path, "remoteunborn_publish", "a.txt", "published");

    EXPECT_TRUE(GitCore::Pull(clone));

    GitCore::Repository reopened = GitCore::Repository::Open(target.string());
    EXPECT_EQ(GitCore::LookupCommit(reopened, "HEAD").summary, "published");
    EXPECT_TRUE(std::filesystem::exists(target / "a.txt"));
    RemoveTree(target);
}

TEST(RemoteTest, PullDoesNotFailOnAMissingRemoteBranch)
{
    BareRemote remote("remotemissing");
    const auto target = TempPath("remotemissing_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    CommitLocalFile(target, "a.txt", "local only");
    // The remote gets the commit, but under a branch name nobody pulls.
    GitCore::Push(clone, {"HEAD:refs/heads/trunk"});

    GitCore::Repository repo = GitCore::Repository::Open(target.string());
    EXPECT_TRUE(GitCore::Pull(repo));
    EXPECT_EQ(GitCore::LookupCommit(repo, "HEAD").summary, "local only");
    RemoveTree(target);
}

TEST(RemoteTest, PullUsesTheCredentialsProviderOfTheOptions)
{
    TestRepo source("remotepullcred");
    source.Write("a.txt", "a\n");
    source.Commit("first");

    const auto target = TempPath("remotepullcred_target");
    RemoveTree(target);
    auto clone = GitCore::Clone(source.root.string(), target.string());

    source.Write("b.txt", "b\n");
    source.Commit("second");

    // A local transport never asks, so the provider must not be consulted and
    // the pull still has to succeed.
    bool asked = false;
    GitCore::RemoteOptions options;
    options.credentials = [&](const GitCore::CredentialRequest&, GitCore::Credential&) {
        asked = true;
        return false;
    };
    EXPECT_TRUE(GitCore::Pull(clone, options));

    EXPECT_FALSE(asked);
    GitCore::Repository reopened = GitCore::Repository::Open(target.string());
    EXPECT_EQ(GitCore::LookupCommit(reopened, "HEAD").summary, "second");
    EXPECT_TRUE(std::filesystem::exists(target / "b.txt"));
    RemoveTree(target);
}

// --- Push without refspecs ------------------------------------------------

// An empty refspec list means "the checked out branch", like `git push origin`.
TEST(RemoteTest, PushWithoutRefspecsUsesTheCurrentBranch)
{
    BareRemote remote("remotepushdefault");
    const auto target = TempPath("remotepushdefault_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    CommitLocalFile(target, "a.txt", "published branch");

    GitCore::Repository repo = GitCore::Repository::Open(target.string());
    GitCore::Push(repo, {});

    GitCore::Repository pushed = GitCore::Repository::Open(remote.path.string());
    const auto log = GitCore::GetLog(pushed, {}, 1);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].summary, "published branch");
    RemoveTree(target);
}

// Without a branch there is no name to push under, but an explicit refspec
// still works.
TEST(RemoteTest, PushWithoutRefspecsNeedsACheckedOutBranch)
{
    BareRemote remote("remotepushdetached");
    const auto target = TempPath("remotepushdetached_target");
    RemoveTree(target);

    auto clone = GitCore::Clone(remote.path.string(), target.string());
    UseMainBranch(target);
    CommitLocalFile(target, "a.txt", "before detaching");
    RunGit(target.string(), "checkout -q --detach HEAD");

    GitCore::Repository repo = GitCore::Repository::Open(target.string());
    EXPECT_THROW(GitCore::Push(repo, {}), GitCore::GitException);

    GitCore::Push(repo, {"HEAD:refs/heads/detached"});
    GitCore::Repository pushed = GitCore::Repository::Open(remote.path.string());
    EXPECT_EQ(GitCore::LookupCommit(pushed, "refs/heads/detached").summary, "before detaching");
    RemoveTree(target);
}
