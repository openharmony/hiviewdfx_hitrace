/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "hitrace/hitracechainc.h"

#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <sys/time.h>

#include "gtest/gtest-message.h"
#include "gtest/gtest-test-part.h"
#include "gtest/gtest_pred_impl.h"
#include "gtest/hwext/gtest-tag.h"

#define ARRAY_FIRST_INDEX 0
#define ARRAY_SECOND_INDEX 1
#define ARRAY_THIRD_INDEX 2
#define HASH_DATA_LENGTH 3

#define DEVICE_CLIENT_SEND 12
#define PROCESS_CLIENT_SEND 22
#define THREAD_CLIENT_SEND 32
#define DEFAULT_CLIENT_SEND 42

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;

#define HITRACE_DEBUG
#ifndef HITRACE_DEBUG
#define PRINT_ID(p)
#else
#define PRINT_ID(p)                                                                                                \
    printf(#p " valid:%d, ver:%d, chain:0x%llx, flags:%x, span:0x%x, pspan:0x%x.\n", static_cast<int>((p)->valid), \
           static_cast<int>((p)->ver), static_cast<long long>((p)->chainId), static_cast<int>((p)->flags),         \
           static_cast<int>((p)->spanId), static_cast<int>((p)->parentSpanId))
#endif

static void HiTraceChainTracepointExWithArgsWrapper(HiTraceCommunicationMode mode, HiTraceTracepointType type,
    const HiTraceIdStruct* pId, const char* fmt, ...)
{
    va_list vaList;
    va_start(vaList, fmt);
    HiTraceChainTracepointExWithArgs(mode, type, pId, fmt, vaList);
    va_end(vaList);
}

static void HiTraceChainTracepointExWithArgsDomainWrapper(HiTraceCommunicationMode mode, HiTraceTracepointType type,
    const HiTraceIdStruct* pId, unsigned int domain, const char* fmt, ...)
{
    va_list vaList;
    va_start(vaList, fmt);
    HiTraceChainTracepointExWithArgsDomain(mode, type, pId, domain, fmt, vaList);
    va_end(vaList);
}

static void HiTraceChainTracepointWithArgsWrapper(HiTraceTracepointType type, const HiTraceIdStruct* pId,
    const char* fmt, ...)
{
    va_list vaList;
    va_start(vaList, fmt);
    HiTraceChainTracepointWithArgs(type, pId, fmt, vaList);
    va_end(vaList);
}

static uint64_t HashFunc(const void* pData, uint32_t dataLen)
{
    const uint64_t seed = 131;
    if ((!pData) || dataLen == 0) {
        return 0;
    }
    uint64_t hash = 0;
    uint64_t len = dataLen;
    const char* p = static_cast<const char*>(pData);
    for (; len > 0; --len) {
        hash = (hash * seed) + (*p++);
    }
    return hash;
}

static uint64_t GenerateChainId()
{
    const uint64_t randomNum = 269;
    uint64_t hashData[HASH_DATA_LENGTH];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_sec);
    hashData[ARRAY_FIRST_INDEX] = tv.tv_sec;
    hashData[ARRAY_SECOND_INDEX] = tv.tv_usec;
    hashData[ARRAY_THIRD_INDEX] = randomNum;
    uint64_t hash = HashFunc(hashData, HASH_DATA_LENGTH * sizeof(uint64_t));
    return hash;
}

static uint64_t GenerateSpanId()
{
    const uint64_t randomNum = 269;
    uint64_t hashData[HASH_DATA_LENGTH];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_sec);
    hashData[ARRAY_FIRST_INDEX] = randomNum;
    hashData[ARRAY_SECOND_INDEX] = tv.tv_sec;
    hashData[ARRAY_THIRD_INDEX] = tv.tv_usec;
    uint64_t hash = HashFunc(hashData, HASH_DATA_LENGTH * sizeof(uint64_t));
    return hash;
}

static uint64_t GenerateParentSpanId()
{
    const uint64_t randomNum = 269;
    uint64_t hashData[HASH_DATA_LENGTH];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_sec);
    hashData[ARRAY_FIRST_INDEX] = tv.tv_usec;
    hashData[ARRAY_SECOND_INDEX] = randomNum;
    hashData[ARRAY_THIRD_INDEX] = tv.tv_sec;
    uint64_t hash = HashFunc(hashData, HASH_DATA_LENGTH * sizeof(uint64_t));
    return hash;
}

class HiTraceChainCTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void HiTraceChainCTest::SetUpTestCase()
{}

void HiTraceChainCTest::TearDownTestCase()
{}

void HiTraceChainCTest::SetUp()
{
    HiTraceChainClearId();
}

void HiTraceChainCTest::TearDown()
{}

/**
 * @tc.name: Dfx_HiTraceChainCTest_IdTest_001
 * @tc.desc: Get, set and clear trace id
 * @tc.type: FUNC
 * @tc.require: AR000CQVA0
 */
