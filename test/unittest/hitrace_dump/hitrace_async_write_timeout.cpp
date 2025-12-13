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

#include <chrono>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <thread>

#include "hitrace_dump.h"
#include "trace_file_utils.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
using namespace testing::ext;
class HitraceAsyncWriteTimeoutTest : public testing::Test {
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

void HitraceAsyncWriteTimeoutTest::SetUp()
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
 * @tc.name: AsyncWriteTimeoutTest001
 * @tc.desc: Test DumpTraceAsync func, execute without 5 seconds timeout
 * @tc.type: FUNC
 */
HWTEST_F(HitraceAsyncWriteTimeoutTest, AsyncWriteTimeoutTest001, TestSize.Level2)
{
    const std::vector<std::string> tagGroups = {"scene_performance"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);

    std::function<void(TraceRetInfo)> func = [](TraceRetInfo traceInfo) {
        off_t totalFileSz = 0;
        for (auto& files : traceInfo.outputFiles) {
            totalFileSz += GetFileSize(files);
            GTEST_LOG_(INFO) << "output: " << files << " file size : " << GetFileSize(files);
        }
        EXPECT_EQ(totalFileSz, traceInfo.fileSize);
    };
    auto ret = DumpTraceAsync(0, 0, INT64_MAX, func);
    EXPECT_EQ(static_cast<int>(ret.errorCode), TraceErrorCode::SUCCESS);
    GTEST_LOG_(INFO) << "interface return file size : " << ret.fileSize;
    for (auto file : ret.outputFiles) {
        GTEST_LOG_(INFO) << "interface return file : " << file;
    }
    ret = DumpTraceAsync(0, 0, INT64_MAX, func);
    EXPECT_EQ(static_cast<int>(ret.errorCode), TraceErrorCode::SUCCESS);
    GTEST_LOG_(INFO) << "interface return file size : " << ret.fileSize;
    for (auto file : ret.outputFiles) {
        GTEST_LOG_(INFO) << "interface return file : " << file;
    }
    std::this_thread::sleep_for(std::chrono::seconds(15)); // 15 : wait 15 seconds to avoid crash in SIGPIPE
    // Close trace after async dump
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS