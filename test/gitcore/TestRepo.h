#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

// Creates a real repository on disk with the system `git` binary, so the
// tests compare gitcore (libgit2) against ground truth that is independent
// from the library under test.
//
// The repository lives under the system temp dir and is removed in the
// destructor. Give every fixture a unique `name` to avoid clashes when
// tests run in parallel.
class TestRepo
{
public:
    explicit TestRepo(const std::string& name)
        : root(std::filesystem::temp_directory_path() /
               ("tgitcore_test_" + name + "_" + std::to_string(::getpid())))
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        RequireGit("init -b main .");
    }

    ~TestRepo()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    TestRepo(const TestRepo&) = delete;
    TestRepo& operator=(const TestRepo&) = delete;

    // Writes `content` into a worktree-relative file (directories created).
    void Write(const std::string& relPath, const std::string& content) const
    {
        const std::filesystem::path file = root / relPath;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << content;
    }

    void Delete(const std::string& relPath) const
    {
        std::filesystem::remove(root / relPath);
    }

    // `git add -A` + commit with a fixed test identity. Every commit gets a
    // unique, strictly increasing timestamp so that history order does not
    // depend on wall-clock collisions (git log --topo / revwalk by time).
    void Commit(const std::string& message) const
    {
        RequireGit("add -A");
        const std::string ts = std::to_string(1600000000 + ++m_commitSeq * 60);
        if (RunRaw("GIT_AUTHOR_DATE='@" + ts + " +0000'"
                   " GIT_COMMITTER_DATE='@" + ts + " +0000'"
                   " git " + Identity() + "commit -q -m '" + message + "'") != 0)
            throw std::runtime_error("commit '" + message + "' failed in " + root.string());
    }

    // Creates a branch without switching to it.
    void CreateBranch(const std::string& branch, const std::string& rev = "HEAD") const
    {
        RequireGit("branch " + branch + " " + rev);
    }

    int Run(const std::string& args) const
    {
        return RunRaw("git " + args);
    }

    int RunRaw(const std::string& shellCommand) const
    {
        const std::string cmd = "cd '" + root.string() + "' && " + shellCommand +
                                " > /dev/null 2>&1";
        const int rc = std::system(cmd.c_str());
        return rc == -1 ? -1 : WEXITSTATUS(rc);
    }

    void RequireGit(const std::string& args) const
    {
        if (Run(args) != 0)
            throw std::runtime_error("git " + args + " failed in " + root.string());
    }

    std::filesystem::path root;

private:
    mutable int m_commitSeq = 0;

    // Identity is passed via -c so the tests never depend on the user's
    // global or system git config.
    static std::string Identity()
    {
        return "-c user.name='Test User' -c user.email='test@example.com' ";
    }
};
