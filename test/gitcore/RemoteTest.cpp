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
