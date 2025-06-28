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

#include <gtest/gtest.h>

#include "trace.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
class HitraceChainNDKTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}

    void SetUp()
    {
        OH_HiTrace_ClearId();
    }
    void TearDown() {}
};

/**
 * @tc.name: HitraceChainNDKTest001
 * @tc.desc: test normal chain begin and end
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest001: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest001", HITRACE_FLAG_DEFAULT);
    ASSERT_TRUE(OH_HiTrace_IsIdValid(&hiTraceId));
    OH_HiTrace_EndChain();
    hiTraceId = OH_HiTrace_GetId();
    ASSERT_FALSE(OH_HiTrace_IsIdValid(&hiTraceId));

    GTEST_LOG_(INFO) << "HitraceChainNDKTest001: end.";
}

/**
 * @tc.name: HitraceChainNDKTest002
 * @tc.desc: test the hitrace flag enabled
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest002: start.";

    int flag = HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_NO_BE_INFO;
    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest002", flag);
    ASSERT_TRUE(OH_HiTrace_IsFlagEnabled(&hiTraceId, HITRACE_FLAG_INCLUDE_ASYNC));
    ASSERT_TRUE(OH_HiTrace_IsFlagEnabled(&hiTraceId, HITRACE_FLAG_NO_BE_INFO));
    ASSERT_FALSE(OH_HiTrace_IsFlagEnabled(&hiTraceId, HITRACE_FLAG_DONOT_CREATE_SPAN));

    OH_HiTrace_EnableFlag(&hiTraceId, HITRACE_FLAG_DONOT_CREATE_SPAN);
    ASSERT_TRUE(OH_HiTrace_IsFlagEnabled(&hiTraceId, HITRACE_FLAG_INCLUDE_ASYNC));
    ASSERT_TRUE(OH_HiTrace_IsFlagEnabled(&hiTraceId, HITRACE_FLAG_NO_BE_INFO));
    ASSERT_TRUE(OH_HiTrace_IsFlagEnabled(&hiTraceId, HITRACE_FLAG_DONOT_CREATE_SPAN));
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest002: end.";
}

/**
 * @tc.name: HitraceChainNDKTest003
 * @tc.desc: Test the HiTraceId initialization
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest003: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest001", HITRACE_FLAG_DEFAULT);
    ASSERT_TRUE(OH_HiTrace_IsIdValid(&hiTraceId));
    OH_HiTrace_InitId(&hiTraceId);
    ASSERT_FALSE(OH_HiTrace_IsIdValid(&hiTraceId));
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest003: end.";
}

/**
 * @tc.name: HitraceChainNDKTest004
 * @tc.desc: test the creation of the span ID
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest004: start.";

    HiTraceId hiTraceId1 = OH_HiTrace_BeginChain("HitraceChainNDKTest004", HITRACE_FLAG_DEFAULT);
    ASSERT_EQ(OH_HiTrace_GetSpanId(&hiTraceId1), 0);
    ASSERT_EQ(OH_HiTrace_GetParentSpanId(&hiTraceId1), 0);

    HiTraceId hiTraceId2 = OH_HiTrace_CreateSpan();
    OH_HiTrace_SetId(&hiTraceId2);
    ASSERT_NE(OH_HiTrace_GetSpanId(&hiTraceId2), 0);
    ASSERT_EQ(OH_HiTrace_GetParentSpanId(&hiTraceId2), 0);

    HiTraceId hiTraceId3 = OH_HiTrace_CreateSpan();
    OH_HiTrace_SetId(&hiTraceId3);
    ASSERT_NE(OH_HiTrace_GetSpanId(&hiTraceId3), OH_HiTrace_GetSpanId(&hiTraceId2));
    ASSERT_EQ(OH_HiTrace_GetParentSpanId(&hiTraceId3), OH_HiTrace_GetSpanId(&hiTraceId2));
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest004: end.";
}

/**
 * @tc.name: HitraceChainNDKTest005
 * @tc.desc: test setting the trace flag
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest005: start.";

    int flag = HITRACE_FLAG_TP_INFO | HITRACE_FLAG_DONOT_ENABLE_LOG;
    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest005", flag);
    ASSERT_EQ(OH_HiTrace_GetFlags(&hiTraceId), flag);

    flag = HITRACE_FLAG_FAULT_TRIGGER | HITRACE_FLAG_D2D_TP_INFO;
    OH_HiTrace_SetFlags(&hiTraceId, flag);
    ASSERT_EQ(OH_HiTrace_GetFlags(&hiTraceId), flag);
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest005: end.";
}

/**
 * @tc.name: HitraceChainNDKTest006
 * @tc.desc: test the custom chain ID
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest006: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest006", HITRACE_FLAG_DEFAULT);
    OH_HiTrace_SetChainId(&hiTraceId, 6);
    ASSERT_EQ(OH_HiTrace_GetChainId(&hiTraceId), 6);
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest006: end.";
}

/**
 * @tc.name: HitraceChainNDKTest007
 * @tc.desc: test the custom span ID
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest007, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest007: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest007", HITRACE_FLAG_DEFAULT);
    OH_HiTrace_SetSpanId(&hiTraceId, 7);
    ASSERT_EQ(OH_HiTrace_GetSpanId(&hiTraceId), 7);
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest007: end.";
}

/**
 * @tc.name: HitraceChainNDKTest008
 * @tc.desc: test the custom parent span ID
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest008: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest008", HITRACE_FLAG_DEFAULT);
    OH_HiTrace_SetParentSpanId(&hiTraceId, 8);
    ASSERT_EQ(OH_HiTrace_GetParentSpanId(&hiTraceId), 8);
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest008: end.";
}

/**
 * @tc.name: HitraceChainNDKTest009
 * @tc.desc: test the conversion between HiTraceId and bytes
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest009, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest009: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest009", HITRACE_FLAG_DEFAULT);
    ASSERT_TRUE(OH_HiTrace_IsIdValid(&hiTraceId));
    uint8_t byteArray[sizeof(HiTraceId)];
    int len = OH_HiTrace_IdToBytes(&hiTraceId, byteArray, sizeof(byteArray));

    HiTraceId hiTraceIdFromBytes;
    OH_HiTrace_InitId(&hiTraceIdFromBytes);
    ASSERT_FALSE(OH_HiTrace_IsIdValid(&hiTraceIdFromBytes));
    OH_HiTrace_IdFromBytes(&hiTraceIdFromBytes, byteArray, len);
    ASSERT_TRUE(OH_HiTrace_IsIdValid(&hiTraceIdFromBytes));
    ASSERT_EQ(OH_HiTrace_GetChainId(&hiTraceIdFromBytes), OH_HiTrace_GetChainId(&hiTraceId));
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest009: end.";
}

/**
 * @tc.name: HitraceChainNDKTest010
 * @tc.desc: test the OH_HiTrace_Tracepoint interface in different tracepoint types
 * @tc.type: FUNC
 */
HWTEST_F(HitraceChainNDKTest, HitraceChainNDKTest010, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceChainNDKTest010: start.";

    HiTraceId hiTraceId = OH_HiTrace_BeginChain("HitraceChainNDKTest010", HITRACE_FLAG_DEFAULT);
    ASSERT_TRUE(OH_HiTrace_IsIdValid(&hiTraceId));
    OH_HiTrace_Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &hiTraceId, "HitraceChainNDKTest010-%d", 10);
    OH_HiTrace_Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_SR, &hiTraceId, "HitraceChainNDKTest010-%d", 10);
    OH_HiTrace_Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_SS, &hiTraceId, "HitraceChainNDKTest010-%d", 10);
    OH_HiTrace_Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_CR, &hiTraceId, "HitraceChainNDKTest010-%d", 10);
    OH_HiTrace_EndChain();

    GTEST_LOG_(INFO) << "HitraceChainNDKTest010: end.";
}
}
}
}