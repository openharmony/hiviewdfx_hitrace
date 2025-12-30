/*
 * Copyright (C) 2022-2024 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <gtest/gtest.h>

#include "hitrace_meter_test_utils.h"
#include "common_define.h"
#include "common_utils.h"
#include "securec.h"
#include "trace.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX::Hitrace;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceTest"
#endif

constexpr uint64_t TAG = (1ULL << 62); // HITRACE_TAG_APP
static char g_pid[6];

class HitraceMeterNDKTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void HitraceMeterNDKTest::SetUpTestCase(void)
{
    std::string pidStr = std::to_string(getprocpid());
    int ret = strcpy_s(g_pid, sizeof(g_pid), pidStr.c_str());
    if (ret != 0) {
        HILOG_ERROR(LOG_CORE, "pid[%{public}s] strcpy_s fail ret: %{public}d.", pidStr.c_str(), ret);
        return;
    }
    ASSERT_TRUE(Init(g_pid));
    ASSERT_TRUE(CleanFtrace());
}

void HitraceMeterNDKTest::TearDownTestCase(void)
{
    SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0");
    SetFtrace(TRACING_ON_NODE, false);
    CleanTrace();
}

void HitraceMeterNDKTest::SetUp(void)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON_NODE, true)) << "SetUp: Setting tracing_on failed.";
    std::string value = std::to_string(TAG);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value)) << "SetUp: Setting enableflags failed.";
    HILOG_INFO(LOG_CORE, "current tag is %{public}s", GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0").c_str());
    ASSERT_TRUE(GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "-123") == value);
}

void HitraceMeterNDKTest::TearDown(void)
{
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0")) << "TearDown: Setting enableflags failed.";
    ASSERT_TRUE(SetFtrace(TRACING_ON_NODE, false)) << "TearDown: Setting tracing_on failed.";
    ASSERT_TRUE(CleanTrace()) << "TearDown: Cleaning trace failed.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest001
 * @tc.desc: Testing OH_HiTrace_StartTraceEx and OH_HiTrace_FinishTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest001: start.";

    const char* name = "HitraceMeterNDKInterfaceTest001";
    const char* customArgs = "key=value";

    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, name, customArgs);
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest001: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest002
 * @tc.desc: Testing OH_HiTrace_StartTrace and OH_HiTrace_FinishTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest002: start.";

    const char* name = "HitraceMeterNDKInterfaceTest002";

    OH_HiTrace_StartTrace(name);
    OH_HiTrace_FinishTrace();

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest002: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest003
 * @tc.desc: Testing OH_HiTrace_StartAsyncTraceEx and OH_HiTrace_FinishAsyncTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest003: start.";

    const char* name = "HitraceMeterNDKInterfaceTest003";
    const char* customArgs = "key=value";
    const char* customCategory = "test";
    int32_t taskId = 3;

    OH_HiTrace_StartAsyncTraceEx(HITRACE_LEVEL_INFO, name, taskId, customCategory, customArgs);
    OH_HiTrace_FinishAsyncTraceEx(HITRACE_LEVEL_INFO, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name, customCategory, customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest003: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest004
 * @tc.desc: Testing OH_HiTrace_StartAsyncTrace and OH_HiTrace_FinishAsyncTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest004: start.";

    const char* name = "HitraceMeterNDKInterfaceTest004";
    int32_t taskId = 4;

    OH_HiTrace_StartAsyncTrace(name, taskId);
    OH_HiTrace_FinishAsyncTrace(name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest004: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest005
 * @tc.desc: Testing OH_HiTrace_CountTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest005: start.";

    const char* name = "HitraceMeterNDKInterfaceTest005";
    int64_t count = 5;

    OH_HiTrace_CountTraceEx(HITRACE_LEVEL_INFO, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_INFO, TAG, count, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest005: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest006
 * @tc.desc: Testing OH_HiTrace_CountTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest006: start.";

    const char* name = "HitraceMeterNDKInterfaceTest006";
    int64_t count = 6;

    OH_HiTrace_CountTrace(name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_COMMERCIAL, TAG, count, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest006: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest007
 * @tc.desc: Testing OH_HiTrace_IsTraceEnabled
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest007, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest007: start.";

    ASSERT_TRUE(OH_HiTrace_IsTraceEnabled());
    const uint64_t tag = (1ULL << 30); // HITRACE_TAG_OHOS
    std::string value = std::to_string(tag);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value));
    ASSERT_FALSE(OH_HiTrace_IsTraceEnabled());

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest007: end.";
}

/**
 * @tc.name: HitraceMeterNDKInterfaceTest008
 * @tc.desc: Testing normal trace switch notification callback register and unregister
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterNDKTest, HitraceMeterNDKInterfaceTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest008: start.";

    int32_t ret = OH_HiTrace_RegisterTraceListener([](bool enable) {
        GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest008 listener, enable: " << enable;
    });
    ASSERT_EQ(ret, 0);

    ret = OH_HiTrace_UnregisterTraceListener(ret);
    ASSERT_EQ(ret, 0);

    GTEST_LOG_(INFO) << "HitraceMeterNDKInterfaceTest008: end.";
}
}
}
}
