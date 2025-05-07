/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
#include "hitrace_dump.h"

#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "common_define.h"
#include "common_utils.h"
#include "test_utils.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
using namespace testing::ext;
namespace {
const int TIME_COUNT = 10000;
const int CMD_OUTPUT_BUF = 1024;
const int BYTE_PER_MB = 1024 * 1024;
constexpr uint64_t S_TO_MS = 1000;
const std::string TRACE_SNAPSHOT_PREFIX = "trace_";
const std::string TRACE_RECORDING_PREFIX = "record_trace_";
const std::string TRACE_CACHE_PREFIX = "cache_trace_";
struct FileWithInfo {
    std::string filename;
    time_t ctime;
    uint64_t fileSize;
    uint64_t duration;
};
enum TRACE_TYPE : uint8_t {
    TRACE_SNAPSHOT = 0,
    TRACE_RECORDING = 1,
    TRACE_CACHE = 2,
};
std::map<TRACE_TYPE, std::string> tracePrefixMap = {
    {TRACE_SNAPSHOT, TRACE_SNAPSHOT_PREFIX},
    {TRACE_RECORDING, TRACE_RECORDING_PREFIX},
    {TRACE_CACHE, TRACE_CACHE_PREFIX},
};
}
class HitraceSystemTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void)
    {
        ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
        ASSERT_TRUE(RunCmd("hitrace --trace_finish --record"));
        ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
    }

    void SetUp()
    {
        ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
        ASSERT_TRUE(RunCmd("hitrace --trace_finish --record"));
        ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
    }
    void TearDown() {}

    static bool RunCmd(const string& cmdstr)
    {
        if (cmdstr.empty()) {
            return false;
        }
        FILE *fp = popen(cmdstr.c_str(), "r");
        if (fp == nullptr) {
            return false;
        }
        char res[CMD_OUTPUT_BUF] = { '\0' };
        while (fgets(res, sizeof(res), fp) != nullptr) {
            std::cout << res;
        }
        pclose(fp);
        return true;
    }
};

namespace {
bool CheckTraceCommandOutput(const std::string& cmd, const std::vector<std::string>& keywords,
    std::vector<std::string>& traceLists)
{
    if (cmd.empty()) {
        return false;
    }
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }

    char buffer[CMD_OUTPUT_BUF];
    int checkIdx = 0;
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        while (checkIdx < keywords.size() && strstr(buffer, keywords[checkIdx].c_str()) != nullptr) {
            GTEST_LOG_(INFO) << "match keyword :" << keywords[checkIdx];
            checkIdx++;
            if (checkIdx == keywords.size()) {
                break;
            }
        }
        char* tracefile = strstr(buffer, TRACE_FILE_DEFAULT_DIR.c_str());
        tracefile += TRACE_FILE_DEFAULT_DIR.length();
        if (tracefile != nullptr) {
            tracefile[strcspn(tracefile, "\n")] = '\0'; // replace "\n" with "\0"
            GTEST_LOG_(INFO) << "match trace file : " << tracefile;
            traceLists.push_back(std::string(tracefile));
        }
    }

    pclose(fp);
    if (checkIdx < keywords.size()) {
        GTEST_LOG_(ERROR) << "Failed to match keyword : " << keywords[checkIdx];
    }
    return checkIdx == keywords.size();
}

bool IsTracingOn()
{
    std::ifstream tracingOnFile(TRACEFS_DIR + TRACING_ON_NODE);
    if (!tracingOnFile.is_open()) {
        std::cout << "Failed to open /sys/kernel/tracing/tracing_on." << std::endl;
        return false;
    }
    std::string content;
    getline(tracingOnFile, content);
    tracingOnFile.close();
    return content == "1";
}

bool IsFileIncludeAllKeyWords(const string& fileName, const std::vector<std::string>& keywords)
{
    std::ifstream readFile;
    readFile.open(fileName.c_str(), std::ios::in);
    if (readFile.fail()) {
        GTEST_LOG_(ERROR) << "Failed to open file " << fileName;
        return false;
    }
    int keywordIdx = 0;
    std::string readLine;
    while (getline(readFile, readLine, '\n') && keywordIdx < keywords.size()) {
        if (readLine.find(keywords[keywordIdx]) == std::string::npos) {
            continue;
        }
        keywordIdx++;
    }
    readFile.close();
    if (keywordIdx < keywords.size()) {
        GTEST_LOG_(ERROR) << "Failed to find keyword: " << keywords[keywordIdx];
    }
    return keywordIdx == keywords.size();
}

bool IsFileExcludeAllKeyWords(const string& fileName, const std::vector<std::string>& keywords)
{
    std::ifstream readFile;
    readFile.open(fileName.c_str(), std::ios::in);
    if (readFile.fail()) {
        GTEST_LOG_(ERROR) << "Failed to open file " << fileName;
        return false;
    }
    std::string readLine;
    while (getline(readFile, readLine, '\n')) {
        for (auto& word : keywords) {
            if (readLine.find(word) != std::string::npos) {
                GTEST_LOG_(ERROR) << "File contained keyword: " << word;
                readFile.close();
                return false;
            }
        }
    }
    readFile.close();
    return true;
}

std::string ReadBufferSizeKB()
{
    std::ifstream file(TRACEFS_DIR + "buffer_size_kb");
    if (!file.is_open()) {
        GTEST_LOG_(ERROR) << "Failed to open buffer_size_kb";
        return "Unknown";
    }
    std::string line;
    if (std::getline(file, line)) {
        GTEST_LOG_(INFO) << "Reading buffer_size_kb: " << line;
        return line;
    }
    return "Unknown";
}

bool GetDurationFromFileName(const std::string& fileName, uint64_t& duration)
{
    auto index = fileName.find("-");
    if (index == std::string::npos) {
        return false;
    }
    uint32_t number;
    if (sscanf_s(fileName.substr(index, fileName.size() - index).c_str(), "-%u.sys", &number) != 1) {
        GTEST_LOG_(INFO) << "sscanf_s failed.";
        return false;
    }
    duration = static_cast<uint64_t>(number);
    return true;
}

bool GetFileInfo(const TRACE_TYPE& traceType, const std::vector<std::string>& outputFiles,
    std::vector<FileWithInfo>& fileList)
{
    struct stat fileStat;
    for (auto i = 0; i < outputFiles.size(); i++) {
        if (outputFiles[i].substr(TRACE_FILE_DEFAULT_DIR.size(), tracePrefixMap[traceType].size()) ==
            tracePrefixMap[traceType]) {
            uint64_t duration = 0;
            if (GetDurationFromFileName(outputFiles[i].substr(TRACE_FILE_DEFAULT_DIR.size(),
                outputFiles[i].size() - TRACE_FILE_DEFAULT_DIR.size()), duration)) {
                if (stat(outputFiles[i].c_str(), &fileStat) == 0) {
                    fileList.push_back({outputFiles[i], fileStat.st_ctime, static_cast<uint64_t>(fileStat.st_size),
                        duration});
                } else {
                    GTEST_LOG_(INFO) << "stat file failed, file is " << outputFiles[i].c_str();
                    return false;
                }
            } else {
                GTEST_LOG_(INFO) << "GetDurationFromFileName failed, file is " << outputFiles[i].c_str();
                return false;
            }
        }
    }
    std::sort(fileList.begin(), fileList.end(), [](const FileWithInfo& a, const FileWithInfo& b) {
        return a.ctime < b.ctime;
    });
    return true;
}

std::vector<std::string> GetTraceFilesInDir(const TRACE_TYPE& traceType)
{
    std::vector<std::string> fileVec;
    for (const auto &entry : std::filesystem::directory_iterator(TRACE_FILE_DEFAULT_DIR)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        if (fileName.substr(0, tracePrefixMap[traceType].size()) == tracePrefixMap[traceType]) {
            fileVec.push_back(TRACE_FILE_DEFAULT_DIR + fileName);
        }
    }
    return fileVec;
}

/**
 * @tc.name: HitraceSystemTest001
 * @tc.desc: when excute hitrace record command, check tracing_on switcher status
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTest001, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --trace_begin ace"));
    int testCnt = TIME_COUNT;
    while (testCnt > 0) {
        usleep(10);
        ASSERT_TRUE(IsTracingOn()) << "tracing_on switcher status is not 1";
        testCnt--;
    }
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
}

/**
 * @tc.name: HitraceSystemTest003
 * @tc.desc: when excute hitrace record command, check record file aging rules.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTest003, TestSize.Level2)
{
    if (IsRootVersion()) {
        const int recordCnt = 20;
        for (int i = 0; i < recordCnt; ++i) {
            ASSERT_TRUE(RunCmd("hitrace --trace_begin --record sched"));
            ASSERT_TRUE(RunCmd("hitrace --trace_finish --record"));
        }
        int filecnt = CountRecordingTraceFile();
        GTEST_LOG_(INFO) << "Filecnt: " << filecnt;
        ASSERT_GE(filecnt, recordCnt);
    }
}

/**
 * @tc.name: SnapShotModeTest001
 * @tc.desc: test open snapshot mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest001, TestSize.Level1)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --start_bgsrv", {"SNAPSHOT_START", "OpenSnapshot done"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: SnapShotModeTest002
 * @tc.desc: test open snapshot mode duplicately
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest002, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --start_bgsrv",
        {"SNAPSHOT_START", "OpenSnapshot failed", "errorCode(1013)"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: SnapShotModeTest003
 * @tc.desc: test close snapshot mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest003, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --stop_bgsrv", {"SNAPSHOT_STOP", "CloseSnapshot done"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: SnapShotModeTest004
 * @tc.desc: test close snapshot mode duplicately
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest004, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --stop_bgsrv", {"SNAPSHOT_STOP", "CloseSnapshot done"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: SnapShotModeTest005
 * @tc.desc: test dump snapshot trace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest005, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists));
    ASSERT_FALSE(traceLists.empty());
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
}

/**
 * @tc.name: SnapShotModeTest006
 * @tc.desc: test dump snapshot trace when snapshot mode not open
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest006, TestSize.Level1)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv",
        {"SNAPSHOT_DUMP", "DumpSnapshot failed", "errorCode(1012)"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: SnapShotModeTest007
 * @tc.desc: test dump snapshot trace twice within 30s offset
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest007, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    sleep(30);
    std::vector<std::string> traceLists1 = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists1));
    ASSERT_FALSE(traceLists1.empty());
    sleep(10); // 10 : sleep 10 seconds
    std::vector<std::string> traceLists2 = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists2));
    ASSERT_FALSE(traceLists2.empty());
    for (const auto& tracefile : traceLists1) {
        ASSERT_NE(std::find(traceLists2.begin(), traceLists2.end(), tracefile), traceLists2.end());
    }
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
}

/**
 * @tc.name: SnapShotModeTest008
 * @tc.desc: test dump snapshot trace twice with 30s offset
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest008, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    std::vector<std::string> traceLists1 = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists1));
    ASSERT_FALSE(traceLists1.empty());
    sleep(31); // 31 : sleep 32 seconds, 1s is tolorance
    std::vector<std::string> traceLists2 = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists2));
    ASSERT_FALSE(traceLists2.empty());
    for (const auto& tracefile : traceLists1) {
        ASSERT_EQ(std::find(traceLists2.begin(), traceLists2.end(), tracefile), traceLists2.end());
    }
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
}

/**
 * @tc.name: SnapShotModeTest009
 * @tc.desc: test dump snapshot trace with 20 files aging
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest009, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    const int snapshotFileAge = 20;
    for (int i = 0; i < snapshotFileAge; ++i) {
        ASSERT_TRUE(RunCmd("hitrace --dump_bgsrv"));
        sleep(1); // wait 1s
    }
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists));
    ASSERT_GE(traceLists.size(), snapshotFileAge + 1);
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
    int filecnt = CountSnapShotTraceFile();
    GTEST_LOG_(INFO) << "Filecnt: " << filecnt;
    ASSERT_LE(filecnt, snapshotFileAge + 1);
}

/**
 * @tc.name: SnapShotModeTest010
 * @tc.desc: test dump snapshot trace with 20 files aging
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest010, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv"));
    const int dumpCnt = 30; // 30 : dump 30 times
    for (int i = 0; i < dumpCnt; ++i) {
        ASSERT_TRUE(RunCmd("hitrace --dump_bgsrv"));
        sleep(1); // wait 1s
    }
    const int snapshotFileAge = 20;
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --dump_bgsrv", {"SNAPSHOT_DUMP", "DumpSnapshot done"}, traceLists));
    ASSERT_GE(traceLists.size(), snapshotFileAge + 1);
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv"));
    std::vector<std::string> dirTraceLists = {};
    GetSnapShotTraceFileList(dirTraceLists);
    ASSERT_LE(dirTraceLists.size(), snapshotFileAge + 1);
    for (int i = 0; i < dirTraceLists.size(); ++i) {
        ASSERT_NE(std::find(traceLists.begin(), traceLists.end(), dirTraceLists[i]), traceLists.end()) <<
            "not found: " << dirTraceLists[i];
    }
}

/**
 * @tc.name: SnapShotModeTest011
 * @tc.desc: test open snapshot mode can not be opened when reocording mode was opened.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, SnapShotModeTest011, TestSize.Level1)
{
    ASSERT_TRUE(RunCmd("hitrace --trace_begin --record ohos"));
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --start_bgsrv",
        {"SNAPSHOT_START", "OpenSnapshot failed", "errorCode(1013)"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(RunCmd("hitrace --trace_finish --record"));
}

/**
 * @tc.name: CacheModeTest001
 * @tc.desc: test dumptrace when cache trace was opened.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, CacheModeTest001, TestSize.Level1)
{
    const std::vector<std::string> tagGroups = {"default"};
    ASSERT_TRUE(OpenTrace(tagGroups) == TraceErrorCode::SUCCESS);
    // total cache filesize limit: 800MB, sliceduration: 20s
    ASSERT_TRUE(CacheTraceOn(800, 5) == TraceErrorCode::SUCCESS);
    sleep(8); // wait 8s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    std::vector<FileWithInfo> fileList;
    ASSERT_TRUE(GetFileInfo(TRACE_SNAPSHOT, ret.outputFiles, fileList));
    ASSERT_GE(fileList.size(), 2); // cache_trace_ file count > 2
    uint64_t totalDuartion = 0;
    for (auto i = 0; i < fileList.size(); i++) {
        GTEST_LOG_(INFO) << "file: " << fileList[i].filename.c_str() << ", size: " << fileList[i].fileSize
            << ", duration:" << fileList[i].duration;
        ASSERT_LE(fileList[i].fileSize, 17 * BYTE_PER_MB); // 17: single cache trace file max size limit(MB)
        totalDuartion += fileList[i].duration;
        ASSERT_TRUE(IsFileIncludeAllKeyWords(fileList[i].filename, {"name: sched_wakeup"}));
    }
    totalDuartion /= S_TO_MS;
    ASSERT_GE(totalDuartion, 7); // total trace duration over 7s, given 1 second of tolerance
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: CacheModeTest002
 * @tc.desc: test dumptrace when cache trace was closed.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, CacheModeTest002, TestSize.Level1)
{
    const std::vector<std::string> tagGroups = {"default"};
    TraceErrorCode retCode = OpenTrace(tagGroups);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    // total cache filesize limit: 800MB, sliceduration: 20s
    retCode = CacheTraceOn(800, 5);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(8); // wait 8s
    retCode = CacheTraceOff();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(2); // wait 2s
    TraceRetInfo ret = DumpTrace();
    ASSERT_EQ(ret.errorCode, TraceErrorCode::SUCCESS);
    std::vector<FileWithInfo> traceFileList;
    EXPECT_TRUE(GetFileInfo(TRACE_SNAPSHOT, ret.outputFiles, traceFileList));
    uint64_t totalDuartion = 0;
    EXPECT_GE(traceFileList.size(), 1); // cache_trace_ file count > 1
    for (auto i = 0; i < traceFileList.size(); i++) {
        GTEST_LOG_(INFO) << "file: " << traceFileList[i].filename.c_str() << ", size: " << traceFileList[i].fileSize
            << ", duration:" << traceFileList[i].duration;
        totalDuartion += traceFileList[i].duration;
        EXPECT_TRUE(IsFileIncludeAllKeyWords(traceFileList[i].filename, {"name: sched_wakeup"}));
    }
    totalDuartion /= S_TO_MS;
    EXPECT_GE(totalDuartion, 10); // total trace duration over 10s
    ASSERT_TRUE(CloseTrace() == TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: CacheModeTest003
 * @tc.desc: Test aging cache trace file when OpenTrace over 30s.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, CacheModeTest003, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    TraceErrorCode retCode = OpenTrace(tagGroups);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    // total cache filesize limit: 800MB, sliceduration: 5s
    retCode = CacheTraceOn(800, 5);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(8); // wait 8s
    retCode = CloseTrace();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(30); // wait 30s: start aging file
    retCode = OpenTrace(tagGroups);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    ASSERT_EQ(GetTraceFilesInDir(TRACE_CACHE).size(), 0); // no cache trace file
    retCode = CloseTrace();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
}

/**
 * @tc.name: CacheModeTest004
 * @tc.desc: Test aging cache trace file when file size overflow
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, CacheModeTest004, TestSize.Level0)
{
    const std::vector<std::string> tagGroups = {"default"};
    TraceErrorCode retCode = OpenTrace(tagGroups);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    // total cache filesize limit: 5MB, sliceduration: 2s
    retCode = CacheTraceOn(5, 2);
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    sleep(10); // wait 10s
    retCode = CacheTraceOff();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
    std::vector<std::string> fileVec = GetTraceFilesInDir(TRACE_CACHE);
    std::vector<FileWithInfo> cacheFileList;
    ASSERT_TRUE(GetFileInfo(TRACE_CACHE, fileVec, cacheFileList));
    uint64_t totalFileSize = 0;
    for (auto i = 0; i < cacheFileList.size(); i++) {
        GTEST_LOG_(INFO) << "file: " << cacheFileList[i].filename.c_str() << ", size: " << cacheFileList[i].fileSize;
        totalFileSize += cacheFileList[i].fileSize;
    }
    ASSERT_LT(totalFileSize, 6 * BYTE_PER_MB); // aging file in 5MB - 6MB
    retCode = CloseTrace();
    ASSERT_EQ(retCode, TraceErrorCode::SUCCESS) << "errorCode: " << static_cast<int>(retCode);
}

/**
 * @tc.name: RecordingModeTest001
 * @tc.desc: test open recording mode with sched tag
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest001, TestSize.Level1)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:18432", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(IsFileIncludeAllKeyWords(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT,
        {"name: print", "name: sched_wakeup", "name: sched_switch"}));
    ASSERT_TRUE(IsFileExcludeAllKeyWords(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT,
        {"name: binder_transaction", "name: binder_transaction_received"}));
}

/**
 * @tc.name: RecordingModeTest002
 * @tc.desc: test open recording mode twice, the recording mode should be closed
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest002, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:18432", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(IsTracingOn());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:18432", "OpenRecording failed", "errorCode(1013)"},
        traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(IsTracingOn());
}

/**
 * @tc.name: RecordingModeTest003
 * @tc.desc: test close recording mode twice, the record trace contain tag format
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest003, TestSize.Level1)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:18432", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_finish --record",
        {"RECORDING_LONG_FINISH_RECORD", "capture done, output files"}, traceLists));
    ASSERT_FALSE(traceLists.empty());
    for (const auto& trace : traceLists) {
        ASSERT_TRUE(IsFileIncludeAllKeyWords(TRACE_FILE_DEFAULT_DIR + trace,
            {"name: print", "name: sched_wakeup", "name: sched_switch"}));
    }
}

/**
 * @tc.name: RecordingModeTest004
 * @tc.desc: test close recording if close already
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest004, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_finish --record",
        {"RECORDING_LONG_FINISH_RECORD", "RecordingOff failed", "errorCode(1012)"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: RecordingModeTest005
 * @tc.desc: test recording mode file aging
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest005, TestSize.Level2)
{
    int testCnt = 30; // 30 : test cnt
    while (testCnt-- > 0) {
        ASSERT_TRUE(RunCmd("hitrace --trace_begin --record sched"));
        ASSERT_TRUE(RunCmd("hitrace --trace_finish --record"));
    }
    std::vector<std::string> dirTraceLists = {};
    GetRecordingTraceFileList(dirTraceLists);
    if (IsRootVersion()) {
        ASSERT_GE(dirTraceLists.size(), 30); // 30 : file cnt
    } else {
        ASSERT_LE(dirTraceLists.size(), 16); // 16 : max file cnt
    }
}

/**
 * @tc.name: RecordingModeTest006
 * @tc.desc: test recording mode has a higher priority than snapshot mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest006, TestSize.Level1)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --start_bgsrv", {"SNAPSHOT_START", "OpenSnapshot done"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:18432", "errorCode(1013)"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: RecordingModeTest007
 * @tc.desc: test recording mode buffer size customization
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest007, TestSize.Level1)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 102400",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:102400", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_EQ(ReadBufferSizeKB(), IsHmKernel() ? "102400" : "102402");
}

/**
 * @tc.name: RecordingModeTest008
 * @tc.desc: test recording mode buffer size customization in hm kernel:[256, 1048576]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest008, TestSize.Level1)
{
    if (!IsHmKernel()) {
        return;
    }
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 255",
        {"buffer size must be from 256 KB to 1024 MB", "parsing args failed, exit"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 256",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:256", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_EQ(ReadBufferSizeKB(), "512");
}

/**
 * @tc.name: RecordingModeTest009
 * @tc.desc: test recording mode buffer size customization in hm kernel:[256, 1048576]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest009, TestSize.Level1)
{
    if (!IsHmKernel()) {
        return;
    }
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 1048577",
        {"buffer size must be from 256 KB to 1024 MB", "parsing args failed, exit"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 1048576",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:1048576", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_EQ(ReadBufferSizeKB(), "1048576");
}

/**
 * @tc.name: RecordingModeTest010
 * @tc.desc: test recording mode buffer size customization in linux kernel:[256, 307200], hm kernel:[256, 1048576]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest010, TestSize.Level1)
{
    if (IsHmKernel()) {
        return;
    }
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 255",
        {"buffer size must be from 256 KB to 300 MB", "parsing args failed, exit"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 256",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:256", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_EQ(ReadBufferSizeKB(), "258");
}

/**
 * @tc.name: RecordingModeTest011
 * @tc.desc: test recording mode buffer size customization in linux kernel: [256, 307200]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, RecordingModeTest011, TestSize.Level1)
{
    if (IsHmKernel()) {
        return;
    }
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 307201",
        {"buffer size must be from 256 KB to 300 MB", "parsing args failed, exit"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin --record sched -b 307200",
        {"RECORDING_LONG_BEGIN_RECORD", "tags:sched", "bufferSize:307200", "trace capturing"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemTestErr001
 * @tc.desc: when excute hitrace record command failed, -b abc
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTestErr001, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    EXPECT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin ace -b abc",
                                        {"buffer size is illegal input"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemTestErr002
 * @tc.desc: when excute hitrace record command failed, -t abc
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTestErr002, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    EXPECT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin ace -t abc",
                                        {"the time is illegal input"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemTestErr003
 * @tc.desc: when excute hitrace record command failed, -t 0
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTestErr003, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    EXPECT_TRUE(CheckTraceCommandOutput("hitrace --trace_begin ace -t 0",
                                        {"to be greater than zero"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemTestErr004
 * @tc.desc: when excute hitrace record command failed, --abc
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTestErr004, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    EXPECT_TRUE(CheckTraceCommandOutput("hitrace --abc", {"parsing args failed"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemHelpTest
 * @tc.desc: excute hitrace help command
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemHelpTest, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    EXPECT_TRUE(CheckTraceCommandOutput("hitrace -h", {"trace_begin", "trace_finish"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemCompressesTest
 * @tc.desc: when excute hitrace record command with -z
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemCompressesTest, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(RunCmd("hitrace --trace_begin app"));
    ASSERT_TRUE(RunCmd("hitrace --trace_dump"));
    ASSERT_FALSE(CheckTraceCommandOutput("hitrace --trace_finish -z", {"TASK-PID"}, traceLists));
    ASSERT_TRUE(traceLists.empty());
}

/**
 * @tc.name: HitraceSystemRawTest
 * @tc.desc: when excute hitrace command with --raw
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemRawTest, TestSize.Level2)
{
    std::vector<std::string> traceLists = {};
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace -t 2 app --raw",
                                        {"RECORDING_SHORT_RAW", "capture done, output files:"}, traceLists));
    ASSERT_FALSE(traceLists.empty());
    for (size_t i = 0; i < traceLists.size(); i++) {
        std::string traceFilePath = "/data/log/hitrace/" + traceLists[i];
        ASSERT_TRUE(access(traceFilePath.c_str(), F_OK) != -1);
    }
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
