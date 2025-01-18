/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#include "hitrace_dump.h"

#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "common_define.h"
#include "common_utils.h"
#include "hilog/log.h"
#include "parameters.h"
#include "securec.h"

using namespace OHOS::HiviewDFX::Hitrace;
using namespace testing::ext;
using OHOS::HiviewDFX::HiLog;

namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceTest"
#endif
const int BUFFER_SIZE = 255;
constexpr uint32_t SLEEP_TIME = 10; // sleep 10ms
constexpr uint32_t TWO_SEC = 2;
constexpr uint32_t TEN_SEC = 10;
constexpr uint32_t MAX_RATIO_UNIT = 1000;
constexpr uint64_t S_TO_MS = 1000;
constexpr int DEFAULT_FULL_TRACE_LENGTH = 30;
const int BYTE_PER_MB = 1024 * 1024;
const std::string TRACE_SNAPSHOT_PREFIX = "trace_";
const std::string TRACE_RECORDING_PREFIX = "record_trace_";
const std::string TRACE_CACHE_PREFIX = "cache_trace_";
struct FileWithTime {
    std::string filename;
    time_t ctime;
    uint64_t fileSize;
};
enum TRACE_TYPE : uint8_t {
    TRACE_SNAPSHOT = 0,
    TRACE_RECORDING = 1,
    TRACE_CACHE = 2,
};
std::map<TRACE_TYPE, std::string> tracePrefixMap = {
    {TRACE_SNAPSHOT, TRACE_SNAPSHOT_PREFIX},
    {TRACE_RECORDING, TRACE_RECORDING_PREFIX},
    {TRACE_CACHE, TRACE_CACHE_PREFIX},
};

bool TraverseFiles(std::vector<std::string> files, std::string outputFileName)
{
    int i = 1;
    bool isExists = false;
    for (std::vector<std::string>::iterator iter = files.begin(); iter != files.end(); iter++) {
        isExists |= (strcmp(iter->c_str(), outputFileName.c_str()) == 0);
        HILOG_INFO(LOG_CORE, "ret.outputFile%{public}d: %{public}s", i++, iter->c_str());
    }
    return isExists;
}

int HasProcessWithName(const std::string& name)
{
    std::array<char, BUFFER_SIZE> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("ps -ef | grep HitraceDump", "r"), pclose);
    if (pipe == nullptr) {
        HILOG_ERROR(LOG_CORE, "Error: run command failed.");
        return -1;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());
        size_t found = line.rfind(name);
        if (found != std::string::npos && (found == line.length() - name.length() ||
            line[found + name.length()] == ' ')) {
                return 1;
            }
    }
    return 0;
}

struct stat GetFileStatInfo(const std::string& filePath)
{
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0) {
        HILOG_ERROR(LOG_CORE, "Error getting file status: %{public}d.", errno);
    }
    return fileStat;
}

std::vector<FileWithTime> GetTraceFilesInDir(const TRACE_TYPE& traceType)
{
    struct stat fileStat;
    std::vector<FileWithTime> fileList;
    for (const auto &entry : std::filesystem::directory_iterator(TRACE_FILE_DEFAULT_DIR)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        if (fileName.substr(0, tracePrefixMap[traceType].size()) == tracePrefixMap[traceType]) {
            if (stat((TRACE_FILE_DEFAULT_DIR + fileName).c_str(), &fileStat) == 0) {
                fileList.push_back({fileName, fileStat.st_ctime, static_cast<uint64_t>(fileStat.st_size)});
            }
        }
    }
    std::sort(fileList.begin(), fileList.end(), [](const FileWithTime& a, const FileWithTime& b) {
        return a.ctime < b.ctime;
    });
    return fileList;
}

uint64_t GetTotalSizeFromDir(const std::vector<FileWithTime>& fileList)
{
    uint64_t totalFileSize = 0;
    for (auto i = 0; i < fileList.size(); i++) {
        totalFileSize += fileList[i].fileSize;
    }
    return totalFileSize;
}

bool GetDurationFromFileName(const std::string& fileName, uint64_t& duration)
{
    auto index = fileName.find("-");
    if (index == std::string::npos) {
        return false;
    }
    uint32_t number;
    if (sscanf_s(fileName.substr(index, fileName.size() - index).c_str(), "-%u.sys", &number) != 1) {
        HILOG_ERROR(LOG_CORE, "sscanf_s failed.");
        return false;
    }
    duration = static_cast<uint64_t>(number);
    return true;
}

class HitraceDumpTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
};

void HitraceDumpTest::SetUp()
{
    CloseTrace();
}

