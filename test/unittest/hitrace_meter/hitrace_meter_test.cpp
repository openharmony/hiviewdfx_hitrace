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

#include <gtest/gtest.h>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_meter.h"
#include "hitrace_meter_test_utils.h"
#include "hitrace/tracechain.h"

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

constexpr uint64_t TAG = HITRACE_TAG_OHOS;
constexpr uint64_t INVALID_TAG = HITRACE_TAG_BLUETOOTH;
const std::string LABEL_HEADER = "|H:";
const std::string VERTICAL_LINE = "|";
static char g_pid[6];

class HitraceMeterTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void HitraceMeterTest::SetUpTestCase(void)
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

void HitraceMeterTest::TearDownTestCase(void)
{
    SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0");
    SetFtrace(TRACING_ON_NODE, false);
    CleanTrace();
}

void HitraceMeterTest::SetUp(void)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON_NODE, true)) << "SetUp: Setting tracing_on failed.";
    std::string value = std::to_string(TAG);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value)) << "SetUp: Setting enableflags failed.";
    HILOG_INFO(LOG_CORE, "current tag is %{public}s", GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0").c_str());
    ASSERT_TRUE(GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "-123") == value);
    UpdateTraceLabel();
}

void HitraceMeterTest::TearDown(void)
{
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0")) << "TearDown: Setting enableflags failed.";
    ASSERT_TRUE(SetFtrace(TRACING_ON_NODE, false)) << "TearDown: Setting tracing_on failed.";
    ASSERT_TRUE(CleanTrace()) << "TearDown: Cleaning trace failed.";
}

