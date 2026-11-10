#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

// Abstract command interface for TortoiseGit command pattern
class ICommand
{
public:
    virtual ~ICommand() = default;

    // Execute the command
    virtual bool Execute() = 0;

    // Check if command can be executed
    virtual bool CanExecute() const = 0;

    // Get command name
    virtual std::string GetName() const = 0;

    // Get command description
    virtual std::string GetDescription() const { return GetName(); }

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

    CommandParams() = default;
    CommandParams(const std::string& act, const std::vector<std::string>& pths)
        : action(act), paths(pths) {}
};

// Command factory / registry
class CommandFactory
{
public:
    using CreatorFunc = std::function<std::unique_ptr<ICommand>()>;

    static CommandFactory& Instance();

    bool Register(const std::string& name, CreatorFunc creator);
    std::unique_ptr<ICommand> Create(const std::string& name);
    bool Has(const std::string& name) const;
    std::vector<std::string> GetRegisteredCommands() const;

private:
    CommandFactory() = default;
    std::map<std::string, CreatorFunc> m_creators;
};

// Auto-registration helper
template<typename T>
class CommandRegistrar
{
public:
    CommandRegistrar(const std::string& name)
    {
        CommandFactory::Instance().Register(name, [](){
            return std::make_unique<T>();
        });
    }
};

// Example usage:
// class CommitCommand : public ICommand { ... };
// static CommandRegistrar<CommitCommand> g_commitRegistrar("commit");
