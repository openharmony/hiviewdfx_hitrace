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
const std::string TRACE_SNAPSHOT_PREFIX = "trace_";
const int BUFFER_SIZE = 255;
const int DUMPTRACE_COUNT = 25;
const int SNAPSHOT_FILE_MAX_COUNT = 20;
constexpr uint32_t SLEEP_TIME = 10; // sleep 10ms
constexpr uint32_t TWO_SEC = 2;
constexpr uint32_t TEN_SEC = 10;
constexpr uint32_t MAX_RATIO_UNIT = 1000;
constexpr int DEFAULT_FULL_TRACE_LENGTH = 30;

void AddPair2Table(std::string outputFileName, int nowSec)
{
    std::vector<std::pair<std::string, int>> traceFilesTable = GetTraceFilesTable();
    traceFilesTable.push_back({outputFileName, nowSec});
    SetTraceFilesTable(traceFilesTable);
}

bool CreateFile(std::string outputFileName)
{
    std::ofstream ofs;
    ofs.open(outputFileName, std::ios::out | std::ios::trunc);
    bool openRes = ofs.is_open();
    ofs.close();
    return openRes;
}

void EraseFile(std::string outputFileName)
{
    std::vector<std::pair<std::string, int>> traceFilesTable = GetTraceFilesTable();
    for (auto iter = traceFilesTable.begin(); iter != traceFilesTable.end();) {
        if (strcmp(iter->first.c_str(), outputFileName.c_str())) {
            iter = traceFilesTable.erase(iter);
            continue;
        }
        iter++;
    }
    SetTraceFilesTable(traceFilesTable);
}

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

bool CountSnapShotTraceFile(int& fileCount)
{
    if (access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) != 0) {
        return false;
    }
    DIR* dirPtr = opendir(TRACE_FILE_DEFAULT_DIR.c_str());
    if (dirPtr == nullptr) {
        HILOG_ERROR(LOG_CORE, "Failed to opendir %{public}s.", TRACE_FILE_DEFAULT_DIR.c_str());
        return false;
    }
    struct dirent* ptr = nullptr;
    while ((ptr = readdir(dirPtr)) != nullptr) {
        if (ptr->d_type == DT_REG) {
            std::string name = std::string(ptr->d_name);
            if (name.compare(0, TRACE_SNAPSHOT_PREFIX.size(), TRACE_SNAPSHOT_PREFIX) != 0) {
                continue;
            }
        }
    }
    closedir(dirPtr);
    return true;
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
    ASSERT_EQ(ret.tagGroup, "default");
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
    ASSERT_EQ(ret.tagGroup, "default");
    ASSERT_GE(ret.coverDuration, TWO_SEC - 1);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * TWO_SEC / DEFAULT_FULL_TRACE_LENGTH);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    sleep(TWO_SEC);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr));
    ret = DumpTrace(TEN_SEC, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);
    ASSERT_EQ(ret.tagGroup, "default");
    ASSERT_GE(ret.coverDuration, TWO_SEC - 1);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * TWO_SEC / TEN_SEC);
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

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) + 100; // current time + 100 seconds
    maxDuration = 10;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_FALSE(ret.outputFiles.empty());
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    traceEndTime = static_cast<uint64_t>(std::time(nullptr)) + 1; // current time + 1 seconds
    maxDuration = 10;
    ret = DumpTrace(maxDuration, traceEndTime);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_FALSE(ret.outputFiles.empty());
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
    ASSERT_EQ(ret.tagGroup, "default");
    ASSERT_GE(ret.coverDuration, TWO_SEC - 1);
    ASSERT_GE(ret.coverRatio, MAX_RATIO_UNIT * TWO_SEC / DEFAULT_FULL_TRACE_LENGTH);
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
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    int nowSec = now.tv_sec;
    int nowUsec = now.tv_usec;
    nowSec--;
    std::string outputFileName = TRACE_FILE_DEFAULT_DIR + "trace_" + std::to_string(nowSec)
        + "_" + std::to_string(nowUsec) + ".sys";
    ASSERT_TRUE(CreateFile(outputFileName)) << "create log file failed.";
    HILOG_INFO(LOG_CORE, "outputFileName: %{public}s", outputFileName.c_str());
    AddPair2Table(outputFileName, nowSec);

    TraceRetInfo ret = DumpTrace();
    // Remove outputFileName in g_hitraceFilesTable
    EraseFile(outputFileName);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_EQ(ret.tagGroup, "scene_performance");
    ASSERT_TRUE(TraverseFiles(ret.outputFiles, outputFileName)) << "file created by user is not exists.";
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_003
 * @tc.desc: Verify if files can be deleted in Service_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_003, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    int nowSec = now.tv_sec;
    int nowUsec = now.tv_usec;
    nowSec = nowSec - 1900;
    std::string outputFileName = TRACE_FILE_DEFAULT_DIR + "trace_" + std::to_string(nowSec)
        + "_" + std::to_string(nowUsec) + ".sys";
    ASSERT_TRUE(CreateFile(outputFileName)) << "create log file failed.";
    HILOG_INFO(LOG_CORE, "outputFileName: %{public}s", outputFileName.c_str());
    AddPair2Table(outputFileName, nowSec);

    TraceRetInfo ret = DumpTrace();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_FALSE(TraverseFiles(ret.outputFiles, outputFileName))
        << "Returned files that should have been deleted half an hour ago.";
    ASSERT_TRUE(access(outputFileName.c_str(), F_OK) < 0) << "The file was not deleted half an hour ago";
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_004
 * @tc.desc: Invalid parameter verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_004, TestSize.Level0)
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
 * @tc.name: DumpForServiceMode_005
 * @tc.desc: Enable the service mode in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_005, TestSize.Level0)
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
 * @tc.name: DumpForServiceMode_006
 * @tc.desc: Invalid parameter verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_006, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    SetSysInitParamTags(123);
    ASSERT_TRUE(SetCheckParam() == false);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_007
 * @tc.desc: File aging and deletion function in SERVICE_MODE with commerical version.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_007, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    int fileCount = 0;
    for (int i = 0; i < DUMPTRACE_COUNT; i++) {
        TraceRetInfo ret = DumpTrace();
        ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
        sleep(1);
    }
    if (!IsRootVersion()) {
        ASSERT_TRUE(CountSnapShotTraceFile(fileCount));
        ASSERT_TRUE(fileCount > 0 && fileCount <= SNAPSHOT_FILE_MAX_COUNT);
    }
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForServiceMode_008
 * @tc.desc: Test TRACE_IS_OCCUPIED.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForServiceMode_008, TestSize.Level0)
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
    struct stat beforeStat = GetFileStatInfo(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT);
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::SUCCESS);
    sleep(1);
    TraceRetInfo ret = DumpTraceOff();
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
    ASSERT_TRUE(ret.outputFiles.size() > 0);
    struct stat afterStat = GetFileStatInfo(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT);
    ASSERT_TRUE(afterStat.st_ctime != beforeStat.st_ctime);
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
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

    // ckeck Property("debug.hitrace.tags.enableflags")
    uint64_t openTags = OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
    ASSERT_TRUE(openTags > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    // ckeck Property("debug.hitrace.tags.enableflags")
    uint64_t closeTags = OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
    ASSERT_TRUE(closeTags == 0);
}
} // namespace
