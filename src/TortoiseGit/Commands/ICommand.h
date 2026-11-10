#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <mutex>

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

    // Thread-safe singleton
    static CommandFactory& Instance()
    {
        static CommandFactory instance;
        return instance;
    }

    bool Register(const std::string& name, CreatorFunc creator)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_creators.emplace(name, std::move(creator)).second;
    }

    std::unique_ptr<ICommand> Create(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_creators.find(name);
        if (it != m_creators.end())
            return it->second();
        return nullptr;
    }

    bool Has(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_creators.find(name) != m_creators.end();
    }

    std::vector<std::string> GetRegisteredCommands() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> result;
        result.reserve(m_creators.size());
        for (const auto& [key, _] : m_creators)
            result.push_back(key);
        return result;
    }

private:
    CommandFactory() = default;
    mutable std::mutex m_mutex;
    std::map<std::string, CreatorFunc> m_creators;
};

// Safe inline registration function (avoids ODR violations in headers)
inline void RegisterCommand(const std::string& name, std::function<std::unique_ptr<ICommand>()> creator)
{
    CommandFactory::Instance().Register(name, std::move(creator));
}

template<typename T>
inline void RegisterCommand(const std::string& name)
{
    RegisterCommand(name, []() { return std::make_unique<T>(); });
}

// Example usage:
// class CommitCommand : public ICommand { ... };
// static auto g_commitRegistrar = []{ RegisterCommand<CommitCommand>("commit"); }();
