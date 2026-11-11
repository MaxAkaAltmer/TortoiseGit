#include <gtest/gtest.h>

#include "GitCore/Clone.h"
#include "GitCore/Log.h"
#include "GitCore/GitException.h"
#include "GitCore/Repository.h"
#include "TestRepo.h"

TEST(CloneTest, LocalPathClone)
{
    TestRepo src("clone");
    src.Write("file.txt", "content\n");
    src.Commit("initial");
    const std::string sourceHead = GitCore::Repository::Open(src.root.string()).HeadOid();

    const auto clonePath = src.root.parent_path() /
                           ("tgitcore_test_clone_target_" +
                            std::to_string(::getpid()));
    std::filesystem::remove_all(clonePath);

    std::vector<std::string> progressLines;
    GitCore::RemoteOptions options;
    options.progress = [&](const std::string& line) { progressLines.push_back(line); };
    auto clone = GitCore::Clone(src.root.string(), clonePath.string(), options);

    EXPECT_FALSE(clone.Info().isBare);
    EXPECT_EQ(clone.HeadOid(), sourceHead);

    const auto log = GitCore::GetLog(clone, {}, 0);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].summary, "initial");

    // the clone is a normal repository: it has a remote-tracking branch
    const auto files = std::filesystem::exists(clonePath / ".git");
    EXPECT_TRUE(files);

    std::filesystem::remove_all(clonePath);
}

TEST(CloneTest, CloneNonexistentUrlThrows)
{
    TestRepo src("clonfail");
    const auto clonePath = src.root.parent_path() /
                           ("tgitcore_test_clone_fail_" +
                            std::to_string(::getpid()));
    std::filesystem::remove_all(clonePath);

    const std::string missing = (src.root.parent_path() / "tgitcore_test_no_such_repo").string();
    EXPECT_THROW(GitCore::Clone(missing, clonePath.string()), GitCore::GitException);

    std::filesystem::remove_all(clonePath);
}
