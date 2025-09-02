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
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TraceDumpType::TRACE_RECORDING,
        "",
        0,
        0,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartDumpTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(3);
    auto list = traceDumpExecutor.StopDumpTraceLoop();
    EXPECT_GT(list.size(), 0);
    for (const auto& filename : list) {
        GTEST_LOG_(INFO) << filename;
        EXPECT_GT(GetFileSize(filename), 0);
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
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TraceDumpType::TRACE_RECORDING,
        TEST_TRACE_TEMP_FILE,
        0,
        0,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartDumpTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(3);
    EXPECT_GT(traceDumpExecutor.StopDumpTraceLoop().size(), 0);
    traceLoopThread.join();
    EXPECT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
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
        TraceDumpType::TRACE_SNAPSHOT,
        "",
        0,
        0,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto ret = traceDumpExecutor.DumpTrace(param);
    EXPECT_EQ(ret.code, TraceErrorCode::SUCCESS);
    GTEST_LOG_(INFO) << "snapshot file: " << ret.outputFile;
    EXPECT_GT(GetFileSize(ret.outputFile), 0);
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
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TraceDumpType::TRACE_CACHE,
        "",
        0, // file limit
        0, // file size
        0, // trace start time
        std::numeric_limits<uint64_t>::max() // trace end time
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartCacheTraceLoop(param, 50 * BYTE_PER_MB, 5)); // 50 : file size 5 : slice
    };
    std::thread traceLoopThread(it, param);
    sleep(8); // 8 : 8 seconds
    traceDumpExecutor.StopCacheTraceLoop();
    traceLoopThread.join();
    auto list = traceDumpExecutor.GetCacheTraceFiles();
    EXPECT_EQ(list.size(), 2); // 2 : should have 2 files
    for (const auto& file : list) {
        GTEST_LOG_(INFO) << file.filename;
        EXPECT_GT(GetFileSize(file.filename), 0);
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
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        TraceDumpType::TRACE_CACHE,
        "",
        0, // file limit
        0, // file size
        0, // trace start time
        std::numeric_limits<uint64_t>::max() // trace end time
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartCacheTraceLoop(param, 50 * BYTE_PER_MB, 5)); // 50 : file size  5 : slice
    };
    std::thread traceLoopThread(it, param);
    sleep(7);
    auto list1 = traceDumpExecutor.GetCacheTraceFiles();
    EXPECT_GE(list1.size(), 2); // 2 : should have 2 files
    for (const auto& file : list1) {
        GTEST_LOG_(INFO) << file.filename;
        EXPECT_GT(GetFileSize(file.filename), 0);
    }
    sleep(1);
    traceDumpExecutor.StopCacheTraceLoop();
    auto list2 = traceDumpExecutor.GetCacheTraceFiles();
    EXPECT_GE(list2.size(), 3); // 3 : should have 3 files
    for (const auto& file : list2) {
        GTEST_LOG_(INFO) << file.filename;
        EXPECT_GT(GetFileSize(file.filename), 0);
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

/**
 * @tc.name: TraceDumpPipeTest004
 * @tc.desc: Test TraceDumpExecutor class trace task pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest004, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task1;
        EXPECT_TRUE(dumpPipe2->ReadTraceTask(1000, task1));
        EXPECT_EQ(task1.status, TraceDumpStatus::READ_DONE);
        TraceDumpTask task2;
        EXPECT_TRUE(dumpPipe2->ReadTraceTask(1000, task2));
        EXPECT_EQ(task2.status, TraceDumpStatus::READ_DONE);
        _exit(0);
    }

    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    auto submitTask = [&dumpPipe]() {
        TraceDumpTask task = {
            .status = TraceDumpStatus::READ_DONE,
        };
        EXPECT_TRUE(dumpPipe->SubmitTraceDumpTask(task));
    };

    std::thread submitTaskThread(submitTask);
    std::thread submitTaskThread2(submitTask);
    submitTaskThread.join();
    submitTaskThread2.join();
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest001
 * @tc.desc: Test TraceDumpExecutor class task management functions
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest001, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();

    // Test IsTraceDumpTaskEmpty and GetTraceDumpTaskCount when empty
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 0);

    // Test AddTraceDumpTask
    TraceDumpTask task1 = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    traceDumpExecutor.AddTraceDumpTask(task1);
    EXPECT_FALSE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 1);

    // Test UpdateTraceDumpTask
    task1.status = TraceDumpStatus::READ_DONE;
    task1.code = TraceErrorCode::SUCCESS;
    EXPECT_TRUE(traceDumpExecutor.UpdateTraceDumpTask(task1));

    // Test RemoveTraceDumpTask
    traceDumpExecutor.RemoveTraceDumpTask(task1.time);
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 0);
}

/**
 * @tc.name: TraceDumpTaskTest002
 * @tc.desc: Test TraceDumpExecutor class multiple tasks management
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest002, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();

    // Add multiple tasks
    TraceDumpTask task1 = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    TraceDumpTask task2 = {
        .time = 2000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };

    traceDumpExecutor.AddTraceDumpTask(task1);
    traceDumpExecutor.AddTraceDumpTask(task2);
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 2);

    // Update first task
    task1.status = TraceDumpStatus::READ_DONE;
    EXPECT_TRUE(traceDumpExecutor.UpdateTraceDumpTask(task1));

    // Remove first task
    traceDumpExecutor.RemoveTraceDumpTask(task1.time);
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 1);

    // Remove second task
    traceDumpExecutor.RemoveTraceDumpTask(task2.time);
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
}

/**
 * @tc.name: TraceDumpTaskTest003
 * @tc.desc: Test TraceDumpExecutor class task update with invalid data
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest003, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();

    // Add a task
    TraceDumpTask task = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    traceDumpExecutor.AddTraceDumpTask(task);

    // Try to update non-existent task
    TraceDumpTask invalidTask = {
        .time = 9999,
        .status = TraceDumpStatus::READ_DONE,
        .code = TraceErrorCode::SUCCESS,
    };
    EXPECT_FALSE(traceDumpExecutor.UpdateTraceDumpTask(invalidTask));

    // Remove non-existent task
    traceDumpExecutor.RemoveTraceDumpTask(9999);
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 1);

    // Clean up
    traceDumpExecutor.RemoveTraceDumpTask(task.time);
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS