/*
 * Copyright (C) 2022-2025 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <sys/stat.h>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_meter.h"
#include "hitrace_meter_test_utils.h"
#include "hitrace/tracechain.h"
#include "hitracechain_ffi.h"
#include "hitracemeter_ffi.h"

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

constexpr uint64_t TAG = HITRACE_TAG_APP;
const std::string LABEL_HEADER = "|H:";
const std::string VERTICAL_LINE = "|";
static char g_pid[6];

class HitraceMeterFfiTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void HitraceMeterFfiTest::SetUpTestCase(void)
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

void HitraceMeterFfiTest::TearDownTestCase(void)
{
    SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0");
    SetFtrace(TRACING_ON_NODE, false);
    CleanTrace();
}

void HitraceMeterFfiTest::SetUp(void)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON_NODE, true)) << "SetUp: Setting tracing_on failed.";
    std::string value = std::to_string(TAG);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value)) << "SetUp: Setting enableflags failed.";
    HILOG_INFO(LOG_CORE, "current tag is %{public}s", GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0").c_str());
    ASSERT_TRUE(GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "-123") == value);
    UpdateTraceLabel();
}

void HitraceMeterFfiTest::TearDown(void)
{
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0")) << "TearDown: Setting enableflags failed.";
    ASSERT_TRUE(SetFtrace(TRACING_ON_NODE, false)) << "TearDown: Setting tracing_on failed.";
    ASSERT_TRUE(CleanTrace()) << "TearDown: Cleaning trace failed.";
}


/**
 * @tc.name: HitraceFfiTest01
 * @tc.desc: Testing FfiOHOSHiTraceStartAsyncTrace and FfiOHOSHiTraceFinishAsyncTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterFfiTest, HitraceFfiTest01, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceFfiTest01: start.";

    const char* name = "HitraceFfiTest01";
    int32_t taskId = 3;
    
    FfiOHOSHiTraceStartAsyncTrace(name, taskId);
    FfiOHOSHiTraceFinishAsyncTrace(name, taskId);

    std::vector<std::string> list = ReadTrace();
    
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceFfiTest01: end.";
}


/**
 * @tc.name: HitraceFfiTest02
 * @tc.desc: Testing FfiOHOSHiTraceStartAsyncTrace and FfiOHOSHiTraceFinishAsyncTrace name is nullptr
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterFfiTest, HitraceFfiTest02, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceFfiTest02: start.";

    const char* name = nullptr;
    int32_t taskId = 3;
    
    FfiOHOSHiTraceStartAsyncTrace(name, taskId);
    FfiOHOSHiTraceFinishAsyncTrace(name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceFfiTest02: end.";
}

/**
 * @tc.name: HitraceFfiTest03
 * @tc.desc: Testing MiddleTraceDebug interface FfiOHOSHiTraceCountTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterFfiTest, HitraceFfiTest03, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceFfiTest03: start.";

    const char* name = nullptr;
    int64_t count = 5;

    FfiOHOSHiTraceCountTrace(name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_INFO, TAG, count, name, "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceFfiTest03: end.";
}

/**
 * @tc.name: HitraceFfiTest04
 * @tc.desc: Testing MiddleTraceDebug interface FfiOHOSHiTraceChainBegin
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterFfiTest, HitraceFfiTest04, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceFfiTest04: start.";

    const char* name = nullptr;

    CHiTraceId hiTraceId= FfiOHOSHiTraceChainBegin(name, HiTraceFlag::HITRACE_FLAG_DEFAULT);
    
    OHOS::HiviewDFX::HiTraceId id = Parse(hiTraceId);
    
    CHiTraceId traceId = Parse(id);
    
    ASSERT_EQ(hiTraceId.chainId, traceId.chainId);
    ASSERT_EQ(hiTraceId.spanId, traceId.spanId);
    ASSERT_EQ(hiTraceId.parentSpanId, traceId.parentSpanId);
    FfiOHOSHiTraceChainEnd(hiTraceId);


    GTEST_LOG_(INFO) << "HitraceFfiTest04: end.";
}

/**
 * @tc.name: HitraceFfiTest05
 * @tc.desc: Testing FfiOHOSHiTraceStartAsyncTrace and FfiOHOSHiTraceFinishAsyncTrace TraceInfo LEVEL
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterFfiTest, HitraceFfiTest05, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceFfiTest05: start.";

    const char* name = "HitraceFfiTest05";
    int32_t taskId = 3;
    
    FfiOHOSHiTraceStartAsyncTrace(name, taskId);
    FfiOHOSHiTraceFinishAsyncTrace(name, taskId);

    std::vector<std::string> list = ReadTrace();
    
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, name, "", ""};
    bool isCommercialStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isCommercialStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isCommercialFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isCommercialFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    CHiTraceId hiTraceId= FfiOHOSHiTraceChainBegin(name, HiTraceFlag::HITRACE_FLAG_DEFAULT);
    FfiOHOSHiTraceStartAsyncTrace(name, taskId);
    FfiOHOSHiTraceFinishAsyncTrace(name, taskId);
    OHOS::HiviewDFX::HiTraceId id = Parse(hiTraceId);
    FfiOHOSHiTraceChainEnd(hiTraceId);

    list = ReadTrace();
    traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name, "", ""};
    traceInfo.hiTraceId = &id;
    bool isInfoStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isInfoStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isInfoFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isInfoFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceFfiTest05: end.";
}

}
}
}
