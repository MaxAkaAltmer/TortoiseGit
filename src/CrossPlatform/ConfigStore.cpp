#include "ConfigStore.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#endif

// ConfigValue implementation
namespace CConfig
{
    int ConfigValue::AsInt() const
    {
        switch (m_type)
        {
            case ValueType::Int: return m_int;
            case ValueType::UInt: return static_cast<int>(m_uint);
            case ValueType::QWord: return static_cast<int>(m_qword);
            case ValueType::Double: return static_cast<int>(m_double);
            case ValueType::Bool: return m_bool ? 1 : 0;
            case ValueType::String: return std::stoi(m_str);
            case ValueType::WString:
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                return std::stoi(converter.to_bytes(m_wstr));
            }
            case ValueType::Binary: return 0;
            default: return 0;
        }
    }

    unsigned int ConfigValue::AsUInt() const
    {
        switch (m_type)
        {
            case ValueType::UInt: return m_uint;
            case ValueType::Int: return static_cast<unsigned int>(m_int);
            case ValueType::QWord: return static_cast<unsigned int>(m_qword);
            case ValueType::Double: return static_cast<unsigned int>(m_double);
            case ValueType::Bool: return m_bool ? 1U : 0U;
            case ValueType::String: return static_cast<unsigned int>(std::stoul(m_str));
            case ValueType::WString:
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                return std::stoul(converter.to_bytes(m_wstr));
            }
            default: return 0;
        }
    }

    long long ConfigValue::AsQWord() const
    {
        switch (m_type)
        {
            case ValueType::QWord: return m_qword;
            case ValueType::Int: return m_int;
            case ValueType::UInt: return m_uint;
            case ValueType::Double: return static_cast<long long>(m_double);
            case ValueType::Bool: return m_bool ? 1LL : 0LL;
            case ValueType::String: return std::stoll(m_str);
            default: return 0;
        }
    }

    double ConfigValue::AsDouble() const
    {
        switch (m_type)
        {
            case ValueType::Double: return m_double;
            case ValueType::Int: return static_cast<double>(m_int);
            case ValueType::UInt: return static_cast<double>(m_uint);
            case ValueType::QWord: return static_cast<double>(m_qword);
            case ValueType::Bool: return m_bool ? 1.0 : 0.0;
            case ValueType::String: return std::stod(m_str);
            default: return 0.0;
        }
    }

    bool ConfigValue::AsBool() const
    {
        switch (m_type)
        {
            case ValueType::Bool: return m_bool;
            case ValueType::Int: return m_int != 0;
            case ValueType::UInt: return m_uint != 0;
            case ValueType::QWord: return m_qword != 0;
            case ValueType::String:
            {
                std::string lower = m_str;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                return lower == "true" || lower == "1" || lower == "yes";
            }
            default: return false;
        }
    }

    std::string ConfigValue::AsString() const
    {
        switch (m_type)
        {
            case ValueType::String: return m_str;
            case ValueType::Int: return std::to_string(m_int);
            case ValueType::UInt: return std::to_string(m_uint);
            case ValueType::QWord: return std::to_string(m_qword);
            case ValueType::Double: return std::to_string(m_double);
            case ValueType::Bool: return m_bool ? "true" : "false";
            default: return "";
        }
    }

    std::wstring ConfigValue::AsWString() const
    {
        switch (m_type)
        {
            case ValueType::WString: return m_wstr;
            case ValueType::String:
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                return converter.from_bytes(m_str);
            }
            case ValueType::Int: return std::to_wstring(m_int);
            case ValueType::UInt: return std::to_wstring(m_uint);
            case ValueType::QWord: return std::to_wstring(m_qword);
            case ValueType::Double: return std::to_wstring(m_double);
            case ValueType::Bool: return m_bool ? L"true" : L"false";
            default: return L"";
        }
    }

    void ConfigValue::Set(int val) { m_type = ValueType::Int; m_int = val; }
    void ConfigValue::Set(unsigned int val) { m_type = ValueType::UInt; m_uint = val; }
    void ConfigValue::Set(long long val) { m_type = ValueType::QWord; m_qword = val; }
    void ConfigValue::Set(double val) { m_type = ValueType::Double; m_double = val; }
    void ConfigValue::Set(bool val) { m_type = ValueType::Bool; m_bool = val; }
    void ConfigValue::Set(const std::string& val) { m_type = ValueType::String; m_str = val; }
    void ConfigValue::Set(const std::wstring& val) { m_type = ValueType::WString; m_wstr = val; }
}

// LinuxConfigStore implementation
namespace CConfig
{
    LinuxConfigStore::LinuxConfigStore(const std::string& appName)
        : m_appName(appName)
    {
    }

    std::string LinuxConfigStore::GetDefaultConfigDir(const std::string& appName)
    {
        const char* xdg = getenv("XDG_CONFIG_HOME");
        std::string configDir;
        if (xdg && *xdg)
            configDir = xdg;
        else
        {
            const char* home = getenv("HOME");
            if (home && *home)
                configDir = home;
            else
                configDir = "/tmp";
            configDir += "/.config";
        }
        configDir += "/" + appName;
        return configDir;
    }

    bool LinuxConfigStore::Load()
    {
        m_loaded = true;
        // Parse INI-style config file
        // TODO: Implement full INI parsing
        return true;
    }

    bool LinuxConfigStore::Save()
    {
        // Ensure config directory exists
        auto configDir = GetDefaultConfigDir(m_appName);
        // TODO: Create directory if not exists

        // Write INI-style config
        std::string configPath = configDir + "/config.ini";
        std::ofstream file(configPath);
        if (!file.is_open())
            return false;

        for (auto& [section, values] : m_sections)
        {
            file << "[" << section << "]" << std::endl;
            for (auto& [key, value] : values)
            {
                file << key << "=" << value.AsString() << std::endl;
            }
            file << std::endl;
        }
        return true;
    }

    ConfigValue LinuxConfigStore::Get(const std::string& key, const ConfigValue& defaultVal)
    {
        auto& section = m_sections[m_currentSection];
        auto it = section.find(key);
        if (it != section.end())
            return it->second;
        return defaultVal;
    }

    void LinuxConfigStore::Set(const std::string& key, const ConfigValue& value)
    {
        m_sections[m_currentSection][key] = value;
    }

    bool LinuxConfigStore::Has(const std::string& key) const
    {
        auto& section = m_sections[m_currentSection];
        return section.find(key) != section.end();
    }

    void LinuxConfigStore::Remove(const std::string& key)
    {
        auto& section = m_sections[m_currentSection];
        section.erase(key);
    }

    void LinuxConfigStore::Clear()
    {
        m_sections.clear();
    }

    std::vector<std::string> LinuxConfigStore::GetKeys() const
    {
        auto& section = m_sections[m_currentSection];
        std::vector<std::string> keys;
        for (auto& [key, _] : section)
            keys.push_back(key);
        return keys;
    }

    std::vector<std::string> LinuxConfigStore::GetSections() const
    {
        std::vector<std::string> sections;
        for (auto& [section, _] : m_sections)
            sections.push_back(section);
        return sections;
    }

    std::string LinuxConfigStore::GetFullKey(const std::string& key) const
    {
        if (m_currentSection.empty())
            return key;
        return m_currentSection + "/" + key;
    }
}