HWTEST_F(HiTraceChainCTest, IdTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get and validate trace id.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. construct trace id with chain id, span id, parent span id
     *     and flags and set it into context, then get and validate it.
     * @tc.expected: step2. trace id is valid with same chain id, span id, parent
     *     span id and flags.
     * @tc.steps: step3. construct invalid trace id and set into context, then get
     *     and validate it.
     * @tc.expected: step3. trace id is the same with step2.
     * @tc.steps: step4. clear trace id, then get and validate it.
     * @tc.expected: step4. trace id is invalid.
     */
    HiTraceIdStruct initId = HiTraceChainGetId();
    EXPECT_EQ(0, HiTraceChainIsValid(&initId));
    PRINT_ID(&initId);

    // set thread id
    constexpr uint64_t chainId = 0xABCDEF;
    constexpr uint64_t spanId = 0x12345;
    constexpr uint64_t parentSpanId = 0x67890;
    constexpr int flags = HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN;
    HiTraceIdStruct setId;
    HiTraceChainInitId(&setId);
    HiTraceChainSetChainId(&setId, chainId);
    HiTraceChainSetFlags(&setId, flags);
    HiTraceChainSetSpanId(&setId, spanId);
    HiTraceChainSetParentSpanId(&setId, parentSpanId);
    PRINT_ID(&setId);

    HiTraceChainSetId(&setId);

    HiTraceIdStruct getId = HiTraceChainGetId();
    EXPECT_EQ(1, HiTraceChainIsValid(&getId));
    EXPECT_EQ(chainId, HiTraceChainGetChainId(&getId));
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, HiTraceChainGetFlags(&getId));
    EXPECT_EQ(spanId, HiTraceChainGetSpanId(&getId));
    EXPECT_EQ(parentSpanId, HiTraceChainGetParentSpanId(&getId));
    PRINT_ID(&getId);

    // set invalid id
    HiTraceIdStruct invalidId;
    HiTraceChainInitId(&invalidId);

    HiTraceChainSetId(&invalidId);

    getId = HiTraceChainGetId();
    EXPECT_EQ(1, HiTraceChainIsValid(&getId));
    EXPECT_EQ(chainId, HiTraceChainGetChainId(&getId));
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, HiTraceChainGetFlags(&getId));
    EXPECT_EQ(spanId, HiTraceChainGetSpanId(&getId));
    EXPECT_EQ(parentSpanId, HiTraceChainGetParentSpanId(&getId));
    PRINT_ID(&getId);

    // clear thread id
    HiTraceChainClearId();

    HiTraceIdStruct clearId = HiTraceChainGetId();
    EXPECT_EQ(0, HiTraceChainIsValid(&clearId));
    PRINT_ID(&clearId);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_IdTest_002
 * @tc.desc: Get, set and clear trace id by trace id pointer
 * @tc.type: FUNC
 * @tc.require: AR000CQVA0
 */
HWTEST_F(HiTraceChainCTest, IdTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get and validate trace id.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. construct trace id with chain id, span id, parent span id
     *     and flags and set it into context, then get and validate it.
     * @tc.expected: step2. trace id is valid with same chain id, span id, parent
     *     span id and flags.
     * @tc.steps: step3. construct invalid trace id and set into context, then get
     *     and validate it.
     * @tc.expected: step3. trace id is the same with step2.
     * @tc.steps: step4. clear trace id, then get and validate it.
     * @tc.expected: step4. trace id is invalid.
     */
    HiTraceIdStruct* pTraceId = HiTraceChainGetIdAddress();
    EXPECT_EQ(0, HiTraceChainIsValid(pTraceId));
    PRINT_ID(pTraceId);

    // set thread id
    constexpr uint64_t chainId = 0xABCDEF;
    constexpr uint64_t spanId = 0x12345;
    constexpr uint64_t parentSpanId = 0x67890;
    constexpr int flags = HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN;
    HiTraceIdStruct setId;
    HiTraceChainInitId(&setId);
    HiTraceChainSetChainId(&setId, chainId);
    HiTraceChainSetFlags(&setId, flags);
    HiTraceChainSetSpanId(&setId, spanId);
    HiTraceChainSetParentSpanId(&setId, parentSpanId);
    PRINT_ID(&setId);

    HiTraceChainSetId(&setId);

    EXPECT_EQ(1, HiTraceChainIsValid(pTraceId));
    EXPECT_EQ(chainId, HiTraceChainGetChainId(pTraceId));
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, HiTraceChainGetFlags(pTraceId));
    EXPECT_EQ(spanId, HiTraceChainGetSpanId(pTraceId));
    EXPECT_EQ(parentSpanId, HiTraceChainGetParentSpanId(pTraceId));
    PRINT_ID(pTraceId);

    // set invalid id
    HiTraceIdStruct invalidId;
    HiTraceChainInitId(&invalidId);
    HiTraceChainSetId(&invalidId);

    EXPECT_EQ(1, HiTraceChainIsValid(pTraceId));
    EXPECT_EQ(chainId, HiTraceChainGetChainId(pTraceId));
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, HiTraceChainGetFlags(pTraceId));
    EXPECT_EQ(spanId, HiTraceChainGetSpanId(pTraceId));
    EXPECT_EQ(parentSpanId, HiTraceChainGetParentSpanId(pTraceId));
    PRINT_ID(pTraceId);

    // clear thread id
    HiTraceChainClearId();

    EXPECT_EQ(0, HiTraceChainIsValid(pTraceId));
    PRINT_ID(pTraceId);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_IntfTest_001
 * @tc.desc: Interconversion between trace id and bytes array.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9T
 */
HWTEST_F(HiTraceChainCTest, IntfTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. construct trace id and validate it.
     * @tc.expected: step1. trace id is valid.
     * @tc.steps: step2. convert trace id to bytes array.
     * @tc.expected: step2. convert success when array size >= id length.
     * @tc.steps: step3. convert bytes array to trace id.
     * @tc.expected: step3. convert success only when array size == id length.
     * @tc.steps: step4. convert invalid id to bytes array.
     * @tc.expected: step4. convert fail.
     * @tc.steps: step5. convert invalid bytes array to id.
     * @tc.expected: step5. convert fail.
     */
    // id to bytes
    constexpr uint64_t chainId = 0xABCDEF;
    constexpr uint64_t spanId = 0x12345;
    constexpr uint64_t parentSpanId = 0x67890;
    constexpr int flags = HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN;
    HiTraceIdStruct id = {HITRACE_ID_VALID, HITRACE_VER_1, chainId, flags, spanId, parentSpanId};
    EXPECT_EQ(1, HiTraceChainIsValid(&id));
    PRINT_ID(&id);

    constexpr int idLen = sizeof(HiTraceIdStruct);
    uint8_t bytes[idLen + 1];
    int len = HiTraceChainIdToBytes(&id, bytes, idLen - 1);
    EXPECT_EQ(0, len);
    len = HiTraceChainIdToBytes(&id, bytes, idLen + 1);
    EXPECT_EQ(idLen, len);
    len = HiTraceChainIdToBytes(&id, bytes, idLen);
    EXPECT_EQ(idLen, len);
    PRINT_ID(reinterpret_cast<HiTraceIdStruct*>(bytes));

    // bytes to id
    HiTraceIdStruct bytesToId = HiTraceChainBytesToId(bytes, idLen - 1);
    EXPECT_EQ(0, HiTraceChainIsValid(&bytesToId));
    bytesToId = HiTraceChainBytesToId(bytes, idLen + 1);
    EXPECT_EQ(0, HiTraceChainIsValid(&bytesToId));
    bytesToId = HiTraceChainBytesToId(bytes, idLen);
    EXPECT_EQ(1, HiTraceChainIsValid(&bytesToId));
    EXPECT_EQ(chainId, HiTraceChainGetChainId(&bytesToId));
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, HiTraceChainGetFlags(&bytesToId));
    EXPECT_EQ(spanId, HiTraceChainGetSpanId(&bytesToId));
    EXPECT_EQ(parentSpanId, HiTraceChainGetParentSpanId(&bytesToId));
    PRINT_ID(&bytesToId);

    // set invalid id
    HiTraceIdStruct invalidId;
    HiTraceChainInitId(&invalidId);
    EXPECT_EQ(0, HiTraceChainIdToBytes(&invalidId, bytes, idLen));
    invalidId = HiTraceChainBytesToId(nullptr, idLen);
    EXPECT_EQ(0, HiTraceChainIsValid(&invalidId));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_IntfTest_002
 * @tc.desc: Start and stop trace.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9T
 */
HWTEST_F(HiTraceChainCTest, IntfTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with flags, get trace id and validit it.
     * @tc.expected: step1. trace id and flags is valid.
     * @tc.steps: step2. stop trace, get trace id and validit it.
     * @tc.expected: step2. trace id is invalid.
     */
    // begin
    HiTraceIdStruct beginId = HiTraceChainBegin("test", HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_NO_BE_INFO);
    EXPECT_EQ(1, HiTraceChainIsValid(&beginId));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&beginId, HITRACE_FLAG_INCLUDE_ASYNC));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&beginId, HITRACE_FLAG_NO_BE_INFO));
    PRINT_ID(&beginId);

    // end
    HiTraceChainEnd(&beginId);

    HiTraceIdStruct endId = HiTraceChainGetId();
    EXPECT_EQ(0, HiTraceChainIsValid(&endId));
    PRINT_ID(&endId);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_IntfTest_003
 * @tc.desc: Start and stop trace with reentered.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9T
 */
HWTEST_F(HiTraceChainCTest, IntfTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace twice and get 2nd trace id.
     * @tc.expected: step1. 2nd trace is invalid.
     * @tc.steps: step2. get trace id and check.
     * @tc.expected: step2. trace id is valid and same with 1st id.
     * @tc.steps: step3. set chain id with wrong id and get trace id.
     * @tc.expected: step3. trace id is valid and same with 1st id.
     * @tc.steps: step4. stop trace twice and get trace id.
     * @tc.expected: step4. trace id is invalid.
     */
    HiTraceIdStruct beginId = HiTraceChainBegin("begin", HITRACE_FLAG_INCLUDE_ASYNC);
    PRINT_ID(&beginId);

    // reenter begin
    HiTraceIdStruct reBeginId = HiTraceChainBegin("reenter begin", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, HiTraceChainIsValid(&reBeginId));
    EXPECT_NE(HiTraceChainGetChainId(&reBeginId), HiTraceChainGetChainId(&beginId));
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&reBeginId, HITRACE_FLAG_INCLUDE_ASYNC));
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&reBeginId, HITRACE_FLAG_TP_INFO));
    PRINT_ID(&reBeginId);

    // reenter end
    HiTraceChainEnd(&reBeginId);

    HiTraceIdStruct endId = HiTraceChainGetId();
    EXPECT_EQ(1, HiTraceChainIsValid(&endId));
    EXPECT_EQ(HiTraceChainGetChainId(&endId), HiTraceChainGetChainId(&beginId));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&endId, HITRACE_FLAG_INCLUDE_ASYNC));
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&endId, HITRACE_FLAG_TP_INFO));
    PRINT_ID(&endId);

    // end with wrong chainId
    HiTraceIdStruct wrongBeginId = beginId;
    HiTraceChainSetChainId(&wrongBeginId, HiTraceChainGetChainId(&beginId) + 1);
    HiTraceChainEnd(&wrongBeginId);

    HiTraceIdStruct wrongEndId = HiTraceChainGetId();
    EXPECT_EQ(1, HiTraceChainIsValid(&wrongEndId));
    EXPECT_EQ(HiTraceChainGetChainId(&wrongEndId), HiTraceChainGetChainId(&beginId));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&wrongEndId, HITRACE_FLAG_INCLUDE_ASYNC));
    PRINT_ID(&wrongEndId);

    // end
    HiTraceChainEnd(&beginId);

    HiTraceIdStruct reEndId = HiTraceChainGetId();
    EXPECT_EQ(0, HiTraceChainIsValid(&reEndId));
    PRINT_ID(&reEndId);

    // end with invalid thread id
    HiTraceChainEnd(&beginId);

    HiTraceIdStruct endInvalidId = HiTraceChainGetId();
    EXPECT_EQ(0, HiTraceChainIsValid(&endInvalidId));
    PRINT_ID(&endInvalidId);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_SpanTest_001
 * @tc.desc: Create child and grand child span.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA2
 */
HWTEST_F(HiTraceChainCTest, SpanTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_DONOT_CREATE_SPAN,
     *     get and check flags.
     * @tc.expected: step1. flags is same with set and span id is 0.
     * @tc.steps: step2. create child id.
     * @tc.expected: step2. child id has same span id with parent.
     * @tc.steps: step3. set child id into context.
     * @tc.steps: step4. create grand child id.
     * @tc.expected: step4. grand child id has same span id with parent and child.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test", 0);
    EXPECT_EQ(0, HiTraceChainGetFlags(&id));
    EXPECT_EQ(0UL, HiTraceChainGetSpanId(&id));
    EXPECT_EQ(0UL, HiTraceChainGetParentSpanId(&id));
    PRINT_ID(&id);

    // create child span
    HiTraceIdStruct childId = HiTraceChainCreateSpan();
    EXPECT_EQ(1, HiTraceChainIsValid(&childId));
    EXPECT_EQ(HiTraceChainGetFlags(&childId), HiTraceChainGetFlags(&id));
    EXPECT_EQ(HiTraceChainGetChainId(&childId), HiTraceChainGetChainId(&id));
    EXPECT_EQ(HiTraceChainGetParentSpanId(&childId), HiTraceChainGetSpanId(&id));
    PRINT_ID(&childId);

    // set child id to thread id
    HiTraceChainSetId(&childId);

    // continue to create child span
    HiTraceIdStruct grandChildId = HiTraceChainCreateSpan();
    EXPECT_EQ(1, HiTraceChainIsValid(&grandChildId));
    EXPECT_EQ(HiTraceChainGetFlags(&grandChildId), HiTraceChainGetFlags(&id));
    EXPECT_EQ(HiTraceChainGetChainId(&grandChildId), HiTraceChainGetChainId(&id));
    EXPECT_EQ(HiTraceChainGetParentSpanId(&grandChildId), HiTraceChainGetSpanId(&childId));
    PRINT_ID(&grandChildId);

    // end
    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_SpanTest_002
 * @tc.desc: Start and stop trace with reentered.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA2
 */
HWTEST_F(HiTraceChainCTest, SpanTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_DONOT_CREATE_SPAN,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_DONOT_CREATE_SPAN is enabled.
     * @tc.steps: step2. create child id.
     * @tc.expected: step2. child id is same with parent id.
     */
    // begin with "donot create span" flag
    HiTraceIdStruct id = HiTraceChainBegin("test", HITRACE_FLAG_DONOT_CREATE_SPAN);
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_DONOT_CREATE_SPAN));
    PRINT_ID(&id);

    // create child span
    HiTraceIdStruct childId = HiTraceChainCreateSpan();
    EXPECT_EQ(1, HiTraceChainIsValid(&childId));
    EXPECT_EQ(HiTraceChainGetFlags(&childId), HiTraceChainGetFlags(&id));
    EXPECT_EQ(HiTraceChainGetChainId(&childId), HiTraceChainGetChainId(&id));
    EXPECT_EQ(HiTraceChainGetSpanId(&childId), HiTraceChainGetSpanId(&id));
    EXPECT_EQ(HiTraceChainGetParentSpanId(&childId), HiTraceChainGetParentSpanId(&id));
    PRINT_ID(&childId);

    // end
    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_001
 * @tc.desc: Start trace with HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_TP_INFO,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add trace point info with null and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test tp flag", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_TP_INFO));
    HiTraceChainTracepoint(HITRACE_TP_CS, &id, "client send msg content %d", 12);

    HiTraceChainTracepoint(HITRACE_TP_CS, nullptr, "client send msg content %d", 12);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_002
 * @tc.desc: Start trace without HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_TP_INFO is not enabled.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     */
    // begin with tp flag
    HiTraceIdStruct id = HiTraceChainBegin("test no tp flag", HITRACE_FLAG_INCLUDE_ASYNC);
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_TP_INFO));
    HiTraceChainTracepoint(HITRACE_TP_CS, &id, "client send msg content %d", 12);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_003
 * @tc.desc: Start trace with HITRACE_FLAG_D2D_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_D2D_TP_INFO,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add D2D trace point info with null and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     * @tc.steps: step3. add trace point info with id and check logs.
     * @tc.expected: step3. trace point cannot be found in logs.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test D2D tp flag", HITRACE_FLAG_D2D_TP_INFO);
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_D2D_TP_INFO));
    HiTraceChainTracepointEx(HITRACE_CM_DEVICE, HITRACE_TP_CS, &id, "client send msg content %d", 12);
    HiTraceChainTracepointEx(HITRACE_CM_PROCESS, HITRACE_TP_CS, &id, "cannot be found %d", 22);
    HiTraceChainTracepointEx(HITRACE_CM_THREAD, HITRACE_TP_CS, &id, "cannot be found %d", 32);
    HiTraceChainTracepointEx(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, "cannot be found %d", 42);

    HiTraceChainTracepointEx(HITRACE_CM_DEVICE, HITRACE_TP_CS, nullptr, "cannot be found %d", 13);

    HiTraceChainTracepoint(HITRACE_TP_CS, &id, "cannot be found %d", 14);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_004
 * @tc.desc: Start trace without HITRACE_FLAG_D2D_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_D2D_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is not enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test no D2D tp flag", HITRACE_FLAG_INCLUDE_ASYNC);
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_D2D_TP_INFO));
    HiTraceChainTracepointEx(HITRACE_CM_DEVICE, HITRACE_TP_CS, &id, "cannot be found %d", 12);
    HiTraceChainTracepointEx(HITRACE_CM_PROCESS, HITRACE_TP_CS, &id, "cannot be found %d", 22);
    HiTraceChainTracepointEx(HITRACE_CM_THREAD, HITRACE_TP_CS, &id, "cannot be found %d", 32);
    HiTraceChainTracepointEx(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, "cannot be found %d", 42);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_005
 * @tc.desc: Start trace with HITRACE_FLAG_D2D_TP_INFO and HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_005, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_D2D_TP_INFO | HITRACE_FLAG_TP_INFO,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is enabled.
     * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step3. add trace point info with id and check logs.
     * @tc.expected: step3. trace point can be found in logs.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test D2D | TP tp flag", HITRACE_FLAG_D2D_TP_INFO | HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_D2D_TP_INFO));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_TP_INFO));
    HiTraceChainTracepointEx(HITRACE_CM_DEVICE, HITRACE_TP_CS, &id, "client send msg content %d", 12);
    HiTraceChainTracepointEx(HITRACE_CM_PROCESS, HITRACE_TP_CS, &id, "client send msg content %d", 22);
    HiTraceChainTracepointEx(HITRACE_CM_THREAD, HITRACE_TP_CS, &id, "client send msg content %d", 32);
    HiTraceChainTracepointEx(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, "client send msg content %d", 42);

    HiTraceChainTracepoint(HITRACE_TP_CS, &id, "client send msg content %d", 13);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_006
 * @tc.desc: Start trace without HITRACE_FLAG_D2D_TP_INFO, but with HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_006, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is not enabled.
     * * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test no D2D, but tp flag", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_D2D_TP_INFO));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_TP_INFO));
    HiTraceChainTracepointEx(HITRACE_CM_DEVICE, HITRACE_TP_CS, &id, "client send msg content %d", 12);
    HiTraceChainTracepointEx(HITRACE_CM_PROCESS, HITRACE_TP_CS, &id, "client send msg content %d", 22);
    HiTraceChainTracepointEx(HITRACE_CM_THREAD, HITRACE_TP_CS, &id, "client send msg content %d", 32);
    HiTraceChainTracepointEx(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, "client send msg content %d", 42);

    HiTraceChainTracepoint(HITRACE_TP_CS, &id, "client send msg content %d", 13);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointTest_007
 * @tc.desc: Start trace without HITRACE_FLAG_D2D_TP_INFO, but with HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCTest, TracepointTest_007, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is not enabled.
     * * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     */
    HiTraceIdStruct id = HiTraceChainBegin("test no D2D, but tp flag", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_D2D_TP_INFO));
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&id, HITRACE_FLAG_TP_INFO));
    HiTraceChainTracepointExWithArgsWrapper(HITRACE_CM_DEVICE, HITRACE_TP_CS, &id, "client send msg content %d", 12);
    HiTraceChainTracepointExWithArgsWrapper(HITRACE_CM_PROCESS, HITRACE_TP_CS, &id, "client send msg content %d", 22);
    HiTraceChainTracepointExWithArgsWrapper(HITRACE_CM_THREAD, HITRACE_TP_CS, &id, "client send msg content %d", 32);
    HiTraceChainTracepointExWithArgsWrapper(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, "client send msg content %d", 42);

    HiTraceChainTracepointWithArgsWrapper(HITRACE_TP_CS, &id, "client send msg content %d", 13);

    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_SyncAsyncTest_001
 * @tc.desc: Start trace with SYNC or ASYNC.
 * @tc.type: FUNC
 * @tc.require: AR000CQ0G7
 */
HWTEST_F(HiTraceChainCTest, SyncAsyncTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_INCLUDE_ASYNC flag.
     *    get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_INCLUDE_ASYNC is not enabled.
     * @tc.steps: step2. start trace with HITRACE_FLAG_INCLUDE_ASYNC flag.
     *    get and check flags.
     * @tc.expected: step2. HITRACE_FLAG_INCLUDE_ASYNC is enabled.
     */
    // begin with sync flag
    HiTraceIdStruct syncId = HiTraceChainBegin("test sync only", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, HiTraceChainIsFlagEnabled(&syncId, HITRACE_FLAG_INCLUDE_ASYNC));
    HiTraceChainTracepoint(HITRACE_TP_CS, &syncId, "client send msg: %s", "sync");

    HiTraceChainEnd(&syncId);

    // begin with async flag
    HiTraceIdStruct asyncId = HiTraceChainBegin("test sync+async", HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&asyncId, HITRACE_FLAG_INCLUDE_ASYNC));
    HiTraceChainTracepoint(HITRACE_TP_CS, &asyncId, "client send msg: %s", "async");

    HiTraceChainEnd(&asyncId);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_RestoreTest_001
 * @tc.desc: Start normal trace.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, RestoreTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without any flag.
     * @tc.expected: step1. no flag is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceIdStruct id = HiTraceChainBegin("RestoreTest_001", HITRACE_FLAG_TP_INFO);
    PRINT_ID(&id);

    //generate new trace id
    HiTraceIdStruct newId{0};
    HiTraceChainInitId(&newId);
    newId.valid=HITRACE_ID_VALID;
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    HiTraceChainSetChainId(&newId, chainId);
    HiTraceChainSetSpanId(&newId, spanId);
    HiTraceChainSetParentSpanId(&newId, parentSpanId);
    PRINT_ID(&newId);

    // set new id and save old id
    HiTraceIdStruct oldId = HiTraceChainSaveAndSetId(&newId);
    PRINT_ID(&oldId);
    HiTraceIdStruct currentId = HiTraceChainGetId();
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChainRestoreId(&oldId);
    HiTraceIdStruct currentId2 = HiTraceChainGetId();
    EXPECT_EQ(HiTraceChainGetChainId(&id), HiTraceChainGetChainId(&currentId2));
    EXPECT_EQ(HiTraceChainGetSpanId(&id), HiTraceChainGetParentSpanId(&currentId2));
    EXPECT_EQ(HiTraceChainGetParentSpanId(&id), HiTraceChainGetParentSpanId(&currentId2));
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_RestoreTest_002
 * @tc.desc: Start trace with HITRACE_FLAG_INCLUDE_ASYNC.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, RestoreTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with flag HITRACE_FLAG_INCLUDE_ASYNC.
     * @tc.expected: step1. flag HITRACE_FLAG_INCLUDE_ASYNC is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceIdStruct id = HiTraceChainBegin("RestoreTest_002", HITRACE_FLAG_TP_INFO | HITRACE_FLAG_INCLUDE_ASYNC);
    PRINT_ID(&id);

    //generate new trace id
    HiTraceIdStruct newId{0};
    HiTraceChainInitId(&newId);
    newId.valid=HITRACE_ID_VALID;
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    HiTraceChainSetChainId(&newId, chainId);
    HiTraceChainSetSpanId(&newId, spanId);
    HiTraceChainSetParentSpanId(&newId, parentSpanId);
    PRINT_ID(&newId);

    // set new id and save old id
    HiTraceIdStruct oldId = HiTraceChainSaveAndSetId(&newId);
    PRINT_ID(&oldId);
    HiTraceIdStruct currentId = HiTraceChainGetId();
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChainRestoreId(&oldId);
    HiTraceIdStruct currentId2 = HiTraceChainGetId();
    EXPECT_EQ(1, HiTraceChainIsFlagEnabled(&currentId2, HITRACE_FLAG_INCLUDE_ASYNC));
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_RestoreTest_003
 * @tc.desc: Start normal trace.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, RestoreTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with version HITRACE_VER_1.
     * @tc.expected: step1. flag HITRACE_VER_1 is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */
    // begin trace
    HiTraceIdStruct id = HiTraceChainBegin("RestoreTest_003", HITRACE_FLAG_TP_INFO);
    PRINT_ID(&id);

    //generate new trace id
    HiTraceIdStruct newId{0};
    HiTraceChainInitId(&newId);
    newId.valid=HITRACE_ID_VALID;
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    HiTraceChainSetChainId(&newId, chainId);
    HiTraceChainSetSpanId(&newId, spanId);
    HiTraceChainSetParentSpanId(&newId, parentSpanId);
    PRINT_ID(&newId);

    // set new id and save old id
    HiTraceIdStruct oldId = HiTraceChainSaveAndSetId(&newId);
    PRINT_ID(&oldId);
    HiTraceIdStruct currentId = HiTraceChainGetId();
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChainRestoreId(&oldId);
    HiTraceIdStruct currentId2 = HiTraceChainGetId();
    EXPECT_EQ(HITRACE_VER_1, currentId2.ver);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChainEnd(&id);
}
/**
 * @tc.name: Dfx_HiTraceChainCTest_RestoreTest_004
 * @tc.desc: Start normal trace and create span.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, RestoreTest_004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without any flag, then create span.
     * @tc.expected: step1. no flag is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */
    // begin trace
    HiTraceIdStruct id = HiTraceChainBegin("RestoreTest_004", HITRACE_FLAG_TP_INFO);
    HiTraceChainCreateSpan();
    PRINT_ID(&id);

    //generate new trace id
    HiTraceIdStruct newId{0};
    HiTraceChainInitId(&newId);
    newId.valid=HITRACE_ID_VALID;
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    HiTraceChainSetChainId(&newId, chainId);
    HiTraceChainSetSpanId(&newId, spanId);
    HiTraceChainSetParentSpanId(&newId, parentSpanId);
    PRINT_ID(&newId);

    // set new id and save old id
    HiTraceIdStruct oldId = HiTraceChainSaveAndSetId(&newId);
    HiTraceChainCreateSpan();
    PRINT_ID(&oldId);
    HiTraceIdStruct currentId = HiTraceChainGetId();
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChainRestoreId(&oldId);
    HiTraceIdStruct currentId2 = HiTraceChainGetId();
    EXPECT_EQ(HiTraceChainGetChainId(&id), HiTraceChainGetChainId(&currentId2));
    EXPECT_EQ(HiTraceChainGetSpanId(&id), HiTraceChainGetParentSpanId(&currentId2));
    EXPECT_EQ(HiTraceChainGetParentSpanId(&id), HiTraceChainGetParentSpanId(&currentId2));
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_RestoreTest_005
 * @tc.desc: Start normal trace.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, RestoreTest_005, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without any flag.
     * @tc.expected: step1. no flag is enabled.
     * @tc.steps: step2. generate a temporary trace id with HITRACE_ID_INVALID flag.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace is invalid.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceIdStruct id = HiTraceChainBegin("RestoreTest_005", HITRACE_FLAG_TP_INFO);
    PRINT_ID(&id);

    //generate new trace id
    HiTraceIdStruct newId{0};
    HiTraceChainInitId(&newId);
    newId.valid=HITRACE_ID_INVALID;
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    HiTraceChainSetChainId(&newId, chainId);
    HiTraceChainSetSpanId(&newId, spanId);
    HiTraceChainSetParentSpanId(&newId, parentSpanId);
    PRINT_ID(&newId);

    // set new id and save old id
    HiTraceIdStruct oldId = HiTraceChainSaveAndSetId(&newId);
    PRINT_ID(&oldId);
    HiTraceIdStruct currentId = HiTraceChainGetId();
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChainRestoreId(&oldId);
    HiTraceIdStruct currentId2 = HiTraceChainGetId();
    EXPECT_EQ(HiTraceChainGetChainId(&id), HiTraceChainGetChainId(&currentId2));
    EXPECT_EQ(HiTraceChainGetSpanId(&id), HiTraceChainGetParentSpanId(&currentId2));
    EXPECT_EQ(HiTraceChainGetParentSpanId(&id), HiTraceChainGetParentSpanId(&currentId2));
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, &currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, &currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_THREAD, HITRACE_TP_CS, &currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChainTracepointExWithArgsWrapper(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, &currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChainEnd(&id);
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_DomainTest_001
 * @tc.desc: Start normal trace with domain 0.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, DomainTest_001, TestSize.Level1)
{
    unsigned int domain = 0x0;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("DomainTest_001", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));
    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_DomainTest_002
 * @tc.desc: Start normal trace with domain 0x6666.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, DomainTest_002, TestSize.Level1)
{
    unsigned int domain = 0x6666;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("DomainTest_002", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));
    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_DomainTest_003
 * @tc.desc: Test TracepointEx with domain 0.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, DomainTest_003, TestSize.Level1)
{
    unsigned int domain = 0x0;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("DomainTest_003", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));
    HiTraceChainTracepointExWithArgsDomainWrapper(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, domain, "client send %d", 3);
    HiTraceChainTracepointExWithDomain(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, domain, "client send %d", 3);
    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_DomainTest_004
 * @tc.desc: Test TracepointEx with domain 0x6666.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, DomainTest_004, TestSize.Level1)
{
    unsigned int domain = 0x6666;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("DomainTest_004", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));
    HiTraceChainTracepointExWithArgsDomainWrapper(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, domain, "client send %d", 4);
    HiTraceChainTracepointExWithDomain(HITRACE_CM_DEFAULT, HITRACE_TP_CS, &id, domain, "client send %d", 4);
    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointParamsTest_001
 * @tc.desc: Test Tracepoint invalid mode.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, TracepointParamsTest_001, TestSize.Level1)
{
    unsigned int domain = 0x6666;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("TracepointParamsTest_001", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));

    // HITRACE_CM_MIN = 0
    HiTraceCommunicationMode mode = static_cast<HiTraceCommunicationMode>(-1);
    HiTraceTracepointType type = HITRACE_TP_CS;
    HiTraceChainTracepointExWithDomain(mode, type, &id, domain, "client send %d", 1);
    // HITRACE_CM_MAX = 3
    mode = static_cast<HiTraceCommunicationMode>(4);
    HiTraceChainTracepointExWithDomain(mode, type, &id, domain, "client send %d", 1);

    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointParamsTest_002
 * @tc.desc: Test Tracepoint invalid type.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, TracepointParamsTest_002, TestSize.Level1)
{
    unsigned int domain = 0x6666;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("TracepointParamsTest_002", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));

    HiTraceCommunicationMode mode = HITRACE_CM_DEFAULT;
    // HITRACE_TP_MIN = 0
    HiTraceTracepointType type = static_cast<HiTraceTracepointType>(-1);
    HiTraceChainTracepointExWithDomain(mode, type, &id, domain, "DEFAULT send %d", 2);
    // HITRACE_TP_MAX = 4
    type = static_cast<HiTraceTracepointType>(5);
    HiTraceChainTracepointExWithDomain(mode, type, &id, domain, "DEFAULT send %d", 2);

    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}

/**
 * @tc.name: Dfx_HiTraceChainCTest_TracepointParamsTest_003
 * @tc.desc: Test Tracepoint invalid HiTraceIdStruct.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCTest, TracepointParamsTest_003, TestSize.Level1)
{
    unsigned int domain = 0x6666;
    HiTraceIdStruct id = HiTraceChainBeginWithDomain("TracepointParamsTest_003", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_TRUE(HiTraceChainIsValid(&id));

    HiTraceIdStruct invalidId = HiTraceChainBeginWithDomain("TracepointParamsTest_003", HITRACE_FLAG_TP_INFO, domain);
    EXPECT_FALSE(HiTraceChainIsValid(&invalidId));

    HiTraceCommunicationMode mode = HITRACE_CM_DEFAULT;
    HiTraceTracepointType type = HITRACE_TP_CS;
    HiTraceChainTracepointExWithDomain(mode, type, &invalidId, domain, "client send %d", 3);

    HiTraceChainEndWithDomain(&id, domain);
    id = HiTraceChainGetId();
    EXPECT_FALSE(HiTraceChainIsValid(&id));
}
}  // namespace HiviewDFX
}  // namespace OHOS
