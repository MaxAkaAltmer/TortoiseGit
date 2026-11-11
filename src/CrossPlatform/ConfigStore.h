#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <atlbase.h>
#include <atlsafe.h>
#endif

namespace CConfig
{
    // Config value types
    enum class ValueType
    {
        Int,
        UInt,
        QWord,
        Double,
        Bool,
        String,
        WString,
        Binary
    };

    // Cross-platform value variant
    class ConfigValue
    {
    public:
        ConfigValue() : m_type(ValueType::String) {}
        ~ConfigValue() = default;

        // Getters
        int AsInt() const;
        unsigned int AsUInt() const;
        long long AsQWord() const;
        double AsDouble() const;
        bool AsBool() const;
        std::string AsString() const;
        std::wstring AsWString() const;

        // Setters
        void Set(int val);
        void Set(unsigned int val);
        void Set(long long val);
        void Set(double val);
        void Set(bool val);
        void Set(const std::string& val);
        void Set(const std::wstring& val);

        ValueType GetType() const { return m_type; }
        bool IsEmpty() const { return m_type == ValueType::String && m_str.empty(); }

    private:
        ValueType m_type;
        int m_int{0};
        unsigned int m_uint{0};
        long long m_qword{0};
        double m_double{0.0};
        bool m_bool{false};
        std::string m_str;
        std::wstring m_wstr;
        std::vector<uint8_t> m_binary;
    };

    // Abstract config store interface
    class IConfigStore
    {
    public:
        virtual ~IConfigStore() = default;

        virtual bool Load() = 0;
        virtual bool Save() = 0;

        virtual ConfigValue Get(const std::string& key, const ConfigValue& defaultVal) = 0;
        virtual void Set(const std::string& key, const ConfigValue& value) = 0;
        virtual bool Has(const std::string& key) const = 0;
        virtual void Remove(const std::string& key) = 0;
        virtual void Clear() = 0;

        virtual std::vector<std::string> GetKeys() const = 0;
        virtual std::vector<std::string> GetSections() const = 0;

        // Nested config access
        IConfigStore& Section(const std::string& sectionName);
        void PopSection();

    protected:
        IConfigStore() = default;
        std::string m_currentSection;
        std::vector<std::string> m_sectionStack;
    };

    // Linux implementation: XDG INI-based config store
    class LinuxConfigStore : public IConfigStore
    {
    public:
        LinuxConfigStore(const std::string& appName);
        ~LinuxConfigStore() override = default;

        bool Load() override;
        bool Save() override;

        ConfigValue Get(const std::string& key, const ConfigValue& defaultVal) override;
        void Set(const std::string& key, const ConfigValue& value) override;
        bool Has(const std::string& key) const override;
        void Remove(const std::string& key) override;
        void Clear() override;

        std::vector<std::string> GetKeys() const override;
        std::vector<std::string> GetSections() const override;

        std::string GetConfigPath() const { return m_configPath; }
        static std::string GetDefaultConfigDir(const std::string& appName);

    private:
        std::string GetFullKey(const std::string& key) const;

        std::string m_appName;
        std::string m_configPath;
        std::map<std::string, std::map<std::string, ConfigValue>> m_sections;
        bool m_loaded{false};
    };

    // Windows implementation: Registry-based config store
    class WindowsConfigStore : public IConfigStore
    {
    public:
        WindowsConfigStore(const std::string& appName);
        ~WindowsConfigStore() override = default;

        bool Load() override;
        bool Save() override;

        ConfigValue Get(const std::string& key, const ConfigValue& defaultVal) override;
        void Set(const std::string& key, const ConfigValue& value) override;
        bool Has(const std::string& key) const override;
        void Remove(const std::string& key) override;
        void Clear() override;

        std::vector<std::string> GetKeys() const override;
        std::vector<std::string> GetSections() const override;

    private:
        std::string m_appName;
        std::string m_registryPath;
    };

    // Factory function
    inline std::unique_ptr<IConfigStore> CreateConfigStore(const std::string& appName)
    {
#ifdef _WIN32
        return std::make_unique<WindowsConfigStore>(appName);
#else
        return std::make_unique<LinuxConfigStore>(appName);
#endif
    }
}

// Convenience type aliases for common usage patterns
using ConfigInt = CConfig::ConfigValue;
using ConfigString = CConfig::ConfigValue;
using ConfigBool = CConfig::ConfigValue;

// CReg-like API compatibility
template<typename T>
class CRegValue
{
public:
    explicit CRegValue(T defaultValue) : m_value(defaultValue) {}
    ~CRegValue() = default;

    operator T() const { return m_value; }
    CRegValue& operator=(T val) { m_value = val; return *this; }
    CRegValue& operator+=(T val) { m_value += val; return *this; }
    CRegValue& operator-=(T val) { m_value -= val; return *this; }

private:
    T m_value;
};

using CRegDWORD = CRegValue<unsigned long>;
using CRegString = CRegValue<std::string>;
using CRegBool = CRegValue<bool>;
