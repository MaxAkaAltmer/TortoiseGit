// TortoiseGit - Linux GUI for Git
// Copyright (C) 2025 - TortoiseGit
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "CrossPlatform/ConfigStore.h"

namespace
{
    // Every test gets its own XDG_CONFIG_HOME so that the config files of
    // different tests (and the developer's real config) never collide.
    class ConfigStoreEnv : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            root = std::filesystem::temp_directory_path() /
                   (std::string("tgit-config-test-") +
                    ::testing::UnitTest::GetInstance()->current_test_info()->name());
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root);
            oldXdg = std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : "";
            setenv("XDG_CONFIG_HOME", root.string().c_str(), 1);
        }

        void TearDown() override
        {
            if (oldXdg.empty())
                unsetenv("XDG_CONFIG_HOME");
            else
                setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
            std::filesystem::remove_all(root);
        }

        std::string ConfigPath(const std::string& appName) const
        {
            return (root / appName / "config.ini").string();
        }

        std::filesystem::path root;

    private:
        std::string oldXdg;
    };

    TEST_F(ConfigStoreEnv, MissingFileIsNotAnError)
    {
        auto store = CConfig::CreateConfigStore("MissingApp");
        EXPECT_TRUE(store->Load());
        CConfig::ConfigValue def;
        def.Set(7);
        EXPECT_EQ(store->Get("Nothing", def).AsInt(), 7);
    }

    TEST_F(ConfigStoreEnv, SaveCreatesDirectoryAndRoundTripsValues)
    {
        {
            auto store = CConfig::CreateConfigStore("RoundTripApp");
            ASSERT_TRUE(store->Load());
            store->Section("General");

            CConfig::ConfigValue limit;
            limit.Set(42);
            store->Set("LogLimit", limit);

            CConfig::ConfigValue path;
            path.Set(std::string("/home/me/my repo"));
            store->Set("DefaultRepoPath", path);

            CConfig::ConfigValue flag;
            flag.Set(true);
            store->Set("ShowRemote", flag);

            ASSERT_TRUE(store->Save());
            EXPECT_TRUE(std::filesystem::exists(ConfigPath("RoundTripApp")));
        }
        {
            auto store = CConfig::CreateConfigStore("RoundTripApp");
            ASSERT_TRUE(store->Load());
            store->Section("General");

            CConfig::ConfigValue intDef;
            intDef.Set(0);
            EXPECT_EQ(store->Get("LogLimit", intDef).AsInt(), 42);

            CConfig::ConfigValue strDef;
            strDef.Set(std::string("fallback"));
            EXPECT_EQ(store->Get("DefaultRepoPath", strDef).AsString(), "/home/me/my repo");

            EXPECT_TRUE(store->Get("ShowRemote", strDef).AsBool());
            EXPECT_EQ(store->Get("Absent", strDef).AsString(), "fallback");
        }
    }

    TEST_F(ConfigStoreEnv, ParsesCommentsSpacesAndValuesWithEquals)
    {
        const std::string file = ConfigPath("ParseApp");
        std::filesystem::create_directories(std::filesystem::path(file).parent_path());
        {
            std::ofstream out(file);
            out << "# a comment\n"
                << "; another comment\n"
                << "\n"
                << "[General]\n"
                << "  Key  =  value with spaces  \n"
                << "Url=https://example.com/?a=b\ncleaned\n";
        }

        auto store = CConfig::CreateConfigStore("ParseApp");
        ASSERT_TRUE(store->Load());
        store->Section("General");

        CConfig::ConfigValue def;
        def.Set(std::string(""));
        EXPECT_EQ(store->Get("Key", def).AsString(), "value with spaces");
        // the line is split at the first '=' only; the trailing "cleaned" line
        // contains no '=' and must be ignored
        EXPECT_EQ(store->Get("Url", def).AsString(), "https://example.com/?a=b");
    }

    TEST_F(ConfigStoreEnv, SectionsAndPopSection)
    {
        {
            auto store = CConfig::CreateConfigStore("SectionApp");
            ASSERT_TRUE(store->Load());
            store->Section("General");
            CConfig::ConfigValue v;
            v.Set(std::string("top"));
            store->Set("Key", v);
            store->Section("Advanced");
            v.Set(std::string("nested"));
            store->Set("Key", v);
            store->PopSection();
            v.Set(std::string("top2"));
            store->Set("Key2", v);
            ASSERT_TRUE(store->Save());
        }

        std::ifstream in(ConfigPath("SectionApp"));
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("[General]"), std::string::npos);
        EXPECT_NE(content.find("[General/Advanced]"), std::string::npos);

        auto store = CConfig::CreateConfigStore("SectionApp");
        ASSERT_TRUE(store->Load());
        store->Section("General");
        CConfig::ConfigValue def;
        def.Set(std::string("?"));
        EXPECT_EQ(store->Get("Key", def).AsString(), "top");
        EXPECT_EQ(store->Get("Key2", def).AsString(), "top2");
        store->Section("Advanced");
        EXPECT_EQ(store->Get("Key", def).AsString(), "nested");
    }
}
