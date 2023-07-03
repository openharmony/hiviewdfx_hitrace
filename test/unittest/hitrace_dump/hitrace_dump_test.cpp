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

#include <iostream>
#include <memory>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>
#include <cstdio>
#include "securec.h"

#include "hilog/log.h"
#include "parameters.h"
#include <gtest/gtest.h>

using namespace OHOS::HiviewDFX::Hitrace;
using namespace testing::ext;
using OHOS::HiviewDFX::HiLog;

namespace {

constexpr uint64_t HITRACE_TAG = 0xD002D33;
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HITRACE_TAG, "HitraceTest"};

const std::string TAG_PROP = "debug.hitrace.tags.enableflags";
const std::string DEFAULT_OUTPUT_DIR = "/data/log/hitrace/";
const std::string LOG_DIR = "/data/log/";

std::string g_traceRootPath;

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
        HiLog::Info(LABEL, "ret.outputFile%{public}d: %{public}s", i++, iter->c_str());
    }
    return isExists;
}

class HitraceDumpTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() {}
    void TearDown() {}
};

void HitraceDumpTest::SetUpTestCase()
{
    const std::string debugfsDir = "/sys/kernel/debug/tracing/";
    const std::string tracefsDir = "/sys/kernel/tracing/";
    if (access((debugfsDir + "trace_marker").c_str(), F_OK) != -1) {
        g_traceRootPath = debugfsDir;
    } else if (access((tracefsDir + "trace_marker").c_str(), F_OK) != -1) {
        g_traceRootPath = tracefsDir;
    } else {
        HiLog::Error(LABEL, "Error: Finding trace folder failed.");
    }

    /* Open CMD_MODE */
}

void HitraceDumpTest::TearDownTestCase()
{
    /* Close CMD_MODE */
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
    
    ASSERT_TRUE(TraverseFiles(ret.outputFiles, filePathName));
  
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: DumpForCmdMode_003
 * @tc.desc: Invalid args verification in CMD_MODE.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, DumpForCmdMode_003, TestSize.Level0)
{
    std::string args = "tags:hdcc clockType:boot bufferSize:1024 overwrite:1 ";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::TAG_ERROR);

    args = "tags:hdcc clockType:boot bufferSize:1024 overwrite:1 descriptions:123";
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::TAG_ERROR);
    
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::CALL_ERROR);
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
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::TRACE_IS_OCCUPIED);

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
    ASSERT_TRUE(OpenTrace(args) == TraceErrorCode::CALL_ERROR);
    ASSERT_TRUE(DumpTraceOn() == TraceErrorCode::CALL_ERROR);
    
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
 * @tc.name: ParammeterCheck_001
 * @tc.desc: Check parameter after interface call.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceDumpTest, ParammeterCheck_001, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    // ckeck Property("debug.hitrace.tags.enableflags")
    uint64_t openTags = OHOS::system::GetUintParameter<uint64_t>(TAG_PROP, 0);
    ASSERT_TRUE(openTags > 0);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    // ckeck Property("debug.hitrace.tags.enableflags")
    uint64_t closeTags = OHOS::system::GetUintParameter<uint64_t>(TAG_PROP, 0);
    ASSERT_TRUE(closeTags == 0);
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
    ASSERT_TRUE(access(DEFAULT_OUTPUT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    int nowSec = now.tv_sec;
    int nowUsec = now.tv_usec;
    nowSec--;
    std::string outputFileName = DEFAULT_OUTPUT_DIR + "trace_" + std::to_string(nowSec)
        + "_" + std::to_string(nowUsec) + ".sys";
    ASSERT_TRUE(CreateFile(outputFileName)) << "create log file failed.";
    HiLog::Info(LABEL, "outputFileName: %{public}s", outputFileName.c_str());
    AddPair2Table(outputFileName, nowSec);
    
    TraceRetInfo ret = DumpTrace();
    // Remove outputFileName in g_hitraceFilesTable
    EraseFile(outputFileName);
    ASSERT_TRUE(ret.errorCode == TraceErrorCode::SUCCESS);
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
    ASSERT_TRUE(access(DEFAULT_OUTPUT_DIR.c_str(), F_OK) == 0) << "/data/log/hitrace not exists.";

    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    int nowSec = now.tv_sec;
    int nowUsec = now.tv_usec;
    nowSec = nowSec - 1900;
    std::string outputFileName = DEFAULT_OUTPUT_DIR + "trace_" + std::to_string(nowSec)
        + "_" + std::to_string(nowUsec) + ".sys";
    ASSERT_TRUE(CreateFile(outputFileName)) << "create log file failed.";
    HiLog::Info(LABEL, "outputFileName: %{public}s", outputFileName.c_str());
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
    ASSERT_TRUE(DumpTrace().errorCode == TraceErrorCode::CALL_ERROR);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::CALL_ERROR);
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
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::TRACE_IS_OCCUPIED);
    
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::CALL_ERROR);

    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}
} // namespace