/**
 * @tc.name: SyncTraceInterfaceTest001
 * @tc.desc: Testing the trace output when the system trace level threshold is DEBUG
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest001: start.";

    const char* name = "SyncTraceInterfaceTest001";
    const char* customArgs = "key=value";
    std::string defaultLevel = GetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_INFO));
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_DEBUG)));

    StartTraceEx(HITRACE_LEVEL_DEBUG, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_DEBUG, TAG);
    StartTraceEx(HITRACE_LEVEL_INFO, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_INFO, TAG);
    StartTraceEx(HITRACE_LEVEL_CRITICAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_CRITICAL, TAG);
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_DEBUG, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_INFO;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_CRITICAL;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_COMMERCIAL;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    traceInfo.type = 'E';
    traceInfo.level = HITRACE_LEVEL_DEBUG;
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_INFO;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_CRITICAL;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_COMMERCIAL;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, defaultLevel));
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest001: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest002
 * @tc.desc: Testing the trace output when the system trace level threshold is INFO
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest002: start.";

    const char* name = "SyncTraceInterfaceTest002";
    const char* customArgs = "key=value";
    std::string defaultLevel = GetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_INFO));
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_INFO)));

    StartTraceEx(HITRACE_LEVEL_DEBUG, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_DEBUG, TAG);
    StartTraceEx(HITRACE_LEVEL_INFO, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_INFO, TAG);
    StartTraceEx(HITRACE_LEVEL_CRITICAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_CRITICAL, TAG);
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_DEBUG, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_INFO;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_CRITICAL;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_COMMERCIAL;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    traceInfo.type = 'E';
    traceInfo.level = HITRACE_LEVEL_DEBUG;
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_INFO;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_CRITICAL;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_COMMERCIAL;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, defaultLevel));
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest002: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest003
 * @tc.desc: Testing the trace output when the system trace level threshold is COMMERCIAL
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest003: start.";

    const char* name = "SyncTraceInterfaceTest003";
    const char* customArgs = "key=value";
    std::string defaultLevel = GetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_INFO));
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_COMMERCIAL)));

    StartTraceEx(HITRACE_LEVEL_DEBUG, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_DEBUG, TAG);
    StartTraceEx(HITRACE_LEVEL_INFO, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_INFO, TAG);
    StartTraceEx(HITRACE_LEVEL_CRITICAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_CRITICAL, TAG);
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_DEBUG, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_INFO;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_CRITICAL;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_COMMERCIAL;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    traceInfo.type = 'E';
    traceInfo.level = HITRACE_LEVEL_DEBUG;
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_INFO;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_CRITICAL;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.level = HITRACE_LEVEL_COMMERCIAL;
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, defaultLevel));
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest003: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest004
 * @tc.desc: Testing the trace output with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest004: start.";

    const char* name = "SyncTraceInterfaceTest004";
    const char* customArgs = "key=value";
    const uint64_t multipleTags = HITRACE_TAG_APP | HITRACE_TAG_ARK | HITRACE_TAG_RPC | HITRACE_TAG_FFRT;
    std::string value = std::to_string(multipleTags);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value));

    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, multipleTags, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, multipleTags);
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, INVALID_TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, INVALID_TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, multipleTags, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    traceInfo.tag = INVALID_TAG;
    traceInfo.type = 'B';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest004: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest005
 * @tc.desc: Testing tag HITRACE_TAG_COMMERCIAL
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest005: start.";

    const char* name = "SyncTraceInterfaceTest005";
    const char* customArgs = "key=value";
    const uint64_t multipleTags = TAG | HITRACE_TAG_COMMERCIAL;

    StartTraceEx(HITRACE_LEVEL_INFO, multipleTags, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_INFO, multipleTags);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, multipleTags, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest005: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest006
 * @tc.desc: Testing parameters are empty strings
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest006: start.";

    const char* name = "SyncTraceInterfaceTest006";
    const char* customArgs = "key=value";

    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, "", customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfoEmptyName = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, "", "", customArgs};
    bool isStartSucEmptyName = GetTraceResult(traceInfoEmptyName, list, record);
    ASSERT_TRUE(isStartSucEmptyName) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfoEmptyName.type = 'E';
    bool isFinishSucEmptyName = GetTraceResult(traceInfoEmptyName, list, record);
    ASSERT_TRUE(isFinishSucEmptyName) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, "");
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    list = ReadTrace();
    TraceInfo traceInfoEmptyArgs = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", ""};
    bool isStartSucEmptyArgs = GetTraceResult(traceInfoEmptyArgs, list, record);
    ASSERT_TRUE(isStartSucEmptyArgs) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfoEmptyArgs.type = 'E';
    bool isFinishSucEmptyArgs = GetTraceResult(traceInfoEmptyArgs, list, record);
    ASSERT_TRUE(isFinishSucEmptyArgs) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, nullptr, nullptr);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    list = ReadTrace();
    TraceInfo traceInfoEmptyAll = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, nullptr, nullptr, nullptr};
    bool isStartSucEmptyAll = GetTraceResult(traceInfoEmptyAll, list, record);
    GTEST_LOG_(INFO) << record;
    ASSERT_TRUE(isStartSucEmptyAll) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfoEmptyAll.type = 'E';
    bool isFinishSucEmptyAll = GetTraceResult(traceInfoEmptyAll, list, record);
    ASSERT_TRUE(isFinishSucEmptyAll) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest006: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest007
 * @tc.desc: Testing parameters are overlength strings
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest007, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest007: start.";

    // Each line contains 75 characters, and the longName has a total length of 325 characters.
    std::string longName = "SyncTraceInterfaceTest007SyncTraceInterfaceTest007SyncTraceInterfaceTest007";
    longName += "SyncTraceInterfaceTest007SyncTraceInterfaceTest007SyncTraceInterfaceTest007";
    longName += "SyncTraceInterfaceTest007SyncTraceInterfaceTest007SyncTraceInterfaceTest007";
    longName += "SyncTraceInterfaceTest007SyncTraceInterfaceTest007SyncTraceInterfaceTest007";
    longName += "SyncTraceInterfaceTest007";
    // Each line contains 50 characters, and the longCustomArgs has a total length of 199 characters.
    std::string longCustomArgs = "key=value,key=value,key=value,key=value,key=value,";
    longCustomArgs += "key=value,key=value,key=value,key=value,key=value,";
    longCustomArgs += "key=value,key=value,key=value,key=value,key=value,";
    longCustomArgs += "key=value,key=value,key=value,key=value,key=value";

    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, longName.c_str(), longCustomArgs.c_str());
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    // Parameter name is limited to 320 characters in the trace output.
    // The total length of the trace output is limited to 512 characters.
    std::string realName = longName.substr(0, 320);
    std::string expectRecord = std::string("B|") + g_pid + LABEL_HEADER + realName + std::string("|M30|");
    int remainLength = RECORD_SIZE_MAX - expectRecord.length();
    std::string realCustomArgs = longCustomArgs.substr(0, remainLength);
    expectRecord += realCustomArgs;

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, longName.c_str(), "", longCustomArgs.c_str()};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    int ret = strcmp(expectRecord.c_str(), record);
    ASSERT_EQ(ret, 0) << "\nexpectRecord=\"" << expectRecord << "\"\nrecord=\"" << record <<"\"";
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest007: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest008
 * @tc.desc: Testing StartTrace and FinishTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest008: start.";

    std::string name = "SyncTraceInterfaceTest008";

    StartTrace(TAG, name);
    FinishTrace(TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, name.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest008: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest009
 * @tc.desc: Testing StartTraceDebug and FinishTraceDebug
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest009, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest009: start.";

    std::string name = "SyncTraceInterfaceTest009";

    StartTraceDebug(true, TAG, name);
    FinishTraceDebug(true, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, name.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceDebug(false, TAG, name);
    FinishTraceDebug(false, TAG);

    list = ReadTrace();
    traceInfo.type = 'B';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest009: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest010
 * @tc.desc: Testing StartTraceArgs
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest010, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest010: start.";

    const char* name = "SyncTraceInterfaceTest010-%d";
    int var = 10;
    // Each line contains 75 characters, and the longName has a total length of 453 characters.
    std::string longName = "SyncTraceInterfaceTest010SyncTraceInterfaceTest010SyncTraceInterfaceTest010";
    longName += "SyncTraceInterfaceTest010SyncTraceInterfaceTest010SyncTraceInterfaceTest010";
    longName += "SyncTraceInterfaceTest010SyncTraceInterfaceTest010SyncTraceInterfaceTest010";
    longName += "SyncTraceInterfaceTest010SyncTraceInterfaceTest010SyncTraceInterfaceTest010";
    longName += "SyncTraceInterfaceTest010SyncTraceInterfaceTest010SyncTraceInterfaceTest010";
    longName += "SyncTraceInterfaceTest010SyncTraceInterfaceTest010SyncTraceInterfaceTest010-%d";

    SetTraceDisabled(true);
    StartTraceArgs(TAG, name, var);
    FinishTrace(TAG);
    SetTraceDisabled(false);

    std::vector<std::string> list = ReadTrace();
    bool isSuccess = FindResult("SyncTraceInterfaceTest010", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest010\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgs(TAG, longName.c_str(), var);
    FinishTrace(TAG);

    list = ReadTrace();
    isSuccess = FindResult("SyncTraceInterfaceTest010", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest010\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgs(INVALID_TAG, name, var);
    FinishTrace(INVALID_TAG);

    list = ReadTrace();
    isSuccess = FindResult("SyncTraceInterfaceTest010", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest010\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgs(TAG, name, var);
    FinishTrace(TAG);

    list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, "SyncTraceInterfaceTest010-10", "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest010: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest011
 * @tc.desc: Testing StartTraceArgsDebug and FinishTraceDebug
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest011, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest011: start.";

    const char* name = "SyncTraceInterfaceTest011-%d";
    int var = 11;
    // Each line contains 75 characters, and the longName has a total length of 453 characters.
    std::string longName = "SyncTraceInterfaceTest011SyncTraceInterfaceTest011SyncTraceInterfaceTest011";
    longName += "SyncTraceInterfaceTest011SyncTraceInterfaceTest011SyncTraceInterfaceTest011";
    longName += "SyncTraceInterfaceTest011SyncTraceInterfaceTest011SyncTraceInterfaceTest011";
    longName += "SyncTraceInterfaceTest011SyncTraceInterfaceTest011SyncTraceInterfaceTest011";
    longName += "SyncTraceInterfaceTest011SyncTraceInterfaceTest011SyncTraceInterfaceTest011";
    longName += "SyncTraceInterfaceTest011SyncTraceInterfaceTest011SyncTraceInterfaceTest011-%d";

    SetTraceDisabled(true);
    StartTraceArgsDebug(true, TAG, name, var);
    FinishTraceDebug(true, TAG);
    SetTraceDisabled(false);

    std::vector<std::string> list = ReadTrace();
    list = ReadTrace();
    bool isSuccess = FindResult("SyncTraceInterfaceTest011", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest011\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgsDebug(false, TAG, name, var);
    FinishTraceDebug(false, TAG);

    list = ReadTrace();
    isSuccess = FindResult("SyncTraceInterfaceTest011", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest011\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgsDebug(true, TAG, longName.c_str(), var);
    FinishTraceDebug(true, TAG);

    list = ReadTrace();
    isSuccess = FindResult("SyncTraceInterfaceTest011", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest011\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgsDebug(true, INVALID_TAG, name, var);
    FinishTraceDebug(true, INVALID_TAG);

    list = ReadTrace();
    isSuccess = FindResult("SyncTraceInterfaceTest011", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"SyncTraceInterfaceTest011\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceArgsDebug(true, TAG, name, var);
    FinishTraceDebug(true, TAG);

    list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, "SyncTraceInterfaceTest011-11", "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest011: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest012
 * @tc.desc: Testing StartTraceWrapper
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest012, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest012: start.";

    const char* name = "SyncTraceInterfaceTest012";

    StartTraceWrapper(TAG, name);
    FinishTrace(TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest012: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest013
 * @tc.desc: Testing innerkits_c interfaces: HiTraceStartTraceEx and HiTraceFinishTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest013, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest013: start.";

    const char* name = "SyncTraceInterfaceTest013";
    const char* customArgs = "key=value";

    HiTraceStartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    HiTraceFinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest013: end.";
}

/**
 * @tc.name: SyncTraceInterfaceTest014
 * @tc.desc: Testing innerkits_c interfaces: HiTraceStartTrace and HiTraceFinishTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, SyncTraceInterfaceTest014, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest014: start.";

    const char* name = "SyncTraceInterfaceTest014";

    HiTraceStartTrace(TAG, name);
    HiTraceFinishTrace(TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "SyncTraceInterfaceTest014: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest001
 * @tc.desc: Testing StartAsyncTraceEx and FinishAsyncTraceEx with HiTraceId
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest001: start.";

    const char* name = "AsyncTraceInterfaceTest001";
    const char* customCategory = nullptr;
    const char* customArgs = nullptr;
    int32_t taskId = 1;

    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, customCategory, customArgs);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, name, customCategory, customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    HiTraceId hiTraceId = HiTraceChain::Begin(name, HiTraceFlag::HITRACE_FLAG_DEFAULT);
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, customCategory, customArgs);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);
    HiTraceChain::End(hiTraceId);

    list = ReadTrace();
    traceInfo.type = 'S';
    traceInfo.hiTraceId = &hiTraceId;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest001: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest002
 * @tc.desc: Testing parameters are overlength strings
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest002: start.";

    // Each line contains 78 characters, and the longName has a total length of 338 characters.
    std::string longName = "AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002";
    longName += "AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002";
    longName += "AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002";
    longName += "AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002AsyncTraceInterfaceTest002";
    longName += "AsyncTraceInterfaceTest002";
    // The longCustomCategory has a total length of 72 characters.
    std::string longCustomCategory = "CategoryTestCategoryTestCategoryTestCategoryTestCategoryTestCategoryTest";
    // Each line contains 50 characters, and the longCustomArgs has a total length of 199 characters.
    std::string longCustomArgs = "key=value,key=value,key=value,key=value,key=value,";
    longCustomArgs += "key=value,key=value,key=value,key=value,key=value,";
    longCustomArgs += "key=value,key=value,key=value,key=value,key=value,";
    longCustomArgs += "key=value,key=value,key=value,key=value,key=value";
    int32_t taskId = 2;

    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, longName.c_str(), taskId,
                      longCustomCategory.c_str(), longCustomArgs.c_str());
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, longName.c_str(), taskId);

    // Parameter name is limited to 320 characters in the trace output.
    // Parameter customCategory is limited to 64 characters in the trace output.
    // The total length of the trace output is limited to 512 characters.
    std::string realName = longName.substr(0, 320);
    std::string realCustomCategory = longCustomCategory.substr(0, 64);
    std::string expectRecord = std::string("S|") + g_pid + LABEL_HEADER + realName + VERTICAL_LINE +
                               std::to_string(taskId) + std::string("|M30|") + realCustomCategory + VERTICAL_LINE;
    int remainLength = RECORD_SIZE_MAX - expectRecord.length();
    std::string realCustomArgs = longCustomArgs.substr(0, remainLength);
    expectRecord += realCustomArgs;

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {
        'S', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, longName.c_str(), longCustomCategory.c_str(), longCustomArgs.c_str()
    };
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    int ret = strcmp(expectRecord.c_str(), record);
    ASSERT_EQ(ret, 0) << "\nexpectRecord=\"" << expectRecord << "\"\nrecord=\"" << record <<"\"";
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest002: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest003
 * @tc.desc: Testing StartAsyncTrace and FinishAsyncTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest003: start.";

    std::string name = "AsyncTraceInterfaceTest003";
    int32_t taskId = 3;

    StartAsyncTrace(TAG, name, taskId);
    FinishAsyncTrace(TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest003: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest004
 * @tc.desc: Testing StartAsyncTraceDebug and FinishAsyncTraceDebug
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest004: start.";

    std::string name = "AsyncTraceInterfaceTest004";
    int32_t taskId = 4;

    StartAsyncTraceDebug(true, TAG, name, taskId);
    FinishAsyncTraceDebug(true, TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceDebug(false, TAG, name, taskId);
    FinishAsyncTraceDebug(false, TAG, name, taskId);

    list = ReadTrace();
    traceInfo.type = 'S';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest004: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest005
 * @tc.desc: Testing StartAsyncTraceArgs and FinishAsyncTraceArgs
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest005: start.";

    const char* name = "AsyncTraceInterfaceTest005-%d";
    int var = 5;
    // Each line contains 78 characters, and the longName has a total length of 470 characters.
    std::string longName = "AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005";
    longName += "AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005";
    longName += "AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005";
    longName += "AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005";
    longName += "AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005";
    longName += "AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005AsyncTraceInterfaceTest005-%d";
    int32_t taskId = 5;

    SetTraceDisabled(true);
    StartAsyncTraceArgs(TAG, taskId, name, var);
    FinishAsyncTraceArgs(TAG, taskId, name, var);
    SetTraceDisabled(false);

    std::vector<std::string> list = ReadTrace();
    bool isSuccess = FindResult("AsyncTraceInterfaceTest005", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest005\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgs(TAG, taskId, longName.c_str(), var);
    FinishAsyncTraceArgs(TAG, taskId, longName.c_str(), var);

    list = ReadTrace();
    isSuccess = FindResult("AsyncTraceInterfaceTest005", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest005\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgs(INVALID_TAG, taskId, name, var);
    FinishAsyncTraceArgs(INVALID_TAG, taskId, name, var);

    list = ReadTrace();
    isSuccess = FindResult("AsyncTraceInterfaceTest005", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest005\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgs(TAG, taskId, name, var);
    FinishAsyncTraceArgs(TAG, taskId, name, var);

    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, "AsyncTraceInterfaceTest005-5", "", ""};
    list = ReadTrace();
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest005: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest006
 * @tc.desc: Testing StartAsyncTraceArgsDebug and FinishAsyncTraceArgsDebug
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest006: start.";

    const char* name = "AsyncTraceInterfaceTest006-%d";
    int var = 6;
    // Each line contains 78 characters, and the longName has a total length of 470 characters.
    std::string longName = "AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006";
    longName += "AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006";
    longName += "AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006";
    longName += "AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006";
    longName += "AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006";
    longName += "AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006AsyncTraceInterfaceTest006-%d";
    int32_t taskId = 6;

    SetTraceDisabled(true);
    StartAsyncTraceArgsDebug(true, TAG, taskId, name, var);
    FinishAsyncTraceArgsDebug(true, TAG, taskId, name, var);
    SetTraceDisabled(false);

    std::vector<std::string> list = ReadTrace();
    bool isSuccess = FindResult("AsyncTraceInterfaceTest006", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest006\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgsDebug(false, TAG, taskId, name, var);
    FinishAsyncTraceArgsDebug(false, TAG, taskId, name, var);

    list = ReadTrace();
    isSuccess = FindResult("AsyncTraceInterfaceTest006", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest006\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgsDebug(true, TAG, taskId, longName.c_str(), var);
    FinishAsyncTraceArgsDebug(true, TAG, taskId, longName.c_str(), var);

    list = ReadTrace();
    isSuccess = FindResult("AsyncTraceInterfaceTest006", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest006\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgsDebug(true, INVALID_TAG, taskId, name, var);
    FinishAsyncTraceArgsDebug(true, INVALID_TAG, taskId, name, var);

    list = ReadTrace();
    isSuccess = FindResult("AsyncTraceInterfaceTest006", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"AsyncTraceInterfaceTest006\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceArgsDebug(true, TAG, taskId, name, var);
    FinishAsyncTraceArgsDebug(true, TAG, taskId, name, var);

    list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, "AsyncTraceInterfaceTest006-6", "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest006: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest007
 * @tc.desc: Testing StartAsyncTraceWrapper and FinishAsyncTraceWrapper
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest007, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest007: start.";

    const char* name = "AsyncTraceInterfaceTest007";
    int32_t taskId = 7;

    StartAsyncTraceWrapper(TAG, name, taskId);
    FinishAsyncTraceWrapper(TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest007: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest008
 * @tc.desc: Testing innerkits_c interfaces: HiTraceStartAsyncTraceEx and HiTraceFinishAsyncTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest008: start.";

    const char* name = "AsyncTraceInterfaceTest008";
    const char* customCategory = "test";
    const char* customArgs = "key=value";
    int32_t taskId = 8;

    HiTraceStartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, customCategory, customArgs);
    HiTraceFinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, name, customCategory, customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest008: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest009
 * @tc.desc: Testing innerkits_c interfaces: HiTraceStartAsyncTrace and HiTraceFinishAsyncTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest009, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest009: start.";

    const char* name = "AsyncTraceInterfaceTest009";
    int32_t taskId = 9;

    HiTraceStartAsyncTrace(TAG, name, taskId);
    HiTraceFinishAsyncTrace(TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_INFO, TAG, taskId, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest009: end.";
}

/**
 * @tc.name: AsyncTraceInterfaceTest010
 * @tc.desc: Testing innerkits_c interfaces: HiTraceStartAsyncTrace and HiTraceFinishAsyncTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, AsyncTraceInterfaceTest010, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest010: start.";

    const char* name = "AsyncTraceInterfaceTest010";
    const char* customCategory = "test";
    const char* customArgs = "key=value";
    int32_t taskId = 10;

    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, nullptr, nullptr);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'S', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, name, nullptr, nullptr};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, customCategory, "");
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    traceInfo.customCategory = customCategory;
    traceInfo.customArgs = "";
    list = ReadTrace();
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, customCategory, customArgs);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    traceInfo.customArgs = customArgs;
    list = ReadTrace();
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, "", customArgs);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    traceInfo.customCategory = "";
    list = ReadTrace();
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "AsyncTraceInterfaceTest010: end.";
}

/**
 * @tc.name: CountTraceInterfaceTest001
 * @tc.desc: Testing CountTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CountTraceInterfaceTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CountTraceInterfaceTest001: start.";

    const char* name = "CountTraceInterfaceTest001";
    int64_t count = 1;

    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_COMMERCIAL, TAG, count, name, "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CountTraceInterfaceTest001: end.";
}

/**
 * @tc.name: CountTraceInterfaceTest002
 * @tc.desc: Testing CountTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CountTraceInterfaceTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CountTraceInterfaceTest002: start.";

    std::string name = "CountTraceInterfaceTest002";
    int64_t count = 2;

    CountTrace(TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_INFO, TAG, count, name.c_str(), "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CountTraceInterfaceTest002: end.";
}

/**
 * @tc.name: CountTraceInterfaceTest003
 * @tc.desc: Testing CountTraceDebug
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CountTraceInterfaceTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CountTraceInterfaceTest003: start.";

    std::string name = "CountTraceInterfaceTest003";
    int64_t count = 3;

    CountTraceDebug(true, TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_INFO, TAG, count, name.c_str(), "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    CountTraceDebug(false, TAG, name, count);

    list = ReadTrace();
    isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CountTraceInterfaceTest003: end.";
}

/**
 * @tc.name: CountTraceInterfaceTest004
 * @tc.desc: Testing CountTraceWrapper
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CountTraceInterfaceTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CountTraceInterfaceTest004: start.";

    const char* name = "CountTraceInterfaceTest004";
    int64_t count = 4;

    CountTraceWrapper(TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_INFO, TAG, count, name, "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CountTraceInterfaceTest004: end.";
}

/**
 * @tc.name: CountTraceInterfaceTest005
 * @tc.desc: Testing HiTraceCountTraceEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CountTraceInterfaceTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CountTraceInterfaceTest005: start.";

    const char* name = "CountTraceInterfaceTest005";
    int64_t count = 5;

    HiTraceCountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_COMMERCIAL, TAG, count, name, "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CountTraceInterfaceTest005: end.";
}

/**
 * @tc.name: CountTraceInterfaceTest006
 * @tc.desc: Testing HiTraceCountTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CountTraceInterfaceTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CountTraceInterfaceTest006: start.";

    const char* name = "CountTraceInterfaceTest006";
    int64_t count = 6;

    HiTraceCountTrace(TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_INFO, TAG, count, name, "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CountTraceInterfaceTest006: end.";
}

/**
 * @tc.name: TagEnabledInterfaceTest001
 * @tc.desc: Testing whether a single tag or multiple tags are enabled by IsTagEnabled
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, TagEnabledInterfaceTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "TagEnabledInterfaceTest001: start.";

    const uint64_t multipleTags = HITRACE_TAG_APP | HITRACE_TAG_ARK | HITRACE_TAG_RPC;
    std::string value = std::to_string(multipleTags);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value));

    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_APP));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_ARK));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_RPC));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_APP | HITRACE_TAG_ARK));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_ARK | HITRACE_TAG_RPC));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_APP | HITRACE_TAG_RPC));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_APP | HITRACE_TAG_ARK | HITRACE_TAG_RPC));
    ASSERT_FALSE(IsTagEnabled(TAG));
    ASSERT_FALSE(IsTagEnabled(INVALID_TAG));
    ASSERT_FALSE(IsTagEnabled(TAG | INVALID_TAG));
    ASSERT_FALSE(IsTagEnabled(HITRACE_TAG_APP | TAG));

    GTEST_LOG_(INFO) << "TagEnabledInterfaceTest001: end.";
}

/**
 * @tc.name: TagEnabledInterfaceTest002
 * @tc.desc: Testing whether a single tag or multiple tags are enabled by HiTraceIsTagEnabled
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, TagEnabledInterfaceTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "TagEnabledInterfaceTest002: start.";

    const uint64_t multipleTags = HITRACE_TAG_APP | HITRACE_TAG_ARK | HITRACE_TAG_RPC;
    std::string value = std::to_string(multipleTags);
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value));

    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_APP));
    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_ARK));
    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_RPC));
    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_APP | HITRACE_TAG_ARK));
    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_ARK | HITRACE_TAG_RPC));
    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_APP | HITRACE_TAG_RPC));
    ASSERT_TRUE(HiTraceIsTagEnabled(HITRACE_TAG_APP | HITRACE_TAG_ARK | HITRACE_TAG_RPC));
    ASSERT_FALSE(HiTraceIsTagEnabled(TAG));
    ASSERT_FALSE(HiTraceIsTagEnabled(INVALID_TAG));
    ASSERT_FALSE(HiTraceIsTagEnabled(TAG | INVALID_TAG));
    ASSERT_FALSE(HiTraceIsTagEnabled(HITRACE_TAG_APP | TAG));

    GTEST_LOG_(INFO) << "TagEnabledInterfaceTest002: end.";
}

/**
 * @tc.name: CaptureAppTraceTest001
 * @tc.desc: Testing invalid args of StartCaptureAppTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest001: start.";

    int fileSize = 100 * 1024 * 1024; // 100M
    std::string fileName = "";

    int ret = StartCaptureAppTrace(TraceFlag(0), TAG, fileSize, fileName);
    ASSERT_EQ(ret, RetType::RET_FAIL_INVALID_ARGS);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, HITRACE_TAG_NOT_READY, fileSize, fileName);
    ASSERT_EQ(ret, RetType::RET_FAIL_INVALID_ARGS);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, HITRACE_TAG_USB, fileSize, fileName);
    ASSERT_EQ(ret, RetType::RET_FAIL_INVALID_ARGS);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, (1ULL << 63), fileName);
    ASSERT_EQ(ret, RetType::RET_FAIL_INVALID_ARGS);

    GTEST_LOG_(INFO) << "CaptureAppTraceTest001: end.";
}

/**
 * @tc.name: CaptureAppTraceTest002
 * @tc.desc: Testing repeatedly calling StartCaptureAppTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest002: start.";

    int fileSize = 100 * 1024 * 1024; // 100M
    std::string fileName = "";

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, fileName);
    ASSERT_EQ(ret, RetType::RET_SUCC);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, fileName);
    ASSERT_EQ(ret, RetType::RET_STARTED);

    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_SUCC);

    GTEST_LOG_(INFO) << "CaptureAppTraceTest002: end.";
}

/**
 * @tc.name: CaptureAppTraceTest003
 * @tc.desc: Testing abnormal filename
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest003: start.";

    int fileSize = 100 * 1024 * 1024; // 100M
    std::string nonExistentPath = "/data/test/test/test.ftrace";
    std::string noAccessFile = "/proc/test.ftrace";

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, nonExistentPath);
    ASSERT_EQ(ret, RetType::RET_FAIL_ENOENT);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, noAccessFile);
    ASSERT_NE(ret, RetType::RET_SUCC);

    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_STOPPED);

    GTEST_LOG_(INFO) << "CaptureAppTraceTest003: end.";
}

/**
 * @tc.name: CaptureAppTraceTest004
 * @tc.desc: Testing normal StartCaptureAppTrace with FLAG_MAIN_THREAD
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest004: start.";

    int fileSize = 600 * 1024 * 1024; // 600MB
    std::string filePath = "/data/test.ftrace";

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);

    const char* name = "CaptureAppTraceTest004";
    const char* customArgs = "key=value";
    const char* customCategory = "test";
    int number = 4;
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, number, customCategory, customArgs);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, number);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, number);

    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_SUCC);

    std::vector<std::string> list = ReadTrace(filePath);
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, number, name, customCategory, customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'S';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'F';
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'C';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CaptureAppTraceTest004: end.";
}

/**
 * @tc.name: CaptureAppTraceTest005
 * @tc.desc: Testing normal StartCaptureAppTrace with FLAG_ALL_THREAD
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest005: start.";

    int fileSize = 600 * 1024 * 1024; // 600MB
    std::string filePath = "/data/test.ftrace";

    int ret = StartCaptureAppTrace(FLAG_ALL_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);

    const char* name = "CaptureAppTraceTest005";
    const char* customArgs = "key=value";
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_SUCC);

    std::vector<std::string> list = ReadTrace(filePath);
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CaptureAppTraceTest005: end.";
}

/**
 * @tc.name: CaptureAppTraceTest006
 * @tc.desc: Testing fileLimitSize in FLAG_MAIN_THREAD and FLAG_ALL_THREAD mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest006: start.";

    int fileSize = 1 * 1024 * 1024; // 1MB
    std::string filePath = "/data/test.ftrace";
    const char* name = "CaptureAppTraceTest006";

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);
    for (int loopCount = 10000, number = 0; loopCount > 0; --loopCount, ++number) {
        CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, number);
    }
    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_STOPPED);

    ret = StartCaptureAppTrace(FLAG_ALL_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);
    for (int loopCount = 10000, number = 0; loopCount > 0; --loopCount, ++number) {
        CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, number);
    }
    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_STOPPED);

    GTEST_LOG_(INFO) << "CaptureAppTraceTest006: end.";
}

/**
 * @tc.name: CaptureAppTraceTest007
 * @tc.desc: Testing WriteAppTraceLong in StartCaptureAppTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest007: start.";

    int nameSize = 32 * 1024; // 32KB
    int fileSize = 600 * 1024 * 1024; // 600MB
    std::string filePath = "/data/test.ftrace";
    std::string name(nameSize, 'K');
    int64_t number = 7;

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name.c_str(), number);
    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_SUCC);

    ret = StartCaptureAppTrace(FLAG_ALL_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name.c_str(), number);
    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_SUCC);

    GTEST_LOG_(INFO) << "CaptureAppTraceTest007: end.";
}

/**
 * @tc.name: CaptureAppTraceTest008
 * @tc.desc: Testing trace with empty string args in StartCaptureAppTrace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, CaptureAppTraceTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CaptureAppTraceTest008: start.";

    int fileSize = 600 * 1024 * 1024; // 600MB
    std::string filePath = "/data/test.ftrace";

    int ret = StartCaptureAppTrace(FLAG_ALL_THREAD, TAG, fileSize, filePath);
    ASSERT_EQ(ret, RetType::RET_SUCC);

    const char* name = "CaptureAppTraceTest008";
    const char* customArgs = "key=value";
    const char* customCategory = "test";
    int taskId = 8;
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, "");
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, "", "");
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, "", customArgs);
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId, customCategory, "");
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, taskId);

    ret = StopCaptureAppTrace();
    ASSERT_EQ(ret, RetType::RET_SUCC);

    std::vector<std::string> list = ReadTrace(filePath);
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, taskId, name, "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    traceInfo.type = 'S';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.customArgs = customArgs;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.customArgs = "";
    traceInfo.customCategory = customCategory;
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "CaptureAppTraceTest008: end.";
}

/**
 * @tc.name: HitraceMeterTest001
 * @tc.desc: Testing CachedHandle is nullptr
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest001: start.";

    SetCachedHandle(nullptr, reinterpret_cast<CachedHandle>(0xf1234567), nullptr);
    SetCachedHandle(reinterpret_cast<CachedHandle>(0xf1234567), nullptr, nullptr);
    SetCachedHandle(reinterpret_cast<CachedHandle>(0xf1234567), reinterpret_cast<CachedHandle>(0xf1234567), nullptr);

    const char* name = "HitraceMeterTest001";
    const char* customArgs = "key=value";
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest001: end.";
}

/**
 * @tc.name: HitraceMeterTest002
 * @tc.desc: Testing cached system parameters changed
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest002: start.";

    std::string defaultTagValue = GetPropertyInner(TRACE_TAG_ENABLE_FLAGS, "0");
    std::string defaultAppPid = GetPropertyInner(TRACE_KEY_APP_PID, "-1");
    std::string defaultTraceLevel = GetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_INFO));

    UpdateTraceLabel();
    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, std::to_string(INVALID_TAG)));
    ASSERT_TRUE(SetPropertyInner(TRACE_KEY_APP_PID, "1"));
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(HITRACE_LEVEL_DEBUG)));
    UpdateTraceLabel();

    const char* name = "HitraceMeterTest002";
    const char* customArgs = "key=value";
    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";

    ASSERT_TRUE(SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, defaultTagValue));
    ASSERT_TRUE(SetPropertyInner(TRACE_KEY_APP_PID, defaultAppPid));
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, defaultTraceLevel));
    UpdateTraceLabel();

    StartTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    FinishTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG);

    list = ReadTrace();
    traceInfo.type = 'B';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest002: end.";
}

/**
 * @tc.name: HitraceMeterTest003
 * @tc.desc: Testing HitraceScoped
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest003: start.";

    std::string name = "HitraceMeterTest003";
    {
        HitraceScoped(TAG, name);
    }

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, name.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest003: end.";
}

/**
 * @tc.name: HitraceMeterTest004
 * @tc.desc: Testing HitraceMeterFmtScoped
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest004: start.";

    const char* name = "HitraceMeterTest004-%d";
    int var = 4;
    // Each line contains 76 characters, and the longName has a total length of 458 characters.
    std::string longName = "HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004";
    longName += "HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004";
    longName += "HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004";
    longName += "HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004";
    longName += "HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004";
    longName += "HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004HitraceMeterTest004-%d";

    SetTraceDisabled(true);
    {
        HitraceMeterFmtScoped(TAG, name, var);
    }
    SetTraceDisabled(false);

    std::vector<std::string> list = ReadTrace();
    bool isSuccess = FindResult("HitraceMeterTest004", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"HitraceMeterTest004\" from trace.";

    ASSERT_TRUE(CleanTrace());
    {
        HitraceMeterFmtScoped(TAG, longName.c_str(), var);
    }

    list = ReadTrace();
    isSuccess = FindResult("HitraceMeterTest004", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"HitraceMeterTest004\" from trace.";

    ASSERT_TRUE(CleanTrace());
    {
        HitraceMeterFmtScoped(INVALID_TAG, name, var);
    }

    list = ReadTrace();
    isSuccess = FindResult("HitraceMeterTest004", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"HitraceMeterTest004\" from trace.";

    ASSERT_TRUE(CleanTrace());
    {
        HitraceMeterFmtScoped(TAG, name, var);
    }

    list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, "HitraceMeterTest004-4", "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest004: end.";
}

/**
 * @tc.name: HitraceMeterTest005
 * @tc.desc: Testing HitracePerfScoped
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest005: start.";

    std::string name = "HitraceMeterTest005";
    {
        HitracePerfScoped hitracePerfScoped(false, TAG, name);
        ASSERT_EQ(hitracePerfScoped.GetInsCount(), 0);
        ASSERT_EQ(hitracePerfScoped.GetCycleCount(), 0);
    }

    std::vector<std::string> list = ReadTrace();
    bool isSuccess = FindResult("HitraceMeterTest005-Ins", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"HitraceMeterTest005-Ins\" from trace.";
    isSuccess = FindResult("HitraceMeterTest005-Cycle", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"HitraceMeterTest005-Cycle\" from trace.";

    ASSERT_TRUE(CleanTrace());
    {
        HitracePerfScoped hitracePerfScoped(true, TAG, name);
        GTEST_LOG_(INFO) << hitracePerfScoped.GetInsCount();
        GTEST_LOG_(INFO) << hitracePerfScoped.GetCycleCount();
    }

    list = ReadTrace();
    isSuccess = FindResult("HitraceMeterTest005-Ins", list);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"HitraceMeterTest005-Ins\" from trace.";
    isSuccess = FindResult("HitraceMeterTest005-Cycle", list);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"HitraceMeterTest005-Cycle\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest005: end.";
}

/**
 * @tc.name: HitraceMeterTest006
 * @tc.desc: Testing HitraceScopedEx
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest006: start.";

    const char* name = "HitraceMeterTest006";
    const char* customArgs = "key=value";
    {
        HitraceScopedEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, customArgs);
    }

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_COMMERCIAL, TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest006: end.";
}

/**
 * @tc.name: HitraceMeterTest007
 * @tc.desc: Testing abnormal HiTraceOutputLevel
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest007: start.";

    const char* name = "HitraceMeterTest007";
    const char* customArgs = "key=value";

    StartTraceEx(static_cast<HiTraceOutputLevel>(-1), TAG, name, customArgs);
    FinishTraceEx(static_cast<HiTraceOutputLevel>(-1), TAG);
    StartTraceEx(static_cast<HiTraceOutputLevel>(4), TAG, name, customArgs);
    FinishTraceEx(static_cast<HiTraceOutputLevel>(4), TAG);

    std::vector<std::string> list = ReadTrace();
    bool isSuccess = FindResult("HitraceMeterTest007", list);
    ASSERT_FALSE(isSuccess) << "Hitrace shouldn't find \"HitraceMeterTest007\" from trace.";

    ASSERT_TRUE(CleanTrace());
    StartTraceEx(static_cast<HiTraceOutputLevel>(3), TAG, name, customArgs);
    FinishTraceEx(static_cast<HiTraceOutputLevel>(3), TAG);

    list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', static_cast<HiTraceOutputLevel>(3), TAG, 0, name, "", customArgs};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest007: end.";
}

/**
 * @tc.name: HitraceMeterTest008
 * @tc.desc: Testing MiddleTrace interface
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest008: start.";

    std::string nameBefore = "HitraceMeterTest008-before";
    std::string nameAfter = "HitraceMeterTest008-after";

    StartTrace(TAG, nameBefore);
    MiddleTrace(TAG, nameBefore, nameAfter);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, nameBefore.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.name = nameAfter.c_str();
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    FinishTrace(TAG);

    GTEST_LOG_(INFO) << "HitraceMeterTest008: end.";
}

/**
 * @tc.name: HitraceMeterTest009
 * @tc.desc: Testing MiddleTraceDebug interface
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest009, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest009: start.";

    std::string nameBefore = "HitraceMeterTest009-before";
    std::string nameAfter = "HitraceMeterTest009-after";

    StartTrace(TAG, nameBefore);
    MiddleTraceDebug(false, TAG, nameBefore, nameAfter);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'B', HITRACE_LEVEL_INFO, TAG, 0, nameBefore.c_str(), "", ""};
    bool isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.name = nameAfter.c_str();
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isStartSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    bool isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_FALSE(isFinishSuc) << "Hitrace shouldn't find \"" << record << "\" from trace.";

    MiddleTraceDebug(true, TAG, nameBefore, nameAfter);

    list = ReadTrace();
    traceInfo.type = 'B';
    isStartSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isStartSuc) << "Hitrace can't find \"" << record << "\" from trace.";
    traceInfo.type = 'E';
    isFinishSuc = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isFinishSuc) << "Hitrace can't find \"" << record << "\" from trace.";

    FinishTrace(TAG);

    GTEST_LOG_(INFO) << "HitraceMeterTest009: end.";
}

/**
 * @tc.name: HitraceMeterTest010
 * @tc.desc: Testing WriteOnceLog function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest010, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest010: start.";

    bool isWrite = true;
    std::string logStr = "HitraceMeterTest010";

    SetWriteOnceLog(LOG_INFO, logStr, isWrite);
    ASSERT_TRUE(isWrite);

    isWrite = false;
    SetWriteOnceLog(LOG_INFO, logStr, isWrite);
    ASSERT_TRUE(isWrite);

    isWrite = false;
    SetWriteOnceLog(LOG_ERROR, logStr, isWrite);
    ASSERT_TRUE(isWrite);

    isWrite = false;
    SetWriteOnceLog(LOG_DEBUG, logStr, isWrite);
    ASSERT_TRUE(isWrite);

    GTEST_LOG_(INFO) << "HitraceMeterTest010: end.";
}

/**
 * @tc.name: HitraceMeterTest011
 * @tc.desc: Testing MiddleTraceDebug interface
 * @tc.type: FUNC
 */
HWTEST_F(HitraceMeterTest, HitraceMeterTest011, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceMeterTest011: start.";

    const char* name = "HitraceMeterTest011";
    int64_t count = 11;

    SetReloadPid(true);
    SetpidHasReload(true);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, count);

    std::vector<std::string> list = ReadTrace();
    char record[RECORD_SIZE_MAX + 1] = {0};
    TraceInfo traceInfo = {'C', HITRACE_LEVEL_COMMERCIAL, TAG, count, name, "", ""};
    bool isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    SetReloadPid(false);
    SetpidHasReload(false);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, count);

    list = ReadTrace();
    isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    SetReloadPid(true);
    SetpidHasReload(false);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, count);

    list = ReadTrace();
    isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    ASSERT_TRUE(CleanTrace());
    SetReloadPid(false);
    SetpidHasReload(true);
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, TAG, name, count);

    list = ReadTrace();
    isSuccess = GetTraceResult(traceInfo, list, record);
    ASSERT_TRUE(isSuccess) << "Hitrace can't find \"" << record << "\" from trace.";

    GTEST_LOG_(INFO) << "HitraceMeterTest011: end.";
}
}
}
}
