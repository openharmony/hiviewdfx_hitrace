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
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_dump.h"
#include "trace_source.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
static const char* const TEST_TRACE_TEMP_FILE = "/data/local/tmp/test_trace_file";
}

class HitraceSourceTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown()
    {
        if (remove(TEST_TRACE_TEMP_FILE) != 0) {
            GTEST_LOG_(ERROR) << "Delete test trace file failed.";
        }
    }
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
 * @tc.desc: Test TraceSourceLinux class GetTraceFileHeader function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest001, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceFileHdr = traceSource->GetTraceFileHeader();
    ASSERT_TRUE(traceFileHdr != nullptr);
    ASSERT_TRUE(traceFileHdr->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileHeader));
}

/**
 * @tc.name: TraceSourceTest002
 * @tc.desc: Test TraceSourceHM class GetTraceFileHeader function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest002, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceFileHdr = traceSource->GetTraceFileHeader();
    ASSERT_TRUE(traceFileHdr != nullptr);
    ASSERT_TRUE(traceFileHdr->WriteTraceContent());
    ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), sizeof(TraceFileHeader));
}

/**
 * @tc.name: TraceSourceTest003
 * @tc.desc: Test TraceSourceLinux class GetTraceHeaderPage function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest003, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceHdrPage = traceSource->GetTraceHeaderPage();
    ASSERT_TRUE(traceHdrPage != nullptr);
    if (IsHmKernel()) {
        ASSERT_FALSE(traceHdrPage->WriteTraceContent());
    } else {
        ASSERT_TRUE(traceHdrPage->WriteTraceContent());
        ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    }
}

/**
 * @tc.name: TraceSourceTest004
 * @tc.desc: Test TraceSourceHM class GetTraceHeaderPage function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest004, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceHdrPage = traceSource->GetTraceHeaderPage();
    ASSERT_TRUE(traceHdrPage != nullptr);
    ASSERT_TRUE(traceHdrPage->WriteTraceContent());
    if (IsHmKernel()) {
        ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    }
}

/**
 * @tc.name: TraceSourceTest005
 * @tc.desc: Test TraceSourceLinux class GetTracePrintkFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest005, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto tracePrintkFmt = traceSource->GetTracePrintkFmt();
    ASSERT_TRUE(tracePrintkFmt != nullptr);
    ASSERT_TRUE(tracePrintkFmt->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest006
 * @tc.desc: Test TraceSourceHM class GetTracePrintkFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest006, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto tracePrintkFmt = traceSource->GetTracePrintkFmt();
    ASSERT_TRUE(tracePrintkFmt != nullptr);
    ASSERT_TRUE(tracePrintkFmt->WriteTraceContent());
    if (IsHmKernel()) {
        ASSERT_EQ(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    }
}

/**
 * @tc.name: TraceSourceTest007
 * @tc.desc: Test TraceSourceLinux class GetTraceEventFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest007, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceEventFmts = traceSource->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    if (access("/data/log/hitrace/saved_events_format", F_OK) == 0) {
        ASSERT_EQ(remove("/data/log/hitrace/saved_events_format"), 0);
    }
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest008
 * @tc.desc: Test TraceSourceHM class GetTraceEventFmt function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest008, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceEventFmts = traceSource->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    if (access("/data/log/hitrace/saved_events_format", F_OK) == 0) {
        ASSERT_EQ(remove("/data/log/hitrace/saved_events_format"), 0);
    }
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest009
 * @tc.desc: Test TraceSourceLinux class GetTraceCmdLines function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest009, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceCmdLines = traceSource->GetTraceCmdLines();
    ASSERT_TRUE(traceCmdLines != nullptr);
    ASSERT_TRUE(traceCmdLines->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest010
 * @tc.desc: Test TraceSourceHM class GetTraceCmdLines function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest010, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceCmdLines = traceSource->GetTraceCmdLines();
    ASSERT_TRUE(traceCmdLines != nullptr);
    ASSERT_TRUE(traceCmdLines->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest011
 * @tc.desc: Test TraceSourceLinux class GetTraceTgids function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest011, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceTgids = traceSource->GetTraceTgids();
    ASSERT_TRUE(traceTgids != nullptr);
    ASSERT_TRUE(traceTgids->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest012
 * @tc.desc: Test TraceSourceHM class GetTraceTgids function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest012, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto traceTgids = traceSource->GetTraceTgids();
    ASSERT_TRUE(traceTgids != nullptr);
    ASSERT_TRUE(traceTgids->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest013
 * @tc.desc: Test ITraceSource class GetTraceCpuRaw function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest013, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    }
    ASSERT_TRUE(traceSource != nullptr);
    TraceDumpRequest request = {
        TRACE_TYPE::TRACE_RECORDING,
        1024000, // 102400 : set 100MB /sys/kernel/tracing/buffer_size_kb.
        false,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto traceCpuRaw = traceSource->GetTraceCpuRaw(request);
    ASSERT_TRUE(traceCpuRaw != nullptr);
    ASSERT_TRUE(traceCpuRaw->WriteTraceContent());
    ASSERT_EQ(traceCpuRaw->GetDumpStatus(), TraceErrorCode::SUCCESS);
    ASSERT_LT(traceCpuRaw->GetFirstPageTimeStamp(), std::numeric_limits<uint64_t>::max());
    ASSERT_GT(traceCpuRaw->GetLastPageTimeStamp(), 0);
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceSourceTest014
 * @tc.desc: Test ITraceSource class e2e features.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest014, TestSize.Level2)
{
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(OpenTrace(appArgs), TraceErrorCode::SUCCESS);
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    }
    ASSERT_TRUE(traceSource != nullptr);
    auto traceFileHdr = traceSource->GetTraceFileHeader();
    ASSERT_TRUE(traceFileHdr != nullptr);
    ASSERT_TRUE(traceFileHdr->WriteTraceContent());
    auto traceEventFmts = traceSource->GetTraceEventFmt();
    ASSERT_TRUE(traceEventFmts != nullptr);
    ASSERT_TRUE(traceEventFmts->WriteTraceContent());
    TraceDumpRequest request = {
        TRACE_TYPE::TRACE_RECORDING,
        1024000,
        false,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    auto traceCpuRaw = traceSource->GetTraceCpuRaw(request);
    ASSERT_TRUE(traceCpuRaw != nullptr);
    ASSERT_TRUE(traceCpuRaw->WriteTraceContent());
    auto traceCmdLines = traceSource->GetTraceCmdLines();
    ASSERT_TRUE(traceCmdLines != nullptr);
    ASSERT_TRUE(traceCmdLines->WriteTraceContent());
    auto traceTgids = traceSource->GetTraceTgids();
    ASSERT_TRUE(traceTgids != nullptr);
    ASSERT_TRUE(traceTgids->WriteTraceContent());
    auto traceHdrPage = traceSource->GetTraceHeaderPage();
    ASSERT_TRUE(traceHdrPage != nullptr);
    ASSERT_TRUE(traceHdrPage->WriteTraceContent());
    auto tracePrintkFmt = traceSource->GetTracePrintkFmt();
    ASSERT_TRUE(tracePrintkFmt != nullptr);
    ASSERT_TRUE(tracePrintkFmt->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    ASSERT_EQ(CloseTrace(), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceSourceTest015
 * @tc.desc: Test ITraceSource class UpdateTraceFile/GetTraceFilePath functions.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest015, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    ASSERT_EQ(traceSource->GetTraceFilePath(), TEST_TRACE_TEMP_FILE);
    const std::string newTestFile = "/data/local/tmp/new_test_file";
    ASSERT_TRUE(traceSource->UpdateTraceFile(newTestFile));
    ASSERT_EQ(traceSource->GetTraceFilePath(), newTestFile);
    if (remove(newTestFile.c_str()) != 0) {
        GTEST_LOG_(ERROR) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceSourceTest016
 * @tc.desc: Test TraceSourceLinux class GetTraceBaseInfo function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest016, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto baseInfo = traceSource->GetTraceBaseInfo();
    ASSERT_TRUE(baseInfo != nullptr);
    ASSERT_TRUE(baseInfo->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}

/**
 * @tc.name: TraceSourceTest017
 * @tc.desc: Test TraceSourceHM class GetTraceBaseInfo function.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSourceTest, TraceSourceTest017, TestSize.Level2)
{
    std::shared_ptr<ITraceSource> traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, TEST_TRACE_TEMP_FILE);
    ASSERT_TRUE(traceSource != nullptr);
    auto baseInfo = traceSource->GetTraceBaseInfo();
    ASSERT_TRUE(baseInfo != nullptr);
    ASSERT_TRUE(baseInfo->WriteTraceContent());
    ASSERT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namesapce OHOS