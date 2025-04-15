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
#include <sstream>
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
constexpr uint32_t S_TO_MS = 1000;
constexpr uint32_t MAX_RATIO_UNIT = 1000;
constexpr int DEFAULT_FULL_TRACE_LENGTH = 30;
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

constexpr int ALIGNMENT_COEFFICIENT = 4;

struct alignas(ALIGNMENT_COEFFICIENT) TraceFileHeader {
    uint16_t magicNumber;
    uint8_t fileType;
    uint16_t versionNumber;
    uint32_t reserved;
};

struct alignas(ALIGNMENT_COEFFICIENT) TraceFileContentHeader {
    uint8_t type;
    uint32_t length;
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
                fileList.push_back({TRACE_FILE_DEFAULT_DIR + fileName, fileStat.st_ctime,
                    static_cast<uint64_t>(fileStat.st_size)});
            }
        }
    }
    std::sort(fileList.begin(), fileList.end(), [](const FileWithTime& a, const FileWithTime& b) {
        return a.ctime < b.ctime;
    });
    return fileList;
}

void DeleteTraceFileInDir(const std::vector<FileWithTime> fileLists)
{
    for (size_t i = 0; i < fileLists.size(); ++i) {
        if (remove(fileLists[i].filename.c_str()) == 0) {
            GTEST_LOG_(INFO) << "remove " << fileLists[i].filename.c_str() << " success.";
        } else {
            GTEST_LOG_(INFO) << "remove " << fileLists[i].filename.c_str() << " fail.";
        }
    }
}

void InitFileFromDir()
{
    DeleteTraceFileInDir(GetTraceFilesInDir(TRACE_CACHE));
    DeleteTraceFileInDir(GetTraceFilesInDir(TRACE_SNAPSHOT));
}

static std::vector<std::string> GetRecordTrace()
{
    std::vector<std::string> result;
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    auto errorCode = OpenTrace(args);
    if (errorCode!= TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetRecordTrace OpenTrace failed, errorCode: " << static_cast<int>(errorCode);
        return result;
    }
    errorCode = RecordTraceOn();
    if (errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetRecordTrace RecordTraceOn failed, errorCode: " << static_cast<int>(errorCode);
        CloseTrace();
        return result;
    }
    sleep(1);
    TraceRetInfo ret = RecordTraceOff();
    if (ret.errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetRecordTrace RecordTraceOff failed, errorCode: " << static_cast<int>(ret.errorCode);
        CloseTrace();
        return result;
    }
    CloseTrace();
    return ret.outputFiles;
}

static std::vector<std::string> GetCacheTrace()
{
    std::vector<std::string> result;
    const std::vector<std::string> tagGroups = {"scene_performance"};
    auto errorCode = OpenTrace(tagGroups);
    if (errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetCacheTrace OpenTrace failed, errorCode: " << static_cast<int>(errorCode);
        return result;
    }
    errorCode = CacheTraceOn();
    if (errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetCacheTrace CacheTraceOn failed, errorCode: " << static_cast<int>(errorCode);
        CloseTrace();
        return result;
    }
    sleep(1);
    TraceRetInfo ret = DumpTrace();
    if (ret.errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetCacheTrace DumpTrace failed, errorCode: " << static_cast<int>(errorCode);
        CloseTrace();
        return result;
    }
    errorCode = CacheTraceOff();
    if (errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetCacheTrace CacheTraceOff failed, errorCode: " << static_cast<int>(errorCode);
        CloseTrace();
        return result;
    }
    CloseTrace();
    return ret.outputFiles;
}

static std::vector<std::string> GetSnapShotTrace()
{
    std::vector<std::string> result;
    const std::vector<std::string> tagGroups = {"scene_performance"};
    auto errorCode = OpenTrace(tagGroups);
    if (errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetSnapShotTrace OpenTrace failed, errorCode: " << static_cast<int>(errorCode);
        return result;
    }
    sleep(1);
    TraceRetInfo ret = DumpTrace();
    if (ret.errorCode != TraceErrorCode::SUCCESS) {
        GTEST_LOG_(INFO) << "GetSnapShotTrace DumpTrace failed, errorCode: " << static_cast<int>(errorCode);
        CloseTrace();
        return result;
    }
    CloseTrace();
    return ret.outputFiles;
}

static bool CheckBaseInfo(const std::string filePath)
{
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1) {
        GTEST_LOG_(INFO) << "open file failed, errno: " << strerror(errno);
        return false;
    }
    if (lseek(fd, sizeof(TraceFileHeader), SEEK_SET) == -1) {
        GTEST_LOG_(INFO) << "lseek failed, errno: " << strerror(errno);
        close(fd);
        return false;
    }

    TraceFileContentHeader contentHeader;
    ssize_t bytesRead = read(fd, &contentHeader, sizeof(TraceFileContentHeader));
    if (bytesRead != static_cast<ssize_t>(sizeof(TraceFileContentHeader))) {
        GTEST_LOG_(INFO) << "read content header failed, errno: " << strerror(errno);
        close(fd);
        return false;
    }
    std::string baseInfo(contentHeader.length, '\0');
    bytesRead = read(fd, &baseInfo[0], contentHeader.length);
    if (bytesRead != static_cast<ssize_t>(contentHeader.length)) {
        GTEST_LOG_(INFO) << "read base info failed, errno: " << strerror(errno);
        close(fd);
        return false;
    }
    close(fd);

    std::vector<std::string> segments;
    std::istringstream iss(baseInfo);
    std::string segment;
    while (std::getline(iss, segment, '\n')) {
        segments.push_back(segment);
    }

    std::vector<std::string> checkList = {
        "KERNEL_VERSION"
    };
    int count = 0;
    for (auto& key : checkList) {
        for (auto& segment : segments) {
            if (segment.find(key) != std::string::npos) {
                count++;
                GTEST_LOG_(INFO) << segment;
                break;
            }
        }
    }
    return (count == checkList.size());
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
 * @tc.desc: test trace state for OpenTrace and CloseTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, GetTraceModeTest_001, TestSize.Level0)
{
    // check OpenTrace(tagGroup)
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    uint8_t traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::OPEN);

    // check close trace
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::CLOSE);

    // check OpenTrace(args)
    std::string args = "tags:sched clockType:boot bufferSize:1024 overwrite:1";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::OPEN);

    // check close trace
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::CLOSE);
}

/**
 * @tc.name: GetTraceModeTest_002
 * @tc.desc: test trace state for RecordOn and RecordOff
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, GetTraceModeTest_002, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_EQ(OpenTrace(tagGroups), TraceErrorCode::SUCCESS);
    ASSERT_EQ(RecordTraceOn(), TraceErrorCode::SUCCESS);
    uint8_t traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::OPEN | TraceMode::RECORD);

    TraceRetInfo ret = RecordTraceOff();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::OPEN);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::CLOSE);
}

/**
 * @tc.name: GetTraceModeTest_003
 * @tc.desc: test trace state for CacheOn and CacheOff
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, GetTraceModeTest_003, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_EQ(OpenTrace(tagGroups), TraceErrorCode::SUCCESS);
    ASSERT_EQ(CacheTraceOn(), TraceErrorCode::SUCCESS);
    uint8_t traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::OPEN | TraceMode::CACHE);

    ASSERT_EQ(CacheTraceOff(), TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::OPEN);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    traceMode = GetTraceMode();
    ASSERT_EQ(traceMode, TraceMode::CLOSE);
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
    sleep(1); // wait 1s
    int maxDuration = 1;
    TraceRetInfo ret = DumpTrace(maxDuration);
    ASSERT_EQ(static_cast<int>(ret.errorCode), static_cast<int>(TraceErrorCode::SUCCESS));
    ASSERT_GT(ret.outputFiles.size(), 0);
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
    sleep(1); // wait 1s
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
    const std::vector<std::string> tagGroups = {"scene_performance"};
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
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(ret.errorCode);
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
    sleep(1); // wait 1s
    uint64_t traceEndTime = static_cast<uint64_t>(std::time(nullptr)) + 10; // current time + 10 seconds
    TraceRetInfo ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_FALSE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(1); // wait 1s
    traceEndTime = 10; // 1970-01-01 08:00:10
    int maxDuration = -1;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(1); // wait 1s
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) + 10; // current time + 10 seconds
    maxDuration = -1;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION);
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
    const std::vector<std::string> tagGroups = {"scene_performance"};
    InitFileFromDir();
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(1); // wait 1s
    uint64_t traceEndTime = 1;
    TraceRetInfo ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    InitFileFromDir();
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(1); // wait 1s
    traceEndTime = 10; // 1970-01-01 08:00:10
    uint64_t maxDuration = 10;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    InitFileFromDir();
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) - 20; // current time - 20 seconds
    ret = DumpTrace(0, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::OUT_OF_TIME);
    ASSERT_TRUE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    InitFileFromDir();
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
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    uint64_t traceEndTime = static_cast<uint64_t>(std::time(nullptr)); // current time

    TraceRetInfo ret = DumpTrace(INT_MAX, traceEndTime);
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    ASSERT_TRUE(!ret.outputFiles.empty());
    ASSERT_EQ(ret.tags, tagGroups);
    ASSERT_GE(ret.coverDuration, (TWO_SEC - 1) * S_TO_MS);
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
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::INVALID_MAX_DURATION) << "errorCode: "
        << static_cast<int>(ret.errorCode);
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
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(ret.errorCode);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_GE(ret.outputFiles.size(), 3); // compare file count
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_010
 * @tc.desc: Test DumpTrace() result in cache_on is opening 40s and slice time is 10s.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_010, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 10s
    ASSERT_TRUE(CacheTraceOn(800, 10) == TraceErrorCode::SUCCESS);
    sleep(40); // wait 40s, over 30s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(ret.errorCode);
    ASSERT_EQ(ret.mode, TraceMode::OPEN | TraceMode::CACHE);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_GE(ret.outputFiles.size(), 3); // at least 3 slices
    // almost fully cover max 30s guaranteed duration
    ASSERT_GE(ret.coverDuration, (DEFAULT_FULL_TRACE_LENGTH - 1) * S_TO_MS);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT - 10); // 1% tolerance
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
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 5s
    ASSERT_TRUE(CacheTraceOn(800, 5) == TraceErrorCode::SUCCESS);
    sleep(8); // wait 8s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(ret.errorCode);
    ASSERT_EQ(ret.mode, TraceMode::OPEN | TraceMode::CACHE);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_GE(ret.outputFiles.size(), 2); // compare file count
    ASSERT_GE(ret.coverDuration, 7 * S_TO_MS); // coverDuration >= 7s
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * 7 / DEFAULT_FULL_TRACE_LENGTH); // coverRatio >= 7/30
    sleep(1);
    ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " <<
        static_cast<int>(ret.errorCode);
    for (int i = 0; i < ret.outputFiles.size(); i++) {
        GTEST_LOG_(INFO) << "outputFiles:" << ret.outputFiles[i].c_str();
    }
    ASSERT_TRUE(ret.outputFiles.size() >= 3); // compare file count
    ASSERT_GE(ret.coverDuration, 8 * S_TO_MS); // coverDuration >= 8s
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * 8 / DEFAULT_FULL_TRACE_LENGTH); // coverRatio >= 8/30
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_012
 * @tc.desc: Test correct calculation of coverDuration and coverRatio for regular DumpTrace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_012, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    sleep(TEN_SEC + 1); // wait 11s before OpenTrace
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TEN_SEC); // wait 10s
    TraceRetInfo ret = DumpTrace(TEN_SEC * 2); // get passed 20s trace
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " <<
        static_cast<int>(ret.errorCode);
    ASSERT_GE(ret.outputFiles.size(), 1);
    ASSERT_GE(ret.coverDuration, (TEN_SEC - 1) * S_TO_MS); // coverDuration >= 9s
    ASSERT_LE(ret.coverDuration, (TEN_SEC + 2) * S_TO_MS); // coverDuration <= 12s
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * (TEN_SEC - 1) / (TEN_SEC * 2)); // coverRatio >= 9/20
    ASSERT_LE(ret.coverRatio, MAX_RATIO_UNIT * (TEN_SEC + 2) / (TEN_SEC * 2)); // coverRatio <= 12/20
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_013
 * @tc.desc: Test correct calculation of coverDuration and coverRatio for DumpTrace during cache.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_013, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    sleep(TEN_SEC + 1); // wait 11s before OpenTrace and CacheTraceOn
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn(800, 10) == TraceErrorCode::SUCCESS);
    sleep(TEN_SEC); // wait 10s
    TraceRetInfo ret = DumpTrace(TEN_SEC * 2); // get passed 20s trace
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " <<
        static_cast<int>(ret.errorCode);
    ASSERT_EQ(ret.mode, TraceMode::OPEN | TraceMode::CACHE);
    ASSERT_GE(ret.outputFiles.size(), 1);
    ASSERT_GE(ret.coverDuration, (TEN_SEC - 1) * S_TO_MS); // coverDuration >= 9s
    ASSERT_LE(ret.coverDuration, (TEN_SEC + 2) * S_TO_MS); // coverDuration <= 12s
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * (TEN_SEC - 1) / (TEN_SEC * 2)); // coverRatio >= 9/20
    ASSERT_LE(ret.coverRatio, MAX_RATIO_UNIT * (TEN_SEC + 2) / (TEN_SEC * 2)); // coverRatio <= 12/20
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpTraceTest_014
 * @tc.desc: Test BaseInfo content in raw trace file.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpTraceTest_014, TestSize.Level0)
{
    std::vector<std::string> outputFiles;
    outputFiles = GetRecordTrace();
    ASSERT_FALSE(outputFiles.empty());
    ASSERT_TRUE(CheckBaseInfo(outputFiles[0])) << outputFiles[0];

    outputFiles = GetCacheTrace();
    ASSERT_FALSE(outputFiles.empty());
    ASSERT_TRUE(CheckBaseInfo(outputFiles[0])) << outputFiles[0];

    outputFiles = GetSnapShotTrace();
    ASSERT_FALSE(outputFiles.empty());
    ASSERT_TRUE(CheckBaseInfo(outputFiles[0])) << outputFiles[0];
}

/**
 * @tc.name: DumpForServiceMode_001
 * @tc.desc: Correct capturing trace using default OpenTrace.
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
 * @tc.desc: Test invalid tag groups for default OpenTrace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_002, TestSize.Level0)
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
 * @tc.name: DumpForServiceMode_003
 * @tc.desc: Enable the service mode in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_003, TestSize.Level0)
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
 * @tc.name: DumpForServiceMode_004
 * @tc.desc: Invalid parameter verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_004, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    SetSysInitParamTags(123);
    ASSERT_TRUE(SetCheckParam() == false);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_005
 * @tc.desc: Test TRACE_IS_OCCUPIED.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_005, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    SetSysInitParamTags(123);
    OHOS::system::SetParameter("debug.hitrace.tags.enableflags", std::to_string(0));
    ASSERT_TRUE(DumpTrace().errorCode == TraceErrorCode::TRACE_IS_OCCUPIED);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_006
 * @tc.desc: Test mix calling RecordTraceOn & RecordTraceOff in opening cache and closing cache.
 * The no arg version DumpTrace() is implicitly tested in other tests.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_006, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(RecordTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);
    TraceRetInfo ret = RecordTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CacheTraceOn() == TraceErrorCode::WRONG_TRACE_MODE);
    ASSERT_TRUE(CacheTraceOff() == TraceErrorCode::WRONG_TRACE_MODE);
}

/**
 * @tc.name: DumpForServiceMode_007
 * @tc.desc: Test all tag groups for default OpenTrace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_007, TestSize.Level0)
{
    ASSERT_TRUE(OpenTrace("default") == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(OpenTrace("scene_performance") == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(OpenTrace("telemetry") == TraceErrorCode::SUCCESS);
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

    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);

    TraceRetInfo ret = RecordTraceOff();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    ASSERT_GE(ret.outputFiles.size(), 0);

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
    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);

    TraceRetInfo ret = RecordTraceOff();
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

    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);

    TraceRetInfo ret = RecordTraceOff();
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
    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    ASSERT_EQ(GetTraceMode(), TraceMode::OPEN | TraceMode::RECORD);
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
    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);

    TraceRetInfo ret = RecordTraceOff();
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

    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);

    TraceRetInfo ret = RecordTraceOff();
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

    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    if (remove(filePathName.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "Delete mylongtrace.sys success.");
    } else {
        HILOG_ERROR(LOG_CORE, "Delete mylongtrace.sys failed.");
    }
    sleep(SLEEP_TIME);

    TraceRetInfo ret = RecordTraceOff();
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

    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);

    TraceRetInfo ret = RecordTraceOff();
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

    TraceErrorCode retCode = RecordTraceOn();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(1);
    TraceRetInfo ret = RecordTraceOff();
    ASSERT_EQ(static_cast<int>(ret.errorCode), static_cast<int>(TraceErrorCode::SUCCESS));
    ASSERT_TRUE(ret.outputFiles.size() > 0);

    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
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

/**
 * @tc.name: TraceFilesTableTest_001
 * @tc.desc: Check files table.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, TraceFilesTableTest_001, TestSize.Level0)
{
    std::vector<std::pair<std::string, int>> data = {{"1", 1}, {"2", 2}, {"3", 3}};
    SetTraceFilesTable(data);
    std::vector<std::pair<std::string, int>> traceFilesTable = GetTraceFilesTable();
    ASSERT_EQ(data.size(), traceFilesTable.size());
    for (size_t index = 0; index < data.size() - 1; index++) {
        EXPECT_EQ(data[index].first, traceFilesTable[index].first);
        EXPECT_EQ(data[index].second, traceFilesTable[index].second);
    }
}
} // namespace
