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

std::string g_traceRootPath;

uint64_t GetNow()
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    return now.tv_sec;
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
        HiLog::Error(LABEL, "Error: Finding trace folder failed");
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

    uint64_t start = GetNow();
    TraceRetInfo ret = DumpTrace();
    uint64_t end = GetNow();
    std::cout << "DumpTrace cost: " << end - start << std::endl;
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

} // namespace