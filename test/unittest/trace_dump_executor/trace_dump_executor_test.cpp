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
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "hitrace_dump.h"
#include "trace_dump_executor.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
static const char* const TEST_TRACE_TEMP_FILE = "/data/local/tmp/test_trace_file";
}

class TraceDumpExecutorTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
static off_t GetFileSize(const std::string& file)
{
    struct stat fileStat;
    if (stat(file.c_str(), &fileStat) != 0) {
        GTEST_LOG_(ERROR) << "Failed to get file size of " << file;
        return 0;
    }
    return fileStat.st_size;
}

/**
 * @tc.name: TraceDumpExecutorTest001
 * @tc.desc: Test TraceDumpExecutor class StartDumpTraceLoop/StopDumpTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest001, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_RECORDING,
        "",
        0,
        0,
    };
    auto it = [&](const TraceDumpParam& param) {
        ASSERT_TRUE(traceDumpExecutor.StartDumpTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(3);
    auto list = traceDumpExecutor.StopDumpTraceLoop();
    ASSERT_GT(list.size(), 0);
    for (const auto& filename : list) {
        GTEST_LOG_(INFO) << filename;
        ASSERT_GT(GetFileSize(filename), 0);
    }
    traceLoopThread.join();
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpExecutorTest002
 * @tc.desc: Test TraceDumpExecutor class StartDumpTraceLoop/StopDumpTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest002, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_RECORDING,
        TEST_TRACE_TEMP_FILE,
        0,
        0,
    };
    auto it = [&](const TraceDumpParam& param) {
        ASSERT_TRUE(traceDumpExecutor.StartDumpTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(3);
    ASSERT_GT(traceDumpExecutor.StopDumpTraceLoop().size(), 0);
    traceLoopThread.join();
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(WARNING) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceDumpExecutorTest003
 * @tc.desc: Test TraceDumpExecutor class DumpTrace function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest003, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_SNAPSHOT,
        "",
        0,
        0,
    };
    const string file = traceDumpExecutor.DumpTrace(param);
    GTEST_LOG_(INFO) << "snapshot file: " << file;
    ASSERT_GT(GetFileSize(file), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpExecutorTest004
 * @tc.desc: Test TraceDumpExecutor class DumpTraceAsync function in sync return scenario
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest004, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_SNAPSHOT,
        "",
        0,
        0,
    };
    auto start = std::chrono::steady_clock::now();
    auto ret = traceDumpExecutor.DumpTraceAsync(param, [&](bool success) {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        std::cout << "Async callback received after " << duration << " seconds. result: "
                  << std::boolalpha << success << std::endl;
    }, 8); // 8 : 8 seconds timeout
    GTEST_LOG_(INFO) << "code : " << static_cast<int>(ret.code) << ", tracefile: " << ret.outputFile;
    ASSERT_EQ(ret.code, TraceErrorCode::SUCCESS);
    ASSERT_GT(GetFileSize(ret.outputFile), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpExecutorTest005
 * @tc.desc: Test TraceDumpExecutor class DumpTraceAsync function in async return scenario
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest005, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_SNAPSHOT,
        "",
        0,
        0,
    };
    auto start = std::chrono::steady_clock::now();
    auto ret = traceDumpExecutor.DumpTraceAsync(param, [&](bool success) {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        std::cout << "Async callback received after " << duration << " seconds. result: "
                  << std::boolalpha << success << std::endl;
        EXPECT_TRUE(success) << "DumpTraceAsync return false.";
    }, 2); // 2 : 2 seconds timeout
    GTEST_LOG_(INFO) << "code : " << static_cast<int>(ret.code) << ", tracefile: " << ret.outputFile;
    ASSERT_EQ(ret.code, TraceErrorCode::ASYNC_DUMP);
    std::this_thread::sleep_for(std::chrono::seconds(3)); // 3 : wait 3 seconds to check trace file size.
    ASSERT_GT(GetFileSize(ret.outputFile), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS