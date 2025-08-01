/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include <fcntl.h>
#include <gtest/gtest.h>

#include "trace_json_parser.h"

namespace {
class FileNumberChecker;

void CreateFile(const std::string& filename)
{
    std::ofstream file(filename);
}

void ClearFile()
{
    const std::filesystem::path dirPath = "/data/log/hitrace";
    if (std::filesystem::exists(dirPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            std::filesystem::remove_all(entry.path());
        }
    }
}

} // namespace

#include "file_ageing_utils.cpp"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
class HitraceAgeingTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

HWTEST_F(HitraceAgeingTest, CreateFileChecker_001, TestSize.Level1)
{
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_CACHE);
    EXPECT_EQ(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_002, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { false, 0, 0 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_SNAPSHOT);
    EXPECT_EQ(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_003, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 1, 0 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_SNAPSHOT);
    EXPECT_NE(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_004, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 0, 1 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_SNAPSHOT);
    EXPECT_NE(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_005, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 0, 0 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_SNAPSHOT);
    EXPECT_EQ(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, FileNumberChecker_001, TestSize.Level1)
{
    static constexpr int64_t fileNumerCount = 2;
    FileNumberChecker checker(fileNumerCount);
    TraceFileInfo info;
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), true);
}

HWTEST_F(HitraceAgeingTest, FileSizeChecker_001, TestSize.Level1)
{
    static constexpr int64_t fileSizeMax = 300;
    static constexpr int64_t fileSize = 100;
    FileSizeChecker checker(fileSizeMax);
    TraceFileInfo info;
    info.fileSize = fileSize;
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), true);
}

HWTEST_F(HitraceAgeingTest, FileSizeChecker_002, TestSize.Level1)
{
    static constexpr int64_t fileSizeMax = 300;
    static constexpr int64_t fileSize = 10000;
    FileSizeChecker checker(fileSizeMax);
    TraceFileInfo info;
    info.fileSize = fileSize;
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), true);
}


HWTEST_F(HitraceAgeingTest, HandleAgeing_001, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 3, 0 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 5; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        vec.push_back(info);

        std::string otherFile = "/data/log/hitrace/trace_" + std::to_string(i) + ".b";
        CreateFile(otherFile);
        otherFiles.push_back(otherFile);
    }

    FileAgeingUtils::HandleAgeing(vec, TraceDumpType::TRACE_SNAPSHOT);

    for (const auto& filename : otherFiles) {
        EXPECT_FALSE(std::filesystem::exists(filename));
    }
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_0.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_0.b"));

    ASSERT_EQ(vec.size(), 3);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS