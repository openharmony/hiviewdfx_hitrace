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
#include <set>
#include <string>

#include "hitrace_dump.h"
#include "trace_file_utils.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
using namespace testing::ext;
class HitraceAsyncReadTimeoutTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        system("service_control stop hiview");
    }

    static void TearDownTestCase(void)
    {
        system("service_control start hiview");
    }

    void SetUp();
    void TearDown() {}
};

void HitraceAsyncReadTimeoutTest::SetUp()
{
    CloseTrace();
    std::set<std::string> tracefiles;
    GetTraceFileNamesInDir(tracefiles, TraceDumpType::TRACE_CACHE);
    GetTraceFileNamesInDir(tracefiles, TraceDumpType::TRACE_RECORDING);
    GetTraceFileNamesInDir(tracefiles, TraceDumpType::TRACE_SNAPSHOT);
    for (auto& file : tracefiles) {
        if (remove(file.c_str()) != 0) {
            GTEST_LOG_(ERROR) << "remove " << file << " failed.";
        }
    }
}

namespace {
/**
 * @tc.name: AsyncReadTimeoutTest001
 * @tc.desc: Test DumpTraceAsync func
 * @tc.type: FUNC
 */
HWTEST_F(HitraceAsyncReadTimeoutTest, AsyncReadTimeoutTest001, TestSize.Level2)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    std::function<void(TraceRetInfo)> func = [](TraceRetInfo traceInfo) {
        EXPECT_EQ(traceInfo.errorCode, TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT);
    };
    auto ret = DumpTraceAsync(0, 0, INT64_MAX, func);
    EXPECT_EQ(ret.errorCode, TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: AsyncReadTimeoutTest002
 * @tc.desc: Test DumpTraceAsync func, call twice.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceAsyncReadTimeoutTest, AsyncReadTimeoutTest002, TestSize.Level2)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    std::function<void(TraceRetInfo)> func = [](TraceRetInfo traceInfo) {
        EXPECT_EQ(traceInfo.errorCode, TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT);
    };
    auto ret = DumpTraceAsync(0, 0, INT64_MAX, func);
    EXPECT_EQ(ret.errorCode, TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT);
    ret = DumpTraceAsync(0, 0, INT64_MAX, func);
    EXPECT_EQ(ret.errorCode, TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS