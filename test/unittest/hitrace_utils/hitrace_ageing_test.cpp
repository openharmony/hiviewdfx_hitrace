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
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_CACHE,
        CheckType::FILENUMBER);
    EXPECT_EQ(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_002, TestSize.Level1)
{
    TraceJsonParser::Instance().recordAgeingParam_ = { false, 0, 0 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_RECORDING,
        CheckType::FILENUMBER);
    EXPECT_EQ(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_003, TestSize.Level1)
{
    TraceJsonParser::Instance().recordAgeingParam_ = { true, 1, 0 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_RECORDING,
        CheckType::FILENUMBER);
    EXPECT_NE(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_004, TestSize.Level1)
{
    TraceJsonParser::Instance().recordAgeingParam_ = { true, 0, 1 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_RECORDING,
        CheckType::FILESIZE);
    EXPECT_NE(checker, nullptr);
}

HWTEST_F(HitraceAgeingTest, CreateFileChecker_005, TestSize.Level1)
{
    TraceJsonParser::Instance().recordAgeingParam_ = { true, 0, 0 };
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(TraceDumpType::TRACE_RECORDING,
        CheckType::FILENUMBER);
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
    static constexpr int64_t fileSize = 100 * 1024;
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
    static constexpr int64_t fileSize = 10000 * 1024;
    FileSizeChecker checker(fileSizeMax);
    TraceFileInfo info;
    info.fileSize = fileSize;
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), false);
    EXPECT_EQ(checker.ShouldAgeing(info), true);
}

/**
 * @tc.name: HandleAgeing_001
 * @tc.desc: test recorn model fileageing max filenumber is 3
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_001, TestSize.Level1)
{
    TraceJsonParser::Instance().recordAgeingParam_ = { true, 3, 0 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 5; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/record_trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        vec.push_back(info);

        std::string otherFile = "/data/log/hitrace/record_trace_" + std::to_string(i) + ".b";
        CreateFile(otherFile);
        otherFiles.push_back(otherFile);
    }

    FileAgeingUtils::HandleAgeing(vec, TraceDumpType::TRACE_RECORDING);

    for (const auto& filename : otherFiles) {
        EXPECT_FALSE(std::filesystem::exists(filename));
    }
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/record_trace_0.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/record_trace_0.b"));

    ASSERT_EQ(vec.size(), 3);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}

/**
 * @tc.name: HandleAgeing_002
 * @tc.desc: test recorn model fileageing max filesize is 512Kb
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_002, TestSize.Level1)
{
    TraceJsonParser::Instance().recordAgeingParam_ = { true, 0, 512 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 7; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/record_trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        info.fileSize = 100 * 1024;
        vec.push_back(info);

        std::string otherFile = "/data/log/hitrace/record_trace_" + std::to_string(i) + ".b";
        CreateFile(otherFile);
        otherFiles.push_back(otherFile);
    }

    FileAgeingUtils::HandleAgeing(vec, TraceDumpType::TRACE_RECORDING);

    for (const auto& filename : otherFiles) {
        EXPECT_FALSE(std::filesystem::exists(filename));
    }
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/record_trace_0.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/record_trace_0.b"));

    ASSERT_EQ(vec.size(), 6);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}

/**
 * @tc.name: HandleAgeing_003
 * @tc.desc: test snapshort model fileageing max filenumber is 3
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_003, TestSize.Level1)
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

/**
 * @tc.name: HandleAgeing_004
 * @tc.desc: test snapshort model fileageing max filenumber is 3 and 1 linkfile is set
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_004, TestSize.Level1)
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
    std::string value = "1";
    int ret = TEMP_FAILURE_RETRY(setxattr("/data/log/hitrace/trace_0.a", ATTR_NAME_LINK,
        value.c_str(), value.size(), 0));
    EXPECT_TRUE(ret != -1);

    FileAgeingUtils::HandleAgeing(vec, TraceDumpType::TRACE_SNAPSHOT);

    for (const auto& filename : otherFiles) {
        EXPECT_FALSE(std::filesystem::exists(filename));
    }
    EXPECT_TRUE(std::filesystem::exists("/data/log/hitrace/trace_0.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_1.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_2.a"));

    ASSERT_EQ(vec.size(), 3);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}


/**
 * @tc.name: HandleAgeing_005
 * @tc.desc: test snapshort model fileageing max filenumber is 20 and 15 linkfile is set
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_005, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 20, 0 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 16; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        vec.push_back(info);

        std::string value = "1";
        int ret = TEMP_FAILURE_RETRY(setxattr(info.filename.c_str(), ATTR_NAME_LINK, value.c_str(), value.size(), 0));
        EXPECT_TRUE(ret != -1);

        std::string otherFile = "/data/log/hitrace/trace_" + std::to_string(i) + ".b";
        CreateFile(otherFile);
        otherFiles.push_back(otherFile);
    }

    for (uint32_t i = 16; i < 22; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        info.fileSize = 100 * 1024;
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
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_16.a"));

    ASSERT_EQ(vec.size(), 20);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}

/**
 * @tc.name: HandleAgeing_006
 * @tc.desc: test snapshort model fileageing max filesize is 512Kb
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_006, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 0, 512 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 7; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        info.fileSize = 100 * 1024;
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
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_1.a"));

    ASSERT_EQ(vec.size(), 5);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}


/**
 * @tc.name: HandleAgeing_007
 * @tc.desc: test snapshort model fileageing max filesize is 512Kb and 1 linkfile is set
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_007, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 0, 512 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 8; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        info.fileSize = 100 * 1024;
        vec.push_back(info);

        std::string otherFile = "/data/log/hitrace/trace_" + std::to_string(i) + ".b";
        CreateFile(otherFile);
        otherFiles.push_back(otherFile);
    }

    std::string value = "1";
    int ret = TEMP_FAILURE_RETRY(setxattr("/data/log/hitrace/trace_0.a", ATTR_NAME_LINK,
        value.c_str(), value.size(), 0));
    EXPECT_TRUE(ret != -1);

    FileAgeingUtils::HandleAgeing(vec, TraceDumpType::TRACE_SNAPSHOT);

    for (const auto& filename : otherFiles) {
        EXPECT_FALSE(std::filesystem::exists(filename));
    }
    EXPECT_TRUE(std::filesystem::exists("/data/log/hitrace/trace_0.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_1.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_2.a"));

    ASSERT_EQ(vec.size(), 6);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}

/**
 * @tc.name: HandleAgeing_008
 * @tc.desc: test snapshort model fileageing max filesize is 512Kb and 15 linkfile is set
 * @tc.type: FUNC
*/
HWTEST_F(HitraceAgeingTest, HandleAgeing_008, TestSize.Level1)
{
    TraceJsonParser::Instance().snapShotAgeingParam_ = { true, 0, 400 };
    ClearFile();

    std::vector<TraceFileInfo> vec;
    std::vector<std::string> otherFiles;
    for (uint32_t i = 0; i < 16; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        info.fileSize = 100 * 1024;
        vec.push_back(info);

        std::string value = "1";
        int ret = TEMP_FAILURE_RETRY(setxattr(info.filename.c_str(), ATTR_NAME_LINK, value.c_str(), value.size(), 0));
        EXPECT_TRUE(ret != -1);

        std::string otherFile = "/data/log/hitrace/trace_" + std::to_string(i) + ".b";
        CreateFile(otherFile);
        otherFiles.push_back(otherFile);
    }

    for (uint32_t i = 16; i < 22; i++) {
        TraceFileInfo info;
        info.filename = "/data/log/hitrace/trace_" + std::to_string(i) + ".a";
        CreateFile(info.filename);
        info.fileSize = 100 * 1024;
        vec.push_back(info);
    }

    FileAgeingUtils::HandleAgeing(vec, TraceDumpType::TRACE_SNAPSHOT);

    for (const auto& filename : otherFiles) {
        EXPECT_FALSE(std::filesystem::exists(filename));
    }
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_0.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_16.a"));
    EXPECT_FALSE(std::filesystem::exists("/data/log/hitrace/trace_17.a"));

    ASSERT_EQ(vec.size(), 19);
    for (const auto& info : vec) {
        EXPECT_TRUE(std::filesystem::exists(info.filename));
    }
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS