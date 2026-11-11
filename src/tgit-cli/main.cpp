// tgit-cli - TortoiseGit console utility for the Linux port (Phase 2)
// Copyright (C) 2025 - TortoiseGit
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <git2.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#    include <termios.h>
#    include <unistd.h>
#endif

#include "GitCore/Branches.h"
#include "GitCore/Clone.h"
#include "GitCore/Credentials.h"
#include "GitCore/Diff.h"
#include "GitCore/GitException.h"
#include "GitCore/Log.h"
#include "GitCore/Remote.h"
#include "GitCore/Repository.h"
#include "GitCore/Status.h"

namespace
{
    constexpr int ExitOk = 0;
    constexpr int ExitGitError = 1;
    constexpr int ExitUsage = 2;
    constexpr int ExitNoRepo = 3;

    struct Options
    {
        std::string command;
        std::string repoPath = ".";
        std::string fromRev;
        std::string fileFilter;
        std::size_t limit = 50;
        std::size_t skip = 0;
        bool json = false;
        bool bare = false;
        bool includeRemote = true;
        bool nameOnly = false;
        // Remote access (clone, fetch, push)
        std::string remoteName = "origin";
        std::string userName;
        std::string password;
        std::string sshKey;
        std::string passphrase;
        std::vector<std::string> positional; // refspecs for fetch/push
    };

    void PrintUsage()
    {
        std::cout <<
            "Usage: tgit-cli <command> [options] [args]\n"
            "\n"
            "Commands:\n"
            "  init <path>               Create a new repository\n"
            "  clone <url> <path>        Clone a repository\n"
            "  status                    Show working tree status\n"
            "  log                       Show commit history\n"
            "  branches                  List branches\n"
            "  show [rev]                Show commit information (default: HEAD)\n"
            "  diff <rev1> [rev2] [file] Show changes between revisions\n"
            "  trust <path>              Add a repository to safe.directory\n"
            "  remote                    List the configured remotes\n"
            "  fetch [refspec...]        Fetch from a remote\n"
            "  push [refspec...]         Push to a remote\n"
            "\n"
            "Options:\n"
            "  -C, --chdir <path>        Run as if started in <path> (default: .)\n"
            "      --json                Machine-readable JSON output\n"
            "  -n, --limit <N>           Limit results (log; 0 = unlimited)\n"
            "      --skip <N>            Skip first N commits (log)\n"
            "      --from <rev>          Start history at <rev> (log)\n"
            "      --path <file>         Restrict to a single path (log, diff)\n"
            "      --name-only           List changed files instead of a patch (diff)\n"
            "      --no-remote           Do not list remote branches (branches)\n"
            "      --bare                Create a bare repository (init)\n"
            "\n"
            "Remote options (clone, fetch, push):\n"
            "      --remote <name>       Remote to use (default: origin)\n"
            "      --username <name>     User name for the remote\n"
            "      --password <secret>   Password or access token (https)\n"
            "      --ssh-key <file>      Private key file (ssh)\n"
            "      --passphrase <secret> Passphrase of the private key\n"
            "Without these options an interactive terminal asks for what the\n"
            "remote requires; ssh-agent and credential helpers are used first.\n"
            "\n"
            "  -h, --help                Show this help\n";
    }

    std::string FormatCommitTime(std::int64_t epoch, int offsetMinutes)
    {
        const std::time_t local = static_cast<std::time_t>(epoch + offsetMinutes * 60LL);
        std::tm tm{};
        gmtime_r(&local, &tm);

        char buf[64];
        if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm) == 0)
            return {};

        const char sign = offsetMinutes < 0 ? '-' : '+';
        const int absOffset = std::abs(offsetMinutes);
        char full[96];
        std::snprintf(full, sizeof(full), "%s %c%02d%02d",
                      buf, sign, absOffset / 60, absOffset % 60);
        return full;
    }

    nlohmann::json CommitInfoToJson(const GitCore::CommitInfo& commit)
    {
        return {
            {"id", commit.id},
            {"shortId", commit.id.substr(0, 8)},
            {"summary", commit.summary},
            {"message", commit.message},
            {"author", {
                {"name", commit.authorName},
                {"email", commit.authorEmail},
                {"time", commit.authorTime},
                {"timeText", FormatCommitTime(commit.authorTime, commit.authorOffsetMinutes)}
            }},
            {"committer", {
                {"name", commit.committerName},
                {"email", commit.committerEmail},
                {"time", commit.commitTime}
            }},
            {"parents", commit.parentIds}
        };
    }

    int CmdInit(const Options& opt)
    {
        if (opt.positional.empty())
        {
            std::cerr << "error: init requires a path\n";
            return ExitUsage;
        }
        GitCore::Repository repo = GitCore::Repository::Init(opt.positional[0], opt.bare);
        std::cout << "Initialized " << (opt.bare ? "bare " : "")
                  << "repository in " << repo.Info().gitDir << '\n';
        return ExitOk;
    }

    bool InteractiveTerminal()
    {
#ifndef _WIN32
        return isatty(STDIN_FILENO) != 0;
#else
        return false;
#endif
    }

    // Reads one line from the terminal. With echo disabled the typed text
    // stays hidden, which matters for passphrases and access tokens.
    bool PromptLine(const std::string& prompt, bool echo, std::string& value)
    {
        std::cerr << prompt << std::flush;
#ifndef _WIN32
        if (echo || !isatty(STDIN_FILENO))
            return static_cast<bool>(std::getline(std::cin, value));

        struct termios original;
        if (tcgetattr(STDIN_FILENO, &original) != 0)
            return static_cast<bool>(std::getline(std::cin, value));

        struct termios hidden = original;
        hidden.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        std::getline(std::cin, value);
        const bool read = !std::cin.fail();
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        std::cerr << '\n' << std::flush;
        return read;
#else
        (void)echo;
        return static_cast<bool>(std::getline(std::cin, value));
#endif
    }

    // Builds the credentials provider from the command line. Without any
    // credential option an interactive terminal is asked for what the remote
    // requires; in a non-interactive run the answers stay empty so that
    // ssh-agent and the credential helpers keep working.
    GitCore::CredentialProvider MakeCredentialProvider(const Options& opt)
    {
        const Options settings = opt;
        return [settings](const GitCore::CredentialRequest& request, GitCore::Credential& credential) {
            const std::string userName =
                !settings.userName.empty() ? settings.userName : request.userName;

            if (!settings.sshKey.empty() && request.allowsSshKey)
            {
                credential = GitCore::Credential::FromSshKey(userName, settings.sshKey,
                                                             settings.passphrase);
                return true;
            }
            if (!settings.password.empty() && request.allowsUsernamePassword)
            {
                credential = GitCore::Credential::FromUserPassword(userName, settings.password);
                return true;
            }
            if (!settings.userName.empty() || !settings.password.empty() || !settings.sshKey.empty())
                return false; // the remote wants something else than what was given

            if (request.allowsAgent)
            {
                credential = GitCore::Credential::FromAgent(userName);
                return true;
            }
            if (!request.allowsUsernamePassword || !InteractiveTerminal())
                return false;

            std::string askedUser = userName;
            if (askedUser.empty() &&
                !PromptLine("Username for '" + request.url + "': ", true, askedUser))
                return false;
            std::string secret;
            if (!PromptLine("Password or token for '" + askedUser + "': ", false, secret))
                return false;

            credential = GitCore::Credential::FromUserPassword(askedUser, secret);
            return true;
        };
    }

    GitCore::RemoteOptions MakeRemoteOptions(const Options& opt)
    {
        GitCore::RemoteOptions options;
        options.progress    = [](const std::string& line) { std::cerr << line; };
        options.remoteName  = opt.remoteName;
        options.credentials = MakeCredentialProvider(opt);
        return options;
    }

    int CmdClone(const Options& opt)
    {
        if (opt.positional.size() < 2)
        {
            std::cerr << "error: clone requires <url> and <path>\n";
            return ExitUsage;
        }
        GitCore::Repository repo = GitCore::Clone(opt.positional[0], opt.positional[1],
                                                  MakeRemoteOptions(opt));
        std::cout << "Cloned into " << repo.Info().workDir << '\n';
        return ExitOk;
    }

    int CmdRemote(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        const auto remotes = GitCore::ListRemotes(repo);

        if (opt.json)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& remote : remotes)
                arr.push_back({{"name", remote.name}, {"url", remote.url}, {"pushUrl", remote.pushUrl}});
            std::cout << arr.dump(2) << '\n';
            return ExitOk;
        }

        if (remotes.empty())
        {
            std::cout << "no remotes\n";
            return ExitOk;
        }
        for (const auto& remote : remotes)
        {
            std::cout << remote.name << '\t' << remote.url;
            if (!remote.pushUrl.empty() && remote.pushUrl != remote.url)
                std::cout << "\tpush: " << remote.pushUrl;
            std::cout << '\n';
        }
        return ExitOk;
    }

    int CmdFetch(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        GitCore::Fetch(repo, opt.positional, MakeRemoteOptions(opt));
        std::cout << "Fetched from '" << opt.remoteName << "'\n";
        return ExitOk;
    }

    int CmdPush(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        GitCore::Push(repo, opt.positional, MakeRemoteOptions(opt));
        std::cout << "Pushed to '" << opt.remoteName << "'\n";
        return ExitOk;
    }

    int CmdTrust(const Options& opt)
    {
        if (opt.positional.empty())
        {
            std::cerr << "error: trust requires a path\n";
            return ExitUsage;
        }
        GitCore::Repository::AddSafeDirectory(opt.positional[0]);
        std::cout << "Added to safe.directory: " << opt.positional[0] << '\n';
        return ExitOk;
    }

    int CmdStatus(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        const auto files = GitCore::GetStatus(repo);

        if (opt.json)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& file : files)
            {
                arr.push_back({
                    {"path", file.path},
                    {"indexStatus", std::string(1, file.indexStatus)},
                    {"worktreeStatus", std::string(1, file.worktreeStatus)}
                });
            }
            std::cout << arr.dump(2) << '\n';
            return ExitOk;
        }

        for (const auto& file : files)
            std::cout << file.indexStatus << file.worktreeStatus << ' ' << file.path << '\n';
        return ExitOk;
    }

    int CmdLog(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        const auto commits = GitCore::GetLog(repo, opt.fileFilter,
                                             opt.limit, opt.skip, opt.fromRev);

        if (opt.json)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& commit : commits)
                arr.push_back(CommitInfoToJson(commit));
            std::cout << arr.dump(2) << '\n';
            return ExitOk;
        }

        for (const auto& commit : commits)
        {
            std::cout << "commit " << commit.id << '\n'
                      << "Author:     " << commit.authorName;
            if (!commit.authorEmail.empty())
                std::cout << " <" << commit.authorEmail << '>';
            std::cout << '\n'
                      << "Date:       "
                      << FormatCommitTime(commit.authorTime, commit.authorOffsetMinutes)
                      << "\n\n";
            for (const char* line = commit.message.c_str();;)
            {
                const char* end = std::strchr(line, '\n');
                std::cout << "    " << std::string(line, end ? static_cast<std::size_t>(end - line)
                                                             : std::strlen(line)) << '\n';
                if (!end)
                    break;
                line = end + 1;
            }
            std::cout << '\n';
        }
        return ExitOk;
    }

    int CmdBranches(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        const auto branches = GitCore::GetBranches(repo, opt.includeRemote);

        if (opt.json)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& branch : branches)
            {
                arr.push_back({
                    {"name", branch.name},
                    {"oid", branch.oid},
                    {"head", branch.isHead},
                    {"remote", branch.isRemote}
                });
            }
            std::cout << arr.dump(2) << '\n';
            return ExitOk;
        }

        for (const auto& branch : branches)
            std::cout << (branch.isHead ? "* " : "  ") << branch.name << '\n';
        return ExitOk;
    }

    int CmdShow(const Options& opt)
    {
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);
        const std::string rev = opt.positional.empty() ? "HEAD" : opt.positional[0];
        const GitCore::CommitInfo commit = GitCore::LookupCommit(repo, rev);

        if (opt.json)
        {
            std::cout << CommitInfoToJson(commit).dump(2) << '\n';
            return ExitOk;
        }

        std::cout << "commit " << commit.id << '\n';
        if (!commit.parentIds.empty())
            std::cout << "Merge: " << commit.parentIds[0].substr(0, 8);
        for (std::size_t i = 1; i < commit.parentIds.size(); ++i)
            std::cout << " " << commit.parentIds[i].substr(0, 8);
        if (!commit.parentIds.empty())
            std::cout << '\n';
        std::cout << "Author:     " << commit.authorName;
        if (!commit.authorEmail.empty())
            std::cout << " <" << commit.authorEmail << '>';
        std::cout << '\n'
                  << "Commit:     " << commit.committerName;
        if (!commit.committerEmail.empty())
            std::cout << " <" << commit.committerEmail << '>';
        std::cout << '\n'
                  << "Date:       "
                  << FormatCommitTime(commit.authorTime, commit.authorOffsetMinutes)
                  << "\n\n" << commit.message;
        if (commit.message.empty() || commit.message.back() != '\n')
            std::cout << '\n';
        return ExitOk;
    }

    int CmdDiff(const Options& opt)
    {
        if (opt.positional.empty())
        {
            std::cerr << "error: diff requires at least one revision\n";
            return ExitUsage;
        }
        GitCore::Repository repo = GitCore::Repository::Open(opt.repoPath);

        const std::string& rev1 = opt.positional[0];
        const std::string rev2 = opt.positional.size() > 1 ? opt.positional[1] : std::string();
        const std::string file =
            !opt.fileFilter.empty()
                ? opt.fileFilter
                : (opt.positional.size() > 2 ? opt.positional[2] : std::string());

        if (opt.nameOnly)
        {
            const auto files = GitCore::GetChangedFiles(repo, rev1, rev2, file);
            if (opt.json)
            {
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& f : files)
                {
                    arr.push_back({
                        {"oldPath", f.oldPath},
                        {"newPath", f.newPath},
                        {"status", std::string(1, f.status)}
                    });
                }
                std::cout << arr.dump(2) << '\n';
                return ExitOk;
            }
            for (const auto& f : files)
                std::cout << f.status << '\t' << f.newPath << '\n';
            return ExitOk;
        }

        if (opt.json)
        {
            std::cout << nlohmann::json{{"patch", GitCore::GetUnifiedPatch(repo, rev1, rev2, file)}}
                             .dump(2)
                      << '\n';
            return ExitOk;
        }

        std::cout << GitCore::GetUnifiedPatch(repo, rev1, rev2, file);
        return ExitOk;
    }

    bool ParseArgs(int argc, char* argv[], Options& opt)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            auto nextValue = [&](const char* flag) -> const char* {
                if (i + 1 >= argc)
                {
                    std::cerr << "error: " << flag << " requires a value\n";
                    return nullptr;
                }
                return argv[++i];
            };

            if (opt.command.empty() && !arg.empty() && arg[0] != '-')
            {
                opt.command = arg;
                continue;
            }

            if (arg == "-h" || arg == "--help")
                return false; // handled as usage
            if (arg == "-C" || arg == "--chdir")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.repoPath = v;
            }
            else if (arg == "--json")
                opt.json = true;
            else if (arg == "--bare")
                opt.bare = true;
            else if (arg == "--remote")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.remoteName = v;
            }
            else if (arg == "--username")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.userName = v;
            }
            else if (arg == "--password")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.password = v;
            }
            else if (arg == "--ssh-key")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.sshKey = v;
            }
            else if (arg == "--passphrase")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.passphrase = v;
            }
            else if (arg == "--no-remote")
                opt.includeRemote = false;
            else if (arg == "--name-only")
                opt.nameOnly = true;
            else if (arg == "-n" || arg == "--limit")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.limit = std::strtoull(v, nullptr, 10);
            }
            else if (arg == "--skip")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.skip = std::strtoull(v, nullptr, 10);
            }
            else if (arg == "--from")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.fromRev = v;
            }
            else if (arg == "--path")
            {
                const char* v = nextValue(arg.c_str());
                if (!v)
                    return false;
                opt.fileFilter = v;
            }
            else if (!arg.empty() && arg[0] == '-')
            {
                std::cerr << "error: unknown option '" << arg << "'\n";
                return false;
            }
            else
                opt.positional.push_back(arg);
        }
        return true;
    }
}

int main(int argc, char* argv[])
{
    Options opt;
    if (!ParseArgs(argc, argv, opt))
    {
        PrintUsage();
        return ExitUsage;
    }
    if (opt.command.empty())
    {
        PrintUsage();
        return ExitUsage;
    }

    try
    {
        if (opt.command == "init")
            return CmdInit(opt);
        if (opt.command == "clone")
            return CmdClone(opt);
        if (opt.command == "remote")
            return CmdRemote(opt);
        if (opt.command == "fetch")
            return CmdFetch(opt);
        if (opt.command == "push")
            return CmdPush(opt);
        if (opt.command == "trust")
            return CmdTrust(opt);
        if (opt.command == "status")
            return CmdStatus(opt);
        if (opt.command == "log")
            return CmdLog(opt);
        if (opt.command == "branches")
            return CmdBranches(opt);
        if (opt.command == "show")
            return CmdShow(opt);
        if (opt.command == "diff")
            return CmdDiff(opt);

        std::cerr << "error: unknown command '" << opt.command << "'\n";
        PrintUsage();
        return ExitUsage;
    }
    catch (const GitCore::GitException& e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return e.ErrorCode() == GIT_ENOTFOUND ? ExitNoRepo : ExitGitError;
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return ExitGitError;
    }
}