/**
 * @tc.name: GetTraceModeTest_001
 * @tc.desc: test GetTraceMode() service mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, GetTraceModeTest_001, TestSize.Level0)
{
    // check service mode
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    TraceMode serviceMode = GetTraceMode();
    ASSERT_TRUE(serviceMode == TraceMode::SERVICE_MODE);

    // check close mode
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    TraceMode closeMode = GetTraceMode();
    ASSERT_TRUE(closeMode == TraceMode::CLOSE);
}

/**
 * @tc.name: GetTraceModeTest_002
 * @tc.desc: test GetTraceMode() cmd mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, GetTraceModeTest_002, TestSize.Level0)
{
    // check cmd mode
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    TraceMode cmdMode = GetTraceMode();
    ASSERT_TRUE(cmdMode == TraceMode::CMD_MODE);

    // check close mode
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    TraceMode closeMode = GetTraceMode();
    ASSERT_TRUE(closeMode == TraceMode::CLOSE);
}

/**
 * @tc.name: DumpTraceTest_001
 * @tc.desc: Test DumpTrace(int maxDuration) for valid input.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_001, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    int maxDuration = 1;
    TraceRetInfo ret = DumpTrace(maxDuration);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);
    ASSERT_EQ(ret.tags, tagGroups);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_002
 * @tc.desc: Test DumpTrace(int maxDuration) for invalid input.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_002, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    int maxDuration = -1;
    TraceRetInfo ret = DumpTrace(maxDuration);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_003
 * @tc.desc: Test DumpTrace(int maxDuration, uint64_t happenTime) for valid input.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_003, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC); // need at least one second of trace in cpu due to the input unit of 1 second to avoid OUT_OF_TIME.
    uint64_t traceEndTime = static_cast<uint64_t>(std::time(nullptr));
    TraceRetInfo ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);
    ASSERT_EQ(ret.tags, tagGroups);
    ASSERT_GE(ret.coverDuration, TWO_SEC - 1);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * (TWO_SEC - 1) / DEFAULT_FULL_TRACE_LENGTH);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr));
    ret = DumpTrace(TEN_SEC, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);
    ASSERT_EQ(ret.tags, tagGroups);
    ASSERT_GE(ret.coverDuration, TWO_SEC - 1);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * (TWO_SEC - 1) / TEN_SEC);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_004
 * @tc.desc: Test DumpTrace(int maxDuration, uint64_t happenTime) for invalid input.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_004, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    uint64_t traceEndTime = 1;
    TraceRetInfo ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) + 10; // current time + 10 seconds
    ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_FALSE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    traceEndTime = 10; // 1970-01-01 08:00:10
    int maxDuration = -1;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) + 10; // current time + 10 seconds
    maxDuration = -1;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    traceEndTime = 10;
    maxDuration = 10;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_005
 * @tc.desc: Test DumpTrace(int maxDuration, uint64_t happenTime) for OUT_OF_TIME.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_005, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    uint64_t traceEndTime = static_cast<uint64_t>(std::time(nullptr)) - 20; // current time - 20 seconds
    TraceRetInfo ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) - 20; // current time - 20 seconds
    ret = DumpTrace(TEN_SEC, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_006
 * @tc.desc: Test DumpTrace(int maxDuration, uint64_t happenTime) for maxDuration is bigger than boot_time
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_006, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    uint64_t traceEndTime = static_cast<uint64_t>(std::time(nullptr)); // current time

    TraceRetInfo ret = DumpTrace(INT_MAX, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(!ret.outputFiles.empty());
    ASSERT_EQ(ret.tags, tagGroups);
    ASSERT_GE(ret.coverDuration, TWO_SEC - 1);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * (TWO_SEC - 1) / DEFAULT_FULL_TRACE_LENGTH);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_007
 * @tc.desc: Test DumpTrace(int maxDuration, uint64_t happenTime) for INVALID_MAX_DURATION.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_007, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    uint64_t traceEndTime = static_cast<uint64_t>(std::time(nullptr));
    TraceRetInfo ret = DumpTrace(-1, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_008
 * @tc.desc: Test Test DumpTrace(int maxDuration) for check process is recycled.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_008, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    for (int i = 0; i < 10; i++) {
        int maxDuration = 1;
        TraceRetInfo ret = DumpTrace(maxDuration);
        ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
        sleep(1);
        ASSERT_TRUE(HasProcessWithName("HitraceDump") == 0);
    }
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_009
 * @tc.desc: Test DumpTrace() result in cache_on is opening 8s and slice time is 5s.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_009, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 5s
    ASSERT_TRUE(CacheTraceOn(800, 5) == TraceErrorCode::SUCCESS);
    sleep(8); // wait 8s
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::SUCCESS);
    sleep(2); // wait 2s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_TRUE(ret.outputFiles.size() >= 3); // compare file count
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_010
 * @tc.desc: Test DumpTrace() result in cache_on is opening 50s and slice time is 10s.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_010, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 5s
    ASSERT_TRUE(CacheTraceOn(800, 10) == TraceErrorCode::SUCCESS);
    sleep(40); // wait 40s, over 30s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_TRUE(ret.outputFiles.size() >= 3); // compare file count
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_011
 * @tc.desc: Test result when calling DumpTrace() twice with cache_on is opening 20s and slice time is 5s.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_011, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 5s
    ASSERT_TRUE(CacheTraceOn(800, 5) == TraceErrorCode::SUCCESS);
    sleep(8); // wait 8s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_TRUE(ret.outputFiles.size() >= 2); // compare file count
    sleep(1);
    TraceRetInfo retNext = DumpTrace();
    ASSERT_EQ(retNext.errorCode, TraceErrorCode::SUCCESS);
    for (int i = 0; i < retNext.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << retNext.outputFiles[i].c_str();
    }
    ASSERT_TRUE(retNext.outputFiles.size() >= 3); // compare file count
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_012
 * @tc.desc: Test CacheTraceOn() is opening, then calling DumpTraceOn and DumpTraceOff.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_012, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);
    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_013
 * @tc.desc: Test CacheTraceOn() when OpenTrace is not ruinng and CacheTraceOff() when CloseTrace is runing.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_013, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::WRONG_TRACE_MODE);
}

/**
 * @tc.name: DumpTraceTest_014
 * @tc.desc: Test aging cache trace file when OpenTrace over 30s.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_014, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 5s
    ASSERT_TRUE(CacheTraceOn(800, 5) == TraceErrorCode::SUCCESS);
    sleep(8); // wait 8s
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    sleep(30); // wait 30s: start aging file
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    std::vector<FileWithTime> fileList;
    ASSERT_EQ(GetTraceFilesInDir(TRACE_CACHE).size(), 0); // no cache trace file
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_015
 * @tc.desc: Test aging cache trace file when file size overflow
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_015, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 5MB, sliceduration: 2s
    ASSERT_TRUE(CacheTraceOn(5, 2) == TraceErrorCode::SUCCESS);
    sleep(10); // wait 10s
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::SUCCESS);
    std::vector<FileWithTime> fileList = GetTraceFilesInDir(TRACE_CACHE);
    if (fileList.empty()) {
        ASSERT_LT(GetTotalSizeFromDir(fileList), 6 * BYTE_PER_MB); // aging file in 6MB
    }
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_016
 * @tc.desc: Test runing CacheTraceOn() twice
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_016, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_001
 * @tc.desc: The correct usage of grasping trace in SERVICE_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_001, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    TraceRetInfo ret = DumpTrace();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_002
 * @tc.desc: Verify if files can be returned as expected in Service_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_002, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 5s
    ASSERT_TRUE(CacheTraceOn(800, 5) == TraceErrorCode::SUCCESS);
    sleep(8); // wait 8s
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::SUCCESS);
    TraceRetInfo ret = DumpTrace();
    uint64_t totalDuartion = 0;
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        uint64_t duration = 0;
        ASSERT_TRUE(GetDurationFromFileName(ret.outputFiles[i], duration) == true);
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
        GTEST_LOG_(INFO) << "duration:" << duration;
        totalDuartion += duration;
    }
    totalDuartion /= S_TO_MS;

    ASSERT_TRUE(totalDuartion >= 8); // total trace duration over 8s
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_003
 * @tc.desc: Invalid parameter verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_003, TestSize.Level0)
{
    const std::vector<std::string> tagGroups;
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::TAG_ERROR);

    const std::vector<std::string> tagGroups1 = {"scene_performance1"};
    ASSERT_TRUE(OpenTrace(tagGroups1) == TraceErrorCode::TAG_ERROR);
    TraceRetInfo ret = DumpTrace();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_004
 * @tc.desc: Enable the service mode in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_004, TestSize.Level0)
{
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::WRONG_TRACE_MODE);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::WRONG_TRACE_MODE);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_005
 * @tc.desc: Invalid parameter verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_005, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    SetSysInitParamTags(123);
    ASSERT_TRUE(SetCheckParam() == false);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_006
 * @tc.desc: Test TRACE_IS_OCCUPIED.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_006, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    SetSysInitParamTags(123);
    OHOS::system::SetParameter("debug.hitrace.tags.enableflags", std::to_string(0));
    ASSERT_TRUE(DumpTrace().errorCode == TraceErrorCode::TRACE_IS_OCCUPIED);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_001
 * @tc.desc: The correct usage of grasping trace in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_001, TestSize.Level0)
{
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_002
 * @tc.desc: Specifies the path of the command in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_002, TestSize.Level0)
{
    std::string filePathName = "/data/local/tmp/mytrace.sys";
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1 output:" + filePathName;
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(TraverseFiles(ret.outputFiles, filePathName))
        << "unspport set outputfile, default generate file in /data/log/hitrace.";

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_003
 * @tc.desc: Invalid args verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_003, TestSize.Level0)
{
    std::string args = "clockType:boot bufferSize:1024 overwrite:1 ";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    args = "tags:hdc clockType:boot bufferSize:1024 overwrite:1 descriptions:123";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::TAG_ERROR);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_004
 * @tc.desc: The CMD_MODE cannot be interrupted by the SERVICE_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_004, TestSize.Level0)
{
    std::string args = "tags:memory clockType:boot1 bufferSize:1024 overwrite:0";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::WRONG_TRACE_MODE);

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    args = "tags:memory clockType: bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    args = "tags:memory clockType:perf bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_005
 * @tc.desc: Enable the cmd mode in non-close mode.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_005, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_006
 * @tc.desc: Test the CMD_MODE when there's extra space in args.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_006, TestSize.Level0)
{
    std::string args = "tags: sched clockType: boot bufferSize:1024 overwrite: 1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_007
 * @tc.desc: Test the CMD_MODE when set fileLimit in args.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_007, TestSize.Level0)
{
    std::string args = "tags:sched,ace,app,disk,distributeddatamgr,freq,graphic,idle,irq,load,mdfs,mmc,";
    args += "notification,ohos,pagecache,regulators,sync,ufs,workq,zaudio,zcamera,zimage,zmedia ";
    args += "clockType: boot bufferSize:1024 overwrite: 1 fileLimit: 2 fileSize: 51200";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_008
 * @tc.desc: Test the CMD_MODE when there's extra space in args.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_008, TestSize.Level0)
{
    std::string filePathName = "/data/local/tmp/mylongtrace.sys";
    std::string args = "tags:sched,ace,app,disk,distributeddatamgr,freq,graphic,idle,irq,load,mdfs,mmc,";
    args += "notification,ohos,pagecache,regulators,sync,ufs,workq,zaudio,zcamera,zimage,zmedia ";
    args += "clockType: boot bufferSize:1024 overwrite: 1 output:" + filePathName;
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    if (remove(filePathName.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "Delete mylongtrace.sys success.");
    } else {
        HILOG_ERROR(LOG_CORE, "Delete mylongtrace.sys failed.");
    }
    sleep(SLEEP_TIME);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_009
 * @tc.desc: Test the CMD_MODE when set fileLimit in args.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_009, TestSize.Level0)
{
    std::string args = "tags:sched,ace,app,disk,distributeddatamgr,freq,graphic,idle,irq,load,mdfs,mmc,";
    args += "notification,ohos,pagecache,regulators,sync,ufs,workq,zaudio,zcamera,zimage,zmedia ";
    args += "clockType: boot bufferSize:1024 overwrite: 1 fileLimit: 2";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_010
 * @tc.desc: Test the multi-process task CMD_MODE when set fileLimit in args.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_010, TestSize.Level0)
{
    std::string args = "tags:sched,ace,app,disk,distributeddatamgr,freq,graphic,idle,irq,load,mdfs,mmc,";
    args += "notification,ohos,pagecache,regulators,sync,ufs,workq,zaudio,zcamera,zimage,zmedia ";
    args += "clockType: boot bufferSize:1024 overwrite: 1 fileLimit: 2";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);

    pid_t pid = fork();
    if (pid < 0) {
        HILOG_ERROR(LOG_CORE, "fork error.");
    } else if (pid == 0) {
        ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::WRONG_TRACE_MODE);
        _exit(EXIT_SUCCESS);
    }

    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);

    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_011
 * @tc.desc: Test the multi-process task SERVICE_MODE and Enable the cmd mode in non-close mode.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_011, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    pid_t pid = fork();
    if (pid < 0) {
        HILOG_ERROR(LOG_CORE, "fork error.");
    } else if (pid == 0) {
        ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::WRONG_TRACE_MODE);
        _exit(EXIT_SUCCESS);
    }

    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_012
 * @tc.desc: Test saved_events_format regenerate.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_012, TestSize.Level0)
{
    std::string args = "tags: sched clockType: boot bufferSize:1024 overwrite: 1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    sleep(1);
    struct stat beforeStat = GetFileStatInfo(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    sleep(1);
    struct stat afterStat = GetFileStatInfo(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(afterStat.st_ctime != beforeStat.st_ctime);
}

/**
 * @tc.name: ParammeterCheck_001
 * @tc.desc: Check parameter after interface call.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, ParammeterCheck_001, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    // check Property("debug.hitrace.tags.enableflags")
    uint64_t openTags = OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
    ASSERT_TRUE(openTags > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    // check Property("debug.hitrace.tags.enableflags")
    uint64_t closeTags = OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
    ASSERT_TRUE(closeTags == 0);
}
} // namespace
