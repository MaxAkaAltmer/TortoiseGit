#pragma once
#include <string>
#include <vector>

// Abstract command interface for TortoiseGit command pattern
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual bool Execute() = 0;
    virtual bool CanExecute() const = 0;
    virtual std::string GetName() const = 0;

protected:
    ICommand() = default;
};

// Command parameter structure
struct CommandParams
{
    std::string action;        // commit, log, diff, merge, etc.
    std::vector<std::string> paths;
    std::string url;
    std::string revision;
    std::string config;
    bool debug{false};
    bool verbose{false};
    bool quiet{false};
    std::string mergeTool;
    std::string diffTool;

    // Default constructor
    CommandParams() = default;
    CommandParams(const std::string& act, const std::vector<std::string>& pths)
        : action(act), paths(pths) {}
};
