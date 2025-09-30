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

#include <atomic>
#include <gtest/gtest.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_dump.h"
#include "trace_source_factory.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
static const char* const TEST_TRACE_TEMP_FILE = "/data/local/tmp/test_trace_file";
}

class HitraceFactoryTest : public testing::Test {
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
 * @tc.name: TraceSourceTest001
 * @tc.desc: Test TraceSourceLinuxFactory class GetTraceFileHeader function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest001, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceFileHdr = traceSourceFactory->GetTraceFileHeader();
    ASSERT_TRUE(traceFileHdr != nullptr);
    ASSERT_TRUE(traceFileHdr->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest002
 * @tc.desc: Test TraceSourceHMFactory class GetTraceFileHeader function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest002, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceFileHdr = traceSourceFactory->GetTraceFileHeader();
    ASSERT_TRUE(traceFileHdr != nullptr);
    ASSERT_TRUE(traceFileHdr->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest003
 * @tc.desc: Test TraceSourceLinuxFactory class GetTraceHeaderPage function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest003, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceHdrPage = traceSourceFactory->GetTraceHeaderPage();
    ASSERT_TRUE(traceHdrPage != nullptr);
    if (IsHmKernel()) {
        ASSERT_FALSE(traceHdrPage->WriteTraceContent());
    } else {
        ASSERT_TRUE(traceHdrPage->WriteTraceContent());
        ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    }
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest004
 * @tc.desc: Test TraceSourceHMFactory class GetTraceHeaderPage function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest004, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceHdrPage = traceSourceFactory->GetTraceHeaderPage();
    ASSERT_TRUE(traceHdrPage != nullptr);
    ASSERT_TRUE(traceHdrPage->WriteTraceContent());
    if (IsHmKernel()) {
        ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    }
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest005
 * @tc.desc: Test TraceSourceLinuxFactory class GetTracePrintkFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest005, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto tracePrintkFmt = traceSourceFactory->GetTracePrintkFmt();
    ASSERT_TRUE(tracePrintkFmt != nullptr);
    ASSERT_TRUE(tracePrintkFmt->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest006
 * @tc.desc: Test TraceSourceHMFactory class GetTracePrintkFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest006, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto tracePrintkFmt = traceSourceFactory->GetTracePrintkFmt();
    ASSERT_TRUE(tracePrintkFmt != nullptr);
    ASSERT_TRUE(tracePrintkFmt->WriteTraceContent());
    if (IsHmKernel()) {
        ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    }
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest007
 * @tc.desc: Test TraceSourceLinuxFactory class GetTraceEventFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest007, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceEventFmts = traceSourceFactory->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE),
        GetFileSize("/data/log/hitrace/saved_events_format") + sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest008
 * @tc.desc: Test TraceSourceHMFactory class GetTraceEventFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest008, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceEventFmts = traceSourceFactory->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE),
        GetFileSize("/data/log/hitrace/saved_events_format") + sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest009
 * @tc.desc: Test TraceSourceHMFactory class GetTraceEventFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest009, TestSize.Level2)
{
    if (access("/data/log/hitrace/saved_events_format", F_OK) == 0) {
        ASSERT_EQ(remove("/data/log/hitrace/saved_events_format"), 0);
        GTEST_LOG_(INFO) << "Delete saved_events_format file.";
    }
    ASSERT_FALSE(access("/data/log/hitrace/saved_events_format", F_OK) == 0);
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceEventFmts = traceSourceFactory->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE),
        GetFileSize("/data/log/hitrace/saved_events_format") + sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest010
 * @tc.desc: Test TraceSourceLinuxFactory class GetTraceCmdLines function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest010, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceCmdLines = traceSourceFactory->GetTraceCmdLines();
    ASSERT_TRUE(traceCmdLines != nullptr);
    ASSERT_TRUE(traceCmdLines->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest011
 * @tc.desc: Test TraceSourceHMFactory class GetTraceCmdLines function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest011, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceCmdLines = traceSourceFactory->GetTraceCmdLines();
    ASSERT_TRUE(traceCmdLines != nullptr);
    ASSERT_TRUE(traceCmdLines->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest012
 * @tc.desc: Test TraceSourceLinuxFactory class GetTraceTgids function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest012, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceTgids = traceSourceFactory->GetTraceTgids();
    ASSERT_TRUE(traceTgids != nullptr);
    ASSERT_TRUE(traceTgids->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest013
 * @tc.desc: Test TraceSourceHMFactory class GetTraceTgids function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest013, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceTgids = traceSourceFactory->GetTraceTgids();
    ASSERT_TRUE(traceTgids != nullptr);
    ASSERT_TRUE(traceTgids->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest014
 * @tc.desc: Test ITraceSourceFactory class GetTraceCpuRaw function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest014, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    }
    ASSERT_TRUE(traceSourceFactory != nullptr);
    TraceDumpRequest request = {
        .type = TraceDumpType::TRACE_RECORDING,
        .fileSize = 102400,
    };
    auto traceCpuRaw = traceSourceFactory->GetTraceCpuRaw(request);
    ASSERT_TRUE(traceCpuRaw != nullptr);
    ASSERT_TRUE(traceCpuRaw->WriteTraceContent());
    ASSERT_EQ(traceCpuRaw->GetDumpStatus(), TraceErrorCode::SUCCESS);
    ASSERT_LT(traceCpuRaw->GetFirstPageTimeStamp(), std::numeric_limits<uint64_t>::max());
    ASSERT_GT(traceCpuRaw->GetLastPageTimeStamp(), 0);
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest015
 * @tc.desc: Test ITraceSourceFactory class e2e features.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest015, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    }
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto traceFileHdr = traceSourceFactory->GetTraceFileHeader();
    ASSERT_TRUE(traceFileHdr != nullptr);
    ASSERT_TRUE(traceFileHdr->WriteTraceContent());
    auto traceEventFmts = traceSourceFactory->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    TraceDumpRequest request = {
        .type = TraceDumpType::TRACE_RECORDING,
        .fileSize = 102400,
    };
    auto traceCpuRaw = traceSourceFactory->GetTraceCpuRaw(request);
    ASSERT_TRUE(traceCpuRaw != nullptr);
    ASSERT_TRUE(traceCpuRaw->WriteTraceContent());
    auto traceCmdLines = traceSourceFactory->GetTraceCmdLines();
    ASSERT_TRUE(traceCmdLines != nullptr);
    ASSERT_TRUE(traceCmdLines->WriteTraceContent());
    auto traceTgids = traceSourceFactory->GetTraceTgids();
    ASSERT_TRUE(traceTgids != nullptr);
    ASSERT_TRUE(traceTgids->WriteTraceContent());
    auto traceHdrPage = traceSourceFactory->GetTraceHeaderPage();
    ASSERT_TRUE(traceHdrPage != nullptr);
    ASSERT_TRUE(traceHdrPage->WriteTraceContent());
    auto tracePrintkFmt = traceSourceFactory->GetTracePrintkFmt();
    ASSERT_TRUE(tracePrintkFmt != nullptr);
    ASSERT_TRUE(tracePrintkFmt->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest016
 * @tc.desc: Test ITraceSourceFactory class UpdateTraceFile/GetTraceFilePath functions.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest016, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    ASSERT_EQ(traceSourceFactory->GetTraceFilePath(), TEST_TRACE_TEMP_FILE);
    const std::string newTestFile = "/data/local/tmp/new_test_file";
    ASSERT_TRUE(traceSourceFactory->UpdateTraceFile(newTestFile));
    ASSERT_EQ(traceSourceFactory->GetTraceFilePath(), newTestFile);
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
    if (remove(newTestFile.c_str()) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest017
 * @tc.desc: Test TraceSourceLinuxFactory class GetTraceBaseInfo function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest017, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto baseInfo = traceSourceFactory->GetTraceBaseInfo();
    ASSERT_TRUE(baseInfo != nullptr);
    ASSERT_TRUE(baseInfo->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest018
 * @tc.desc: Test TraceSourceHMFactory class GetTraceBaseInfo function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest018, TestSize.Level2)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory =
        std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSourceFactory != nullptr);
    auto baseInfo = traceSourceFactory->GetTraceBaseInfo();
    ASSERT_TRUE(baseInfo != nullptr);
    ASSERT_TRUE(baseInfo->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileContentHeader));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest019
 * @tc.desc: Test TraceSourceLinuxFactory/TraceSourceHMFactory class GetTraceCpuRawRead function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest019, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    EXPECT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    sleep(1);
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, "");
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, "");
    }
    EXPECT_TRUE(traceSourceFactory != nullptr);
    TraceDumpRequest request = { .taskId = 1 };
    auto traceCpuRawRead = traceSourceFactory->GetTraceCpuRawRead(request);
    EXPECT_TRUE(traceCpuRawRead != nullptr);
    EXPECT_TRUE(traceCpuRawRead->WriteTraceContent());
    EXPECT_GT(TraceBufferManager::GetInstance().GetTaskTotalUsedBytes(1), 0);
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(1);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceSourceTest020
 * @tc.desc: Test TraceSourceHMFactory class GetTraceCpuRawWrite function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceSourceTest020, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    EXPECT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    sleep(1);
    std::shared_ptr<ITraceSourceFactory> traceSourceRead = nullptr;
    std::shared_ptr<ITraceSourceFactory> traceSourceWrite = nullptr;
    if (IsHmKernel()) {
        traceSourceRead = std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, "");
        traceSourceWrite = std::make_shared<TraceSourceHMFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    } else {
        traceSourceRead = std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, "");
        traceSourceWrite = std::make_shared<TraceSourceLinuxFactory>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    }
    EXPECT_TRUE(traceSourceRead != nullptr);
    EXPECT_TRUE(traceSourceWrite != nullptr);
    TraceDumpRequest request = { TraceDumpType::TRACE_SNAPSHOT, 0, false, 0, std::numeric_limits<uint64_t>::max(), 1 };
    auto traceCpuRawRead = traceSourceRead->GetTraceCpuRawRead(request);
    EXPECT_TRUE(traceCpuRawRead != nullptr);
    EXPECT_TRUE(traceCpuRawRead->WriteTraceContent());
    EXPECT_GT(TraceBufferManager::GetInstance().GetTaskTotalUsedBytes(1), 0);
    auto traceCpuRawWrite = traceSourceWrite->GetTraceCpuRawWrite(1);
    EXPECT_TRUE(traceCpuRawWrite != nullptr);
    EXPECT_TRUE(traceCpuRawWrite->WriteTraceContent());
    EXPECT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    EXPECT_EQ(traceCpuRawRead->GetDumpStatus(), TraceErrorCode::SUCCESS);
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(1);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceBufferManagerTest01
 * @tc.desc: Test TraceBufferManager class AllocateBlock/GetTaskBuffers/GetCurrentTotalSize function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceBufferManagerTest01, TestSize.Level2)
{
    const uint64_t taskId = 1;
    TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
    EXPECT_EQ(TraceBufferManager::GetInstance().GetCurrentTotalSize(), DEFAULT_BLOCK_SZ);
    TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
    EXPECT_EQ(TraceBufferManager::GetInstance().GetCurrentTotalSize(), DEFAULT_BLOCK_SZ * 2); // 2 : 2 blocks
    auto buffers = TraceBufferManager::GetInstance().GetTaskBuffers(1);
    EXPECT_EQ(buffers.size(), 2);
    for (auto& buf : buffers) {
        EXPECT_NE(buf->data.data(), nullptr);
        EXPECT_EQ(buf->usedBytes, 0);
    }
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId);
}

/**
 * @tc.name: TraceBufferManagerTest02
 * @tc.desc: Test TraceBufferManager class with multiple tasks and buffer sizes.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceBufferManagerTest02, TestSize.Level2)
{
    // 测试多个任务的内存分配
    const uint64_t taskId1 = 1;
    const uint64_t taskId2 = 2;

    // 为不同任务分配不同大小的缓冲区
    TraceBufferManager::GetInstance().AllocateBlock(taskId1, 0);
    TraceBufferManager::GetInstance().AllocateBlock(taskId2, 0);

    // 验证任务1的缓冲区
    auto buffers1 = TraceBufferManager::GetInstance().GetTaskBuffers(taskId1);
    EXPECT_EQ(buffers1.size(), 1);
    EXPECT_NE(buffers1.front()->data.data(), nullptr);
    EXPECT_EQ(buffers1.front()->usedBytes, 0);

    // 验证任务2的缓冲区
    auto buffers2 = TraceBufferManager::GetInstance().GetTaskBuffers(taskId2);
    EXPECT_EQ(buffers2.size(), 1);
    EXPECT_NE(buffers2.front()->data.data(), nullptr);
    EXPECT_EQ(buffers2.front()->usedBytes, 0);

    // 验证总大小
    size_t totalSize = TraceBufferManager::GetInstance().GetCurrentTotalSize();
    EXPECT_EQ(totalSize, DEFAULT_BLOCK_SZ * 2); // 2 : 2 blocks

    // 清理测试数据
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId1);
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId2);
}

/**
 * @tc.name: TraceBufferManagerTest03
 * @tc.desc: Test TraceBufferManager class with buffer size limits and overflow.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceBufferManagerTest03, TestSize.Level2)
{
    const uint64_t taskId = 1;

    // 测试正常大小分配
    TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
    auto buffers = TraceBufferManager::GetInstance().GetTaskBuffers(taskId);
    EXPECT_EQ(buffers.size(), 1);
    EXPECT_NE(buffers.front()->data.data(), nullptr);

    // 测试溢出大小分配
    for (int i = 0; i < 50; i++) { // 50 : try to allocate 50 blocks , 500MB
        TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
    }
    buffers = TraceBufferManager::GetInstance().GetTaskBuffers(taskId);
    EXPECT_EQ(buffers.size(), DEFAULT_MAX_TOTAL_SZ / DEFAULT_BLOCK_SZ);

    // 验证总大小限制
    size_t totalSize = TraceBufferManager::GetInstance().GetCurrentTotalSize();
    EXPECT_LE(totalSize, DEFAULT_MAX_TOTAL_SZ);  // 总大小不应超过最大限制的两倍

    // 清理测试数据
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId);
}

/**
 * @tc.name: TraceBufferManagerTest04
 * @tc.desc: Test TraceBufferManager class with task buffer reuse and cleanup.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceBufferManagerTest04, TestSize.Level2)
{
    const uint64_t taskId = 1;

    // 第一次分配
    TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
    auto buffers1 = TraceBufferManager::GetInstance().GetTaskBuffers(taskId);
    EXPECT_EQ(buffers1.size(), 1);

    // 清理缓冲区
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId);
    auto buffers2 = TraceBufferManager::GetInstance().GetTaskBuffers(taskId);
    EXPECT_EQ(buffers2.size(), 0);

    // 重新分配
    TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
    auto buffers3 = TraceBufferManager::GetInstance().GetTaskBuffers(taskId);
    EXPECT_EQ(buffers3.size(), 1);
    EXPECT_NE(buffers3.front()->data.data(), nullptr);
    EXPECT_EQ(buffers3.front()->usedBytes, 0);

    // 验证总大小
    size_t totalSize = TraceBufferManager::GetInstance().GetCurrentTotalSize();
    EXPECT_EQ(totalSize, DEFAULT_BLOCK_SZ);

    // 清理测试数据
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId);
}

/**
 * @tc.name: TraceBufferManagerTest05
 * @tc.desc: Test TraceBufferManager class with multi-thread buffer allocation.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceFactoryTest, TraceBufferManagerTest05, TestSize.Level2)
{
    const int threadCount = 10;
    const int allocPerThread = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);
    for (int i = 0; i < threadCount; i++) {
        threads.emplace_back([i, &successCount]() {
            const uint64_t taskId = i + 1;
            int allocCount = 0;
            for (int j = 0; j < allocPerThread; j++) {
                auto block = TraceBufferManager::GetInstance().AllocateBlock(taskId, 0);
                allocCount += (block != nullptr) ? 1 : 0;
            }
            auto buffers = TraceBufferManager::GetInstance().GetTaskBuffers(taskId);
            EXPECT_EQ(buffers.size(), allocCount);
            successCount += allocCount;
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    size_t totalSize = TraceBufferManager::GetInstance().GetCurrentTotalSize();
    EXPECT_LE(totalSize, DEFAULT_MAX_TOTAL_SZ);
    EXPECT_LE(successCount.load() * DEFAULT_BLOCK_SZ, DEFAULT_MAX_TOTAL_SZ);
    for (int i = 0; i < threadCount; i++) {
        TraceBufferManager::GetInstance().ReleaseTaskBlocks(i + 1);
    }
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namesapce OHOS