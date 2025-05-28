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
#include "trace_dump_pipe.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
static const char* const TEST_TRACE_TEMP_FILE = "/data/local/tmp/test_trace_file";
constexpr int BYTE_PER_MB = 1024 * 1024;
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
    ASSERT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_RECORDING,
        "",
        0,
        0,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
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
    ASSERT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_RECORDING,
        TEST_TRACE_TEMP_FILE,
        0,
        0,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
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
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto ret = traceDumpExecutor.DumpTrace(param);
    ASSERT_EQ(ret.code, TraceErrorCode::SUCCESS);
    GTEST_LOG_(INFO) << "snapshot file: " << ret.outputFile;
    ASSERT_GT(GetFileSize(ret.outputFile), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpExecutorTest004
 * @tc.desc: Test TraceDumpExecutor class StartCacheTraceLoop/StopCacheTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest004, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    traceDumpExecutor.ClearCacheTraceFiles();
    ASSERT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_CACHE,
        "",
        0, // file limit
        0, // file size
        0, // trace start time
        std::numeric_limits<uint64_t>::max() // trace end time
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        ASSERT_TRUE(traceDumpExecutor.StartCacheTraceLoop(param, 50 * BYTE_PER_MB, 5)); // 50 : file size 5 : slice
    };
    std::thread traceLoopThread(it, param);
    sleep(8); // 8 : 8 seconds
    traceDumpExecutor.StopCacheTraceLoop();
    traceLoopThread.join();
    auto list = traceDumpExecutor.GetCacheTraceFiles();
    ASSERT_EQ(list.size(), 2); // 2 : should have 2 files
    for (const auto& file : list) {
        GTEST_LOG_(INFO) << file.filename;
        ASSERT_GT(GetFileSize(file.filename), 0);
    }
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpExecutorTest005
 * @tc.desc: Test TraceDumpExecutor class StartCacheTraceLoop/StopCacheTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest005, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    traceDumpExecutor.ClearCacheTraceFiles();
    ASSERT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TRACE_TYPE::TRACE_CACHE,
        "",
        0, // file limit
        0, // file size
        0, // trace start time
        std::numeric_limits<uint64_t>::max() // trace end time
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        ASSERT_TRUE(traceDumpExecutor.StartCacheTraceLoop(param, 50 * BYTE_PER_MB, 5)); // 50 : file size  5 : slice
    };
    std::thread traceLoopThread(it, param);
    sleep(7);
    auto list1 = traceDumpExecutor.GetCacheTraceFiles();
    ASSERT_EQ(list1.size(), 2); // 2 : should have 2 files
    for (const auto& file : list1) {
        GTEST_LOG_(INFO) << file.filename;
        ASSERT_GT(GetFileSize(file.filename), 0);
    }
    sleep(1);
    traceDumpExecutor.StopCacheTraceLoop();
    auto list2 = traceDumpExecutor.GetCacheTraceFiles();
    ASSERT_EQ(list2.size(), 3); // 3 : should have 3 files
    for (const auto& file : list2) {
        GTEST_LOG_(INFO) << file.filename;
        ASSERT_GT(GetFileSize(file.filename), 0);
    }
    traceLoopThread.join();
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpPipeTest001
 * @tc.desc: Test TraceDumpExecutor class trace task pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest001, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task;
        EXPECT_TRUE(dumpPipe2->ReadTraceTask(1000, task));
        EXPECT_EQ(task.status, TraceDumpStatus::READ_DONE);
        _exit(0);
    }
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task = {
        .status = TraceDumpStatus::READ_DONE,
    };
    EXPECT_TRUE(dumpPipe1->SubmitTraceDumpTask(task));
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest002
 * @tc.desc: Test TraceDumpExecutor class sync dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest002, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .status = TraceDumpStatus::READ_DONE,
        };
        EXPECT_TRUE(dumpPipe2->WriteSyncReturn(task));
        _exit(0);
    }
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe1->ReadSyncDumpRet(1, task));
    EXPECT_EQ(task.status, TraceDumpStatus::READ_DONE);
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest003
 * @tc.desc: Test TraceDumpExecutor class async dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest003, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .status = TraceDumpStatus::FINISH,
        };
        EXPECT_TRUE(dumpPipe2->WriteAsyncReturn(task));
        _exit(0);
    }
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe1->ReadAsyncDumpRet(1, task));
    EXPECT_EQ(task.status, TraceDumpStatus::FINISH);
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS