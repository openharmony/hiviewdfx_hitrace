/*
 * Copyright (C) 2023-2025 Huawei Device Co., Ltd.
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

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <csignal>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <functional>
#include <map>

#include "common_define.h"
#include "common_utils.h"
#include "dfx_dump_catcher.h"
#include "dynamic_buffer.h"
#include "file_ageing_utils.h"
#include "hitrace_meter.h"
#include "hitrace_option/hitrace_option.h"
#include "hilog/log.h"
#include "parameters.h"
#include "securec.h"
#include "trace_dump_executor.h"
#include "trace_dump_pipe.h"
#include "trace_file_utils.h"
#include "trace_json_parser.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceDump"
#endif
namespace {
struct TraceParams {
    std::vector<std::string> tags;
    std::vector<std::string> tagGroups;
    std::vector<std::string> filterPids;
    std::string bufferSize;
    std::string clockType;
    std::string isOverWrite;
    std::string outputFile;
    int fileLimit;
    int fileSize;
    int appPid;
};

const int SAVED_CMDLINES_SIZE = 3072; // 3M
const int BYTE_PER_MB = 1024 * 1024;
constexpr int32_t MAX_RATIO_UNIT = 1000;
constexpr uint32_t DURATION_TOLERANCE = 100;
constexpr int32_t DEFAULT_FULL_TRACE_LENGTH = 30;
constexpr uint64_t SNAPSHOT_MIN_REMAINING_SPACE = 300 * 1024 * 1024;     // 300M
constexpr uint64_t DEFAULT_ASYNC_TRACE_SIZE = 50 * 1024 * 1024;          // 50M
constexpr int ASYNC_WAIT_EMPTY_LOOP_CNT = 180; // 3 minutes

static volatile sig_atomic_t g_traceDumpTaskPid = -1;
std::atomic<pid_t> g_asyncWaitTid(-1);

std::mutex g_traceMutex;
bool g_serviceThreadIsStart = false;
uint64_t g_sysInitParamTags = 0;
uint8_t g_traceMode = TraceMode::CLOSE;
std::string g_traceRootPath;
uint64_t g_totalFileSizeLimit = 0;
uint64_t g_sliceMaxDuration = 0;
uint64_t g_traceStartTime = 0;
uint64_t g_traceEndTime = std::numeric_limits<uint64_t>::max(); // in nano seconds
uint64_t g_firstPageTimestamp = std::numeric_limits<uint64_t>::max();
uint64_t g_lastPageTimestamp = 0;
uint64_t g_utDestTraceStartTime = 0;
uint64_t g_utDestTraceEndTime = 0;
uint8_t g_dumpStatus(TraceErrorCode::UNSET);
std::vector<TraceFileInfo> g_traceFileVec{};

TraceParams g_currentTraceParams = {};

std::mutex g_traceRetAndCallbackMutex;
std::map<uint64_t, std::function<void(TraceRetInfo)>> g_callbacks;
std::map<uint64_t, TraceRetInfo> g_traceRetInfos;

const std::string TELEMETRY_APP_PARAM = "debug.hitrace.telemetry.app";

bool IsTraceOpen()
{
    return (g_traceMode & TraceMode::OPEN) != 0;
}

bool IsRecordOn()
{
    return (g_traceMode & TraceMode::RECORD) != 0;
}

bool IsCacheOn()
{
    return (g_traceMode & TraceMode::CACHE) != 0;
}

std::vector<std::string> Split(const std::string& str, char delimiter)
{
    std::vector<std::string> res;
    size_t startPos = 0;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == delimiter) {
            res.push_back(str.substr(startPos, i - startPos));
            startPos = i + 1;
        }
    }
    if (startPos < str.size()) {
        res.push_back(str.substr(startPos));
    }
    return res;
}

bool CheckTags(const std::vector<std::string>& tags, const std::map<std::string, TraceTag>& allTags)
{
    for (const auto &tag : tags) {
        if (allTags.find(tag) == allTags.end()) {
            HILOG_ERROR(LOG_CORE, "CheckTags: %{public}s is not provided.", tag.c_str());
            return false;
        }
    }
    return true;
}

bool CheckTagGroup(const std::vector<std::string>& tagGroups,
                   const std::map<std::string, std::vector<std::string>>& tagGroupTable)
{
    for (auto groupName : tagGroups) {
        if (tagGroupTable.find(groupName) == tagGroupTable.end()) {
            HILOG_ERROR(LOG_CORE, "CheckTagGroup: %{public}s is not provided.", groupName.c_str());
            return false;
        }
    }
    return true;
}

bool WriteStrToFileInner(const std::string& filename, const std::string& str)
{
    std::ofstream out;
    out.open(filename, std::ios::out);
    if (out.fail()) {
        HILOG_ERROR(LOG_CORE, "WriteStrToFile: %{public}s open failed.", filename.c_str());
        return false;
    }
    out << str;
    if (!out.good()) {
        HILOG_ERROR(LOG_CORE, "WriteStrToFile: %{public}s write failed.", filename.c_str());
        out.close();
        return false;
    }
    out.flush();
    out.close();
    return true;
}

bool WriteStrToFile(const std::string& filename, const std::string& str)
{
    if (access((g_traceRootPath + filename).c_str(), W_OK) < 0) {
        HILOG_WARN(LOG_CORE, "WriteStrToFile: Failed to access %{public}s, errno(%{public}d).",
            (g_traceRootPath + filename).c_str(), errno);
        return false;
    }
    return WriteStrToFileInner(g_traceRootPath + filename, str);
}

bool SetTraceNodeStatus(const std::string &path, bool enabled)
{
    return WriteStrToFile(path, enabled ? "1" : "0");
}

void TruncateFile(const std::string& path)
{
    int fd = creat((g_traceRootPath + path).c_str(), 0);
    if (fd == -1) {
        HILOG_ERROR(LOG_CORE, "TruncateFile: clear old trace failed.");
        return;
    }
    close(fd);
    return;
}

bool SetProperty(const std::string& property, const std::string& value)
{
    bool result = OHOS::system::SetParameter(property, value);
    if (!result) {
        HILOG_ERROR(LOG_CORE, "SetProperty: set %{public}s failed.", value.c_str());
    } else {
        HILOG_INFO(LOG_CORE, "SetProperty: set %{public}s success.", value.c_str());
    }
    return result;
}

void ClearFilterParam()
{
    bool ok = true;
    if (!OHOS::system::SetParameter(TELEMETRY_APP_PARAM, "")) {
        HILOG_ERROR(LOG_CORE, "ClearFilterParam: clear param fail");
        ok = false;
    }
    if (ClearFilterPid() != HITRACE_NO_ERROR) {
        HILOG_ERROR(LOG_CORE, "ClearFilterParam: clear pid fail");
        ok = false;
    }

    HILOG_INFO(LOG_CORE, "ClearFilterParam %{public}d.", ok);
}

// close all trace node
void TraceInit(const std::map<std::string, TraceTag>& allTags)
{
    // close all ftrace events
    for (auto it = allTags.begin(); it != allTags.end(); it++) {
        if (it->second.type != 1) {
            continue;
        }
        for (size_t i = 0; i < it->second.enablePath.size(); i++) {
            if (!SetTraceNodeStatus(it->second.enablePath[i], false)) {
                HILOG_ERROR(LOG_CORE, "TraceInit: SetTraceNodeStatus fail");
            }
        }
    }
    // close all user tags
    SetProperty(TRACE_TAG_ENABLE_FLAGS, std::to_string(0));

    // set buffer_size_kb 1
    if (!WriteStrToFile("buffer_size_kb", "1")) {
        HILOG_ERROR(LOG_CORE, "TraceInit: WriteStrToFile fail");
    }

    // close tracing_on
    SetTraceNodeStatus(TRACING_ON_NODE, false);
}

// Open specific trace node
void SetAllTags(const TraceParams& traceParams, const std::map<std::string, TraceTag>& allTags,
                const std::map<std::string, std::vector<std::string>>& tagGroupTable,
                std::vector<std::string>& tagFmts)
{
    std::set<std::string> readyEnableTagList;
    for (std::string tagName : traceParams.tags) {
        readyEnableTagList.insert(tagName);
    }

    for (std::string groupName : traceParams.tagGroups) {
        auto iter = tagGroupTable.find(groupName);
        if (iter == tagGroupTable.end()) {
            continue;
        }
        for (std::string tag : iter->second) {
            readyEnableTagList.insert(tag);
        }
    }

    uint64_t enabledUserTags = 0;
    for (std::string tagName : readyEnableTagList) {
        auto iter = allTags.find(tagName);
        if (iter == allTags.end()) {
            HILOG_ERROR(LOG_CORE, "tag<%{public}s> is invalid.", tagName.c_str());
            continue;
        }

        if (iter->second.type == 0) {
            enabledUserTags |= iter->second.tag;
        }

        if (iter->second.type == 1) {
            for (const auto& path : iter->second.enablePath) {
                if (!SetTraceNodeStatus(path, true)) {
                    HILOG_ERROR(LOG_CORE, "SetAllTags: SetTraceNodeStatus fail");
                }
            }
            for (const auto& format : iter->second.formatPath) {
                tagFmts.emplace_back(format);
            }
        }
    }
    SetProperty(TRACE_TAG_ENABLE_FLAGS, std::to_string(enabledUserTags));
}

void SetClock(const std::string& clockType)
{
    const std::string traceClockPath = "trace_clock";
    if (clockType.size() == 0) {
        if (!WriteStrToFile(traceClockPath, "boot")) { // set default: boot
            HILOG_ERROR(LOG_CORE, "SetClock: WriteStrToFile fail.");
        }
        return;
    }
    std::string allClocks = ReadFile(traceClockPath, g_traceRootPath);
    if (allClocks.find(clockType) == std::string::npos) {
        HILOG_ERROR(LOG_CORE, "SetClock: %{public}s is non-existent, set to boot", clockType.c_str());
        if (!WriteStrToFile(traceClockPath, "boot")) { // set default: boot
            HILOG_ERROR(LOG_CORE, "SetClock: WriteStrToFile fail.");
        }
        return;
    }

    allClocks.erase(allClocks.find_last_not_of(" \n") + 1);
    allClocks.push_back(' ');

    std::set<std::string> allClockTypes;
    size_t curPos = 0;
    for (size_t i = 0; i < allClocks.size(); i++) {
        if (allClocks[i] == ' ') {
            allClockTypes.insert(allClocks.substr(curPos, i - curPos));
            curPos = i + 1;
        }
    }

    std::string currentClockType;
    for (auto i : allClockTypes) {
        if (clockType.compare(i) == 0) {
            HILOG_INFO(LOG_CORE, "SetClock: set clock %{public}s success.", clockType.c_str());
            if (!WriteStrToFile(traceClockPath, clockType)) {
                HILOG_ERROR(LOG_CORE, "SetClock: WriteStrToFile fail.");
            }
            return;
        }
        if (i[0] == '[') {
            currentClockType = i;
        }
    }

    const int marks = 2;
    if (clockType.compare(currentClockType.substr(1, currentClockType.size() - marks)) == 0) {
        HILOG_INFO(LOG_CORE, "SetClock: set clock %{public}s success.", clockType.c_str());
        return;
    }

    HILOG_INFO(LOG_CORE, "SetClock: unknown %{public}s, change to default clock_type: boot.", clockType.c_str());
    if (!WriteStrToFile(traceClockPath, "boot")) { // set default: boot
        HILOG_ERROR(LOG_CORE, "SetClock: WriteStrToFile fail.");
    }
    return;
}

bool SetTraceSetting(const TraceParams& traceParams, const std::map<std::string, TraceTag>& allTags,
                     const std::map<std::string, std::vector<std::string>>& tagGroupTable,
                     std::vector<std::string>& tagFmts)
{
    AddFilterPids(traceParams.filterPids);
    if (!traceParams.filterPids.empty()) {
        TruncateFile("trace_pipe_raw");
    }
    TraceInit(allTags);

    TruncateFile(TRACE_NODE);

    SetAllTags(traceParams, allTags, tagGroupTable, tagFmts);

    if (!WriteStrToFile("current_tracer", "nop")) {
        HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
    }
    if (!WriteStrToFile("buffer_size_kb", traceParams.bufferSize)) {
        HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
    }

    SetClock(traceParams.clockType);

    if (traceParams.isOverWrite == "1") {
        if (!WriteStrToFile("options/overwrite", "1")) {
            HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
        }
    } else {
        if (!WriteStrToFile("options/overwrite", "0")) {
            HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
        }
    }

    if (!WriteStrToFile("saved_cmdlines_size", std::to_string(SAVED_CMDLINES_SIZE))) {
        HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
    }
    if (!WriteStrToFile("options/record-tgid", "1")) {
        HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
    }
    if (!WriteStrToFile("options/record-cmd", "1")) {
        HILOG_ERROR(LOG_CORE, "SetTraceSetting: WriteStrToFile fail.");
    }
    return true;
}

TraceErrorCode SetTimeIntervalBoundary(uint64_t inputMaxDuration, uint64_t utTraceEndTime)
{
    uint64_t utNow = static_cast<uint64_t>(std::time(nullptr));
    if (utTraceEndTime >= utNow) {
        HILOG_WARN(LOG_CORE, "DumpTrace: Warning: traceEndTime is later than current time, set to current.");
        utTraceEndTime = 0;
    }
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    uint64_t btNow = static_cast<uint64_t>(bts.tv_sec) + (static_cast<uint64_t>(bts.tv_nsec) != 0 ? 1 : 0);
    uint64_t utBootTime = utNow - btNow;
    if (utTraceEndTime == 0) {
        g_traceEndTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
    } else if (utTraceEndTime > utBootTime) {
        // beware of input precision of seconds: add an extra second of tolerance
        g_traceEndTime = (utTraceEndTime - utBootTime + 1) * S_TO_NS;
    } else {
        HILOG_ERROR(LOG_CORE,
            "DumpTrace: traceEndTime:(%{public}" PRIu64 ") is earlier than boot_time:(%{public}" PRIu64 ").",
            utTraceEndTime, utBootTime);
        return OUT_OF_TIME;
    }

    uint64_t maxDuration = inputMaxDuration > 0 ? inputMaxDuration + 1 : 0;
    if (maxDuration > g_traceEndTime / S_TO_NS) {
        HILOG_WARN(LOG_CORE, "maxDuration is larger than TraceEndTime boot clock.");
        maxDuration = 0;
    }
    if (maxDuration > 0) {
        g_traceStartTime = g_traceEndTime - maxDuration * S_TO_NS;
    } else {
        g_traceStartTime = 0;
    }
    return SUCCESS;
}

void RestoreTimeIntervalBoundary()
{
    g_traceStartTime = 0;
    g_traceEndTime = std::numeric_limits<uint64_t>::max();
}

int32_t GetTraceFileFromVec(const uint64_t& inputTraceStartTime, const uint64_t& inputTraceEndTime,
    std::vector<TraceFileInfo>& fileVec, std::vector<TraceFileInfo>& targetFiles)
{
    int32_t coverDuration = 0;
    uint64_t utTargetStartTimeMs = inputTraceStartTime * S_TO_MS;
    uint64_t utTargetEndTimeMs = inputTraceEndTime * S_TO_MS;
    for (auto it = fileVec.begin(); it != fileVec.end(); it++) {
        if (access(it->filename.c_str(), F_OK) != 0) {
            HILOG_WARN(LOG_CORE, "GetTraceFileFromVec: %{public}s is not exist.", it->filename.c_str());
            continue;
        }
        HILOG_INFO(LOG_CORE, "GetTraceFileFromVec: %{public}s, [%{public}" PRIu64 ", %{public}" PRIu64 "].",
            it->filename.c_str(), it->traceStartTime, it->traceEndTime);
        if (((it->traceEndTime >= utTargetStartTimeMs && it->traceStartTime <= utTargetEndTimeMs)) &&
            (it->traceEndTime - it->traceStartTime < 2000 * S_TO_MS)) { // 2000 : max trace duration 2000s
            targetFiles.push_back(*it);
            coverDuration += static_cast<int32_t>(std::min(it->traceEndTime, utTargetEndTimeMs + DURATION_TOLERANCE) -
                std::max(it->traceStartTime, utTargetStartTimeMs - DURATION_TOLERANCE));
        }
    }
    return coverDuration;
}

void SearchTraceFiles(const uint64_t& inputTraceStartTime, const uint64_t& inputTraceEndTime,
    TraceRetInfo& traceRetInfo)
{
    HILOG_INFO(LOG_CORE, "target trace time: [%{public}" PRIu64 ", %{public}" PRIu64 "].",
        inputTraceStartTime, inputTraceEndTime);
    uint64_t curTime = GetCurUnixTimeMs();
    HILOG_INFO(LOG_CORE, "current time: %{public}" PRIu64 ".", curTime);
    int32_t coverDuration = 0;
    std::vector<TraceFileInfo> targetFiles;
    coverDuration += GetTraceFileFromVec(inputTraceStartTime, inputTraceEndTime, g_traceFileVec, targetFiles);
    auto inputCacheFiles = TraceDumpExecutor::GetInstance().GetCacheTraceFiles();
    coverDuration += GetTraceFileFromVec(inputTraceStartTime, inputTraceEndTime, inputCacheFiles, targetFiles);
    for (auto& file : targetFiles) {
        if (file.filename.find(CACHE_FILE_PREFIX) != std::string::npos) {
            file.filename = RenameCacheFile(file.filename);
            g_traceFileVec.push_back(file);
        }
        traceRetInfo.outputFiles.push_back(file.filename);
        traceRetInfo.fileSize += file.fileSize;
    }
    traceRetInfo.coverDuration += coverDuration;
}

void ProcessCacheTask()
{
    const std::string threadName = "CacheTraceTask";
    prctl(PR_SET_NAME, threadName.c_str());
    struct TraceDumpParam param = {
        TraceDumpType::TRACE_CACHE,
        g_currentTraceParams.outputFile,
        g_currentTraceParams.fileLimit,
        g_currentTraceParams.fileSize,
        0,
        std::numeric_limits<uint64_t>::max()
    };
    if (!TraceDumpExecutor::GetInstance().StartCacheTraceLoop(param, g_totalFileSizeLimit, g_sliceMaxDuration)) {
        HILOG_ERROR(LOG_CORE, "ProcessCacheTask: StartCacheTraceLoop failed.");
        return;
    }
    HILOG_INFO(LOG_CORE, "ProcessCacheTask: trace cache thread exit.");
}

void ProcessRecordTask()
{
    const std::string threadName = "RecordTraceTask";
    prctl(PR_SET_NAME, threadName.c_str());
    struct TraceDumpParam param = {
        TraceDumpType::TRACE_RECORDING,
        g_currentTraceParams.outputFile,
        g_currentTraceParams.fileLimit,
        g_currentTraceParams.fileSize,
        0,
        std::numeric_limits<uint64_t>::max()

    };
    TraceDumpExecutor::GetInstance().StartDumpTraceLoop(param);
}

void SetProcessName(std::string& processName)
{
    if (processName.size() <= 0) {
        return;
    }

    const int maxNameLen = 16;
    std::string setName;
    if (processName.size() > maxNameLen) {
        setName = processName.substr(0, maxNameLen);
    } else {
        setName = processName;
    }

    prctl(PR_SET_NAME, setName.c_str(), nullptr, nullptr, nullptr);
    HILOG_INFO(LOG_CORE, "New process: %{public}s.", setName.c_str());
}

void TimeoutSignalHandler(int signum)
{
    if (signum == SIGUSR1) {
        _exit(EXIT_SUCCESS);
    } else if (signum == SIGCHLD) {
        // only work for async dump process.
        if (g_traceDumpTaskPid > 0 && waitpid(g_traceDumpTaskPid, nullptr, WNOHANG) > 0) {
            g_traceDumpTaskPid = -1;
        }
    }
}

void LogStackTrace(const pid_t pid)
{
    DfxDumpCatcher dumplog;
    std::string stack;
    bool ret = dumplog.DumpCatch(pid, 0, stack);
    if (!ret) {
        HILOG_ERROR(LOG_CORE, "LogStackTrace: dump stack trace failed, pid: %{public}d", pid);
    }
    HILOG_INFO(LOG_CORE, "LogStackTrace: %{public}s", stack.c_str());
}

void WaitForChildProcess(const pid_t pid)
{
    const int maxWaitTime = 5000;
    const int checkInterval = 100;
    int waitedTime = 0;
    while (waitedTime < maxWaitTime) {
        pid_t result = TEMP_FAILURE_RETRY(waitpid(pid, nullptr, WNOHANG));
        if (result > 0) {
            HILOG_INFO(LOG_CORE, "Child process %d exited successfully.", pid);
            break;
        } else if (result == 0) {
            usleep(checkInterval * 1000); // 1000 : 1ms
            waitedTime += checkInterval;
        } else {
            HILOG_ERROR(LOG_CORE, "waitpid failed: %s", strerror(errno));
            break;
        }
    }
    if (waitedTime >= maxWaitTime) {
        HILOG_ERROR(LOG_CORE, "Child process %d did not exit within timeout.", pid);
    }
}

bool EpollWaitforChildProcess(pid_t& pid, int& pipefd, std::string& reOutPath)
{
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        HILOG_ERROR(LOG_CORE, "epoll_create1 error.");
        return false;
    }
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = pipefd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd, &event) == -1) {
        HILOG_ERROR(LOG_CORE, "epoll_ctl error.");
        close(epollfd);
        return false;
    }
    struct epoll_event events[1];
    int numEvents = 0;
    for (int retry = 0; retry < 10 && numEvents <= 0; retry++) { // 10 : ten seconds timeout
        numEvents = TEMP_FAILURE_RETRY(epoll_wait(epollfd, events, 1, 1000)); // 1000 : one second timeout
        if (numEvents == -1) {
            HILOG_ERROR(LOG_CORE, "epoll_wait error, error: (%{public}s).", strerror(errno));
            break;
        }
    }
    if (numEvents <= 0) {
        LogStackTrace(pid);
        HILOG_ERROR(LOG_CORE, "kill timeout child process.");
        if (kill(pid, SIGUSR1) != 0) {
            HILOG_ERROR(LOG_CORE, "kill child process failed.");
        }
        WaitForChildProcess(pid);
        close(epollfd);
        return false;
    }
    TraceDumpRet retVal;
    read(pipefd, &retVal, sizeof(retVal));
    HILOG_INFO(LOG_CORE,
        "Epoll wait read : %{public}d, outputFile: %{public}s, [%{public}" PRIu64 ", %{public}" PRIu64 "].",
        retVal.code, retVal.outputFile, retVal.traceStartTime, retVal.traceEndTime);
    g_dumpStatus = retVal.code;
    reOutPath = retVal.outputFile;
    g_firstPageTimestamp = retVal.traceStartTime;
    g_lastPageTimestamp = retVal.traceEndTime;
    close(epollfd);
    WaitForChildProcess(pid);
    return true;
}

TraceErrorCode HandleDumpResult(std::string& reOutPath, TraceRetInfo& traceRetInfo)
{
    SearchTraceFiles(g_utDestTraceStartTime, g_utDestTraceEndTime, traceRetInfo);
    if (g_dumpStatus) {
        if (remove(reOutPath.c_str()) == 0) {
            HILOG_INFO(LOG_CORE, "Delete outpath:%{public}s success.", reOutPath.c_str());
        } else {
            HILOG_INFO(LOG_CORE, "Delete outpath:%{public}s failed.", reOutPath.c_str());
        }
    } else if (access(reOutPath.c_str(), F_OK) != 0) { // trace access error
        HILOG_ERROR(LOG_CORE, "ProcessDump: write %{public}s failed.", reOutPath.c_str());
    } else {
        HILOG_INFO(LOG_CORE, "Output: %{public}s.", reOutPath.c_str());
        TraceFileInfo traceFileInfo;
        if (!SetFileInfo(true, reOutPath, g_firstPageTimestamp, g_lastPageTimestamp, traceFileInfo)) {
            // trace rename error
            HILOG_ERROR(LOG_CORE, "SetFileInfo: set %{public}s info failed.", reOutPath.c_str());
            RemoveFile(reOutPath);
        } else { // success
            g_traceFileVec.push_back(traceFileInfo);
            traceRetInfo.outputFiles.push_back(traceFileInfo.filename);
            traceRetInfo.coverDuration +=
                static_cast<int32_t>(traceFileInfo.traceEndTime - traceFileInfo.traceStartTime);
            traceRetInfo.fileSize += traceFileInfo.fileSize;
        }
    }

    if (traceRetInfo.outputFiles.empty()) {
        return (g_dumpStatus != 0) ? static_cast<TraceErrorCode>(g_dumpStatus) : TraceErrorCode::FILE_ERROR;
    }
    return TraceErrorCode::SUCCESS;
}

void HandleAsyncDumpResult(TraceDumpTask& task, TraceRetInfo& traceRetInfo)
{
    SearchTraceFiles(g_utDestTraceStartTime, g_utDestTraceEndTime, traceRetInfo);
    TraceFileInfo traceFileInfo;
    if (task.code == TraceErrorCode::SUCCESS) {
        if (!SetFileInfo(false, std::string(task.outputFile),
            g_firstPageTimestamp, g_lastPageTimestamp, traceFileInfo)) {
            // trace rename error
            HILOG_ERROR(LOG_CORE, "SetFileInfo: set %{public}s info failed.", task.outputFile);
        } else { // success
            traceFileInfo.fileSize = task.fileSize;
            g_traceFileVec.push_back(traceFileInfo);
            traceRetInfo.outputFiles.push_back(traceFileInfo.filename);
            traceRetInfo.coverDuration +=
                static_cast<int32_t>(traceFileInfo.traceEndTime - traceFileInfo.traceStartTime);
            traceRetInfo.fileSize += traceFileInfo.fileSize;
        }
        traceRetInfo.errorCode = task.code;
    } else {
        traceRetInfo.errorCode = traceRetInfo.outputFiles.empty() ? task.code : TraceErrorCode::SUCCESS;
    }
    if (task.isFileSizeOverLimit || traceRetInfo.fileSize > task.fileSizeLimit) {
        traceRetInfo.isOverflowControl = true;
    }
}

TraceErrorCode ProcessDumpSync(TraceRetInfo& traceRetInfo)
{
    auto taskCnt = TraceDumpExecutor::GetInstance().GetTraceDumpTaskCount();
    if (GetRemainingSpace("/data") <= SNAPSHOT_MIN_REMAINING_SPACE + taskCnt * DEFAULT_ASYNC_TRACE_SIZE) {
        HILOG_ERROR(LOG_CORE, "ProcessDumpSync: remaining space not enough");
        return TraceErrorCode::FILE_ERROR;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        HILOG_ERROR(LOG_CORE, "ProcessDumpSync: pipe creation error.");
        return TraceErrorCode::PIPE_CREATE_ERROR;
    }

    g_dumpStatus = TraceErrorCode::UNSET;
    /*Child process handles task, Father process wait.*/
    pid_t pid = fork();
    if (pid < 0) {
        HILOG_ERROR(LOG_CORE, "fork error.");
        return TraceErrorCode::FORK_ERROR;
    } else if (pid == 0) {
        signal(SIGUSR1, TimeoutSignalHandler);
        close(pipefd[0]);
        std::string processName = "HitraceDump";
        SetProcessName(processName);
        struct TraceDumpParam param = { TRACE_SNAPSHOT, "", 0, 0, g_traceStartTime, g_traceEndTime };
        TraceDumpRet ret = TraceDumpExecutor::GetInstance().DumpTrace(param);
        HILOG_INFO(LOG_CORE,
            "TraceDumpRet : %{public}d, outputFile: %{public}s, [%{public}" PRIu64 ", %{public}" PRIu64 "].",
            ret.code, ret.outputFile, ret.traceStartTime, ret.traceEndTime);
        write(pipefd[1], &ret, sizeof(ret));
        _exit(EXIT_SUCCESS);
    } else {
        close(pipefd[1]);
    }

    std::string reOutPath;
    if (!EpollWaitforChildProcess(pid, pipefd[0], reOutPath)) {
        close(pipefd[0]);
        return TraceErrorCode::EPOLL_WAIT_ERROR;
    }

    close(pipefd[0]);
    return HandleDumpResult(reOutPath, traceRetInfo);
}

void LoadDumpRet(TraceRetInfo& ret, int32_t committedDuration)
{
    ret.mode = g_traceMode;
    committedDuration = committedDuration <= 0 ? DEFAULT_FULL_TRACE_LENGTH : committedDuration;
    ret.coverRatio = ret.coverDuration / committedDuration;
    ret.tags.reserve(g_currentTraceParams.tagGroups.size() + g_currentTraceParams.tags.size());
    ret.tags.insert(ret.tags.end(), g_currentTraceParams.tagGroups.begin(), g_currentTraceParams.tagGroups.end());
    ret.tags.insert(ret.tags.end(), g_currentTraceParams.tags.begin(), g_currentTraceParams.tags.end());
}

void SanitizeRetInfo(TraceRetInfo& traceRetInfo)
{
    traceRetInfo.coverDuration =
        std::min(traceRetInfo.coverDuration, static_cast<int>(DEFAULT_FULL_TRACE_LENGTH * S_TO_MS));
    traceRetInfo.coverRatio = std::min(traceRetInfo.coverRatio, MAX_RATIO_UNIT);
}

TraceDumpTask WaitSyncDumpRetLoop(const pid_t pid, const std::shared_ptr<HitraceDumpPipe> pipe)
{
    HILOG_INFO(LOG_CORE, "WaitSyncDumpRetLoop: start.");
    TraceDumpTask task;
    if (pipe->ReadSyncDumpRet(10, task)) { // 10 : 10 seconds
        g_firstPageTimestamp = task.traceStartTime;
        g_lastPageTimestamp = task.traceEndTime;
        g_dumpStatus = task.code;
        if (task.status == TraceDumpStatus::WRITE_DONE) {
            task.status = TraceDumpStatus::FINISH;
            TraceDumpExecutor::GetInstance().RemoveTraceDumpTask(task.time);
            HILOG_INFO(LOG_CORE, "WaitSyncDumpRetLoop: task finished.");
        } else {
            HILOG_ERROR(LOG_CORE, "WaitSyncDumpRetLoop: task status is not FINISH.");
            TraceDumpExecutor::GetInstance().UpdateTraceDumpTask(task);
        }
    } else {
        task.code = TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT;
        TraceDumpExecutor::GetInstance().ClearTraceDumpTask();
        LogStackTrace(pid);
        if (kill(pid, SIGUSR1) != 0) {
            HILOG_ERROR(LOG_CORE, "WaitSyncDumpRetLoop: kill dump process failed.");
        }
        HILOG_WARN(LOG_CORE, "WaitSyncDumpRetLoop: wait timeout, clear task and kill dump process.");
        WaitForChildProcess(pid);
    }
    HILOG_INFO(LOG_CORE, "WaitSyncDumpRetLoop: exit.");
    return task;
}

void WaitAsyncDumpRetLoop(const std::shared_ptr<HitraceDumpPipe> pipe)
{
    g_asyncWaitTid.store(gettid());
    HILOG_INFO(LOG_CORE, "WaitAsyncDumpRetLoop: start.");
    int emptyLoopCnt = 0;
    do {
        HILOG_INFO(LOG_CORE, "WaitAsyncDumpRetLoop: loop start.");
        if (emptyLoopCnt >= ASYNC_WAIT_EMPTY_LOOP_CNT || !IsProcessExist(g_traceDumpTaskPid)) {
            g_traceDumpTaskPid = -1;
            HILOG_INFO(LOG_CORE, "WaitAsyncDumpRetLoop: task queue is empty or dump process has gone.");
            TraceDumpExecutor::GetInstance().ClearTraceDumpTask();
            HitraceDumpPipe::ClearTraceDumpPipe();
            break;
        }
        TraceDumpTask task;
        if (!pipe->ReadAsyncDumpRet(1, task)) {
            emptyLoopCnt++;
            continue;
        }
        emptyLoopCnt = 0;
        if (task.status == TraceDumpStatus::WRITE_DONE) {
            task.status = TraceDumpStatus::FINISH;
            HILOG_INFO(LOG_CORE, "WaitAsyncDumpRetLoop: task finished.");
            std::lock_guard<std::mutex> lock(g_traceRetAndCallbackMutex);
            auto traceRetInfo = g_traceRetInfos[task.time];
            traceRetInfo.fileSize = 0;
            for (auto& file : traceRetInfo.outputFiles) {
                traceRetInfo.fileSize += GetFileSize(file);
            }
            if (traceRetInfo.fileSize > task.fileSizeLimit) {
                traceRetInfo.isOverflowControl = true;
            }
            if (g_callbacks[task.time] != nullptr) {
                g_callbacks[task.time](traceRetInfo);
                HILOG_INFO(LOG_CORE, "WaitAsyncDumpRetLoop: call callback func done, taskid[%{public}" PRIu64 "]",
                    task.time);
            }
            g_callbacks.erase(task.time);
            g_traceRetInfos.erase(task.time);
        } else {
            HILOG_ERROR(LOG_CORE, "WaitAsyncDumpRetLoop: task status is not FINISH.");
        }
        TraceDumpExecutor::GetInstance().RemoveTraceDumpTask(task.time);
    } while (true);
    HILOG_INFO(LOG_CORE, "WaitAsyncDumpRetLoop: exit.");
    g_asyncWaitTid.store(-1);
}

TraceErrorCode SubmitTaskAndWaitReturn(TraceDumpTask& task, TraceRetInfo& traceRetInfo)
{
    TraceDumpExecutor::GetInstance().AddTraceDumpTask(task);
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    if (!dumpPipe->SubmitTraceDumpTask(task)) {
        return TraceErrorCode::TRACE_TASK_SUBMIT_ERROR;
    }
    auto taskRet = WaitSyncDumpRetLoop(g_traceDumpTaskPid, dumpPipe);
    HandleAsyncDumpResult(taskRet, traceRetInfo);
    if (taskRet.status == TraceDumpStatus::FINISH) {
        HILOG_INFO(LOG_CORE, "SubmitTaskAndWaitReturn: task finished.");
        return traceRetInfo.errorCode;
    } else if (taskRet.code != TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT) {
        HILOG_ERROR(LOG_CORE, "SubmitTaskAndWaitReturn: task status is not FINISH.");
        if (g_asyncWaitTid.load() == -1) {
            std::thread asyncThread(WaitAsyncDumpRetLoop, std::move(dumpPipe));
            asyncThread.detach();
        }
        return TraceErrorCode::ASYNC_DUMP;
    }
    return TraceErrorCode::TRACE_TASK_DUMP_TIMEOUT;
}

bool SetSigChldHandler()
{
    struct sigaction sa;
    sa.sa_handler = TimeoutSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        HILOG_ERROR(LOG_CORE, "ProcessDumpAsync: Failed to setup SIGCHLD handler.");
        return false;
    }
    return true;
}

TraceErrorCode ProcessDumpAsync(const uint64_t taskid, const int64_t fileSizeLimit, TraceRetInfo& traceRetInfo)
{
    auto taskCnt = TraceDumpExecutor::GetInstance().GetTraceDumpTaskCount();
    if (GetRemainingSpace("/data") <= SNAPSHOT_MIN_REMAINING_SPACE + taskCnt * DEFAULT_ASYNC_TRACE_SIZE) {
        HILOG_ERROR(LOG_CORE, "ProcessDumpAsync: remaining space not enough");
        return TraceErrorCode::FILE_ERROR;
    }
    if (!SetSigChldHandler()) {
        return TraceErrorCode::FORK_ERROR;
    }

    TraceDumpTask task = {
        .time = taskid,
        .traceStartTime = g_traceStartTime,
        .traceEndTime = g_traceEndTime,
        .fileSizeLimit = fileSizeLimit
    };
    HILOG_INFO(LOG_CORE, "ProcessDumpAsync: new task id[%{public}" PRIu64 "]", task.time);
    if (IsProcessExist(g_traceDumpTaskPid) && taskCnt > 0) {
        // must have a trace dump process running, just submit trace dump task
        HILOG_INFO(LOG_CORE, "ProcessDumpAsync: task queue is not empty, do not fork new process.");
        return SubmitTaskAndWaitReturn(task, traceRetInfo);
    }
    HitraceDumpPipe::ClearTraceDumpPipe();
    if (!HitraceDumpPipe::InitTraceDumpPipe()) {
        HILOG_ERROR(LOG_CORE, "ProcessDumpAsync: create fifo failed.");
        return TraceErrorCode::PIPE_CREATE_ERROR;
    }
    pid_t pid = fork();
    if (pid < 0) {
        HILOG_ERROR(LOG_CORE, "ProcessDumpAsync: fork failed.");
        return TraceErrorCode::FORK_ERROR;
    }
    if (pid == 0) {
        signal(SIGUSR1, TimeoutSignalHandler);
        std::string processName = "HitraceDumpAsync";
        SetProcessName(processName);
        // create loop read thread and loop write thread.
        auto& traceDumpExecutor = TraceDumpExecutor::GetInstance();
        std::thread loopReadThrad(&TraceDumpExecutor::ReadRawTraceLoop, std::ref(traceDumpExecutor));
        std::thread loopWriteThread(&TraceDumpExecutor::WriteTraceLoop, std::ref(traceDumpExecutor));
        traceDumpExecutor.TraceDumpTaskMonitor();
        loopReadThrad.join();
        loopWriteThread.join();
        _exit(EXIT_SUCCESS);
    }
    g_traceDumpTaskPid = static_cast<sig_atomic_t>(pid);
    return SubmitTaskAndWaitReturn(task, traceRetInfo);
}

uint64_t GetSysParamTags()
{
    return OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
}

bool CheckParam()
{
    uint64_t currentTags = GetSysParamTags();
    if (currentTags == g_sysInitParamTags) {
        return true;
    }

    if (currentTags == 0) {
        HILOG_ERROR(LOG_CORE, "allowed tags are cleared, should restart.");
        return false;
    }
    HILOG_ERROR(LOG_CORE, "trace is being used, should restart.");
    return false;
}

/**
 * SERVICE_MODE is running, check param and tracing_on.
*/
bool CheckServiceRunning()
{
    if (CheckParam() && IsTracingOn(g_traceRootPath)) {
        return true;
    }
    return false;
}

void CpuBufferBalanceTask()
{
    g_serviceThreadIsStart = true;
    const std::string threadName = "CpuBufferBalancer";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "CpuBufferBalanceTask: monitor thread start.");
    const int intervalTime = 15;
    while (IsTraceOpen() && CheckServiceRunning()) {
        sleep(intervalTime);

        const int cpuNums = GetCpuProcessors();
        std::vector<int> result;
        std::unique_ptr<DynamicBuffer> dynamicBuffer = std::make_unique<DynamicBuffer>(g_traceRootPath, cpuNums);
        dynamicBuffer->CalculateBufferSize(result);

        if (static_cast<int>(result.size()) != cpuNums) {
            HILOG_ERROR(LOG_CORE, "CalculateAllNewBufferSize failed.");
            break;
        }

        for (size_t i = 0; i < result.size(); i++) {
            HILOG_DEBUG(LOG_CORE, "cpu%{public}zu set size %{public}d.", i, result[i]);
            std::string path = "per_cpu/cpu" + std::to_string(i) + "/buffer_size_kb";
            if (!WriteStrToFile(path, std::to_string(result[i]))) {
                HILOG_ERROR(LOG_CORE, "CpuBufferBalanceTask: WriteStrToFile failed.");
            }
        }
    }
    HILOG_INFO(LOG_CORE, "CpuBufferBalanceTask: monitor thread exit.");
    g_serviceThreadIsStart = false;
}

void StartCpuBufferBalanceService()
{
    if (!IsHmKernel() && !g_serviceThreadIsStart) {
        // open monitor thread
        auto it = []() {
            CpuBufferBalanceTask();
        };
        std::thread auxiliaryTask(it);
        auxiliaryTask.detach();
    }
}

bool PreWriteEventsFormat(const std::vector<std::string>& eventFormats)
{
    DelSavedEventsFormat();
    const std::string savedEventsFormatPath = TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT;
    int fd = open(savedEventsFormatPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (fd < 0) {
        HILOG_ERROR(LOG_CORE, "PreWriteEventsFormat: open %{public}s failed.", savedEventsFormatPath.c_str());
        return false;
    }
    for (auto& format : eventFormats) {
        std::string srcPath = g_traceRootPath + format;
        if (access(srcPath.c_str(), R_OK) != -1) {
            WriteEventFile(srcPath, fd);
        }
    }
    close(fd);
    HILOG_INFO(LOG_CORE, "PreWriteEventsFormat end.");
    return true;
}

TraceErrorCode HandleTraceOpen(const TraceParams& traceParams,
                               const std::map<std::string, TraceTag>& allTags,
                               const std::map<std::string, std::vector<std::string>>& tagGroupTable,
                               std::vector<std::string>& tagFmts)
{
    if (!SetTraceSetting(traceParams, allTags, tagGroupTable, tagFmts)) {
        return TraceErrorCode::FILE_ERROR;
    }
    SetTraceNodeStatus(TRACING_ON_NODE, true);
    PreWriteEventsFormat(tagFmts);
    g_currentTraceParams = traceParams;
    return TraceErrorCode::SUCCESS;
}

TraceErrorCode HandleDefaultTraceOpen(const std::vector<std::string>& tagGroups)
{
    TraceJsonParser& traceJsonParser = TraceJsonParser::Instance();
    const std::map<std::string, TraceTag>& allTags = traceJsonParser.GetAllTagInfos();
    const std::map<std::string, std::vector<std::string>>& tagGroupTable = traceJsonParser.GetTagGroups();
    std::vector<std::string> tagFmts = traceJsonParser.GetBaseFmtPath();

    if (tagGroups.size() == 0 || !CheckTagGroup(tagGroups, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceParams defaultTraceParams;
    defaultTraceParams.tagGroups = tagGroups;
    defaultTraceParams.bufferSize = std::to_string(traceJsonParser.GetSnapshotDefaultBufferSizeKb());
    defaultTraceParams.clockType = "boot";
    defaultTraceParams.isOverWrite = "1";
    defaultTraceParams.fileSize = DEFAULT_FILE_SIZE;
    return HandleTraceOpen(defaultTraceParams, allTags, tagGroupTable, tagFmts);
}

void RemoveUnSpace(std::string str, std::string& args)
{
    int maxCircleTimes = 30;
    int curTimes = 0;
    const size_t symbolAndSpaceLen = 2;
    std::string strSpace = str + " ";
    while (curTimes < maxCircleTimes) {
        curTimes++;
        std::string::size_type index = args.find(strSpace);
        if (index != std::string::npos) {
            args.replace(index, symbolAndSpaceLen, str);
        } else {
            break;
        }
    }
}

void SetCmdTraceIntParams(const std::string& traceParamsStr, int& traceParams)
{
    if (!OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, traceParams)) {
        traceParams = 0;
        return;
    }
    if (traceParams <= 0) {
        HILOG_WARN(LOG_CORE, "Illegal input, traceParams initialized to null.");
        traceParams = 0;
    }
}

void SetDestTraceTimeAndDuration(uint32_t maxDuration, const uint64_t& utTraceEndTime)
{
    if (utTraceEndTime == 0) {
        time_t currentTime;
        time(&currentTime);
        g_utDestTraceEndTime = static_cast<uint64_t>(currentTime);
    } else {
        g_utDestTraceEndTime = utTraceEndTime;
    }
    if (maxDuration == 0 || maxDuration == UINT32_MAX) {
        maxDuration = static_cast<uint32_t>(DEFAULT_FULL_TRACE_LENGTH);
    }
    if (g_utDestTraceEndTime <= maxDuration) {
        g_utDestTraceStartTime = 1; // theoretical impossible value, to avoid overflow while minus tolerance 100ms
    } else {
        g_utDestTraceStartTime = g_utDestTraceEndTime - maxDuration;
    }
    HILOG_INFO(LOG_CORE, "g_utDestTraceStartTime:(%{public}" PRIu64 "), g_utDestTraceEndTime:(%{public}" PRIu64 ").",
        g_utDestTraceStartTime, g_utDestTraceEndTime);
}

/**
 * args: tags:tag1,tags2... tagGroups:group1,group2... clockType:boot bufferSize:1024 overwrite:1 output:filename
 * traceParams:  Save the above parameters
*/
bool ParseArgs(std::string args, TraceParams& traceParams, const std::map<std::string, TraceTag>& allTags,
               const std::map<std::string, std::vector<std::string>>& tagGroupTable)
{
    RemoveUnSpace(":", args);
    RemoveUnSpace(",", args);
    std::vector<std::string> argList = Split(args, ' ');
    for (std::string item : argList) {
        size_t pos = item.find(":");
        if (pos == std::string::npos) {
            HILOG_ERROR(LOG_CORE, "trace command line without colon appears: %{public}s, continue.", item.c_str());
            continue;
        }
        std::string itemName = item.substr(0, pos);
        if (itemName == "tags") {
            traceParams.tags = Split(item.substr(pos + 1), ',');
        } else if (itemName == "tagGroups") {
            traceParams.tagGroups = Split(item.substr(pos + 1), ',');
        } else if (itemName == "clockType") {
            traceParams.clockType = item.substr(pos + 1);
        } else if (itemName == "bufferSize") {
            traceParams.bufferSize = item.substr(pos + 1);
        } else if (itemName == "overwrite") {
            traceParams.isOverWrite = item.substr(pos + 1);
        } else if (itemName == "output") {
            traceParams.outputFile = item.substr(pos + 1);
        } else if (itemName == "fileSize") {
            std::string fileSizeStr = item.substr(pos + 1);
            SetCmdTraceIntParams(fileSizeStr, traceParams.fileSize);
        } else if (itemName == "fileLimit") {
            std::string fileLimitStr = item.substr(pos + 1);
            SetCmdTraceIntParams(fileLimitStr, traceParams.fileLimit);
        } else if (itemName == "appPid") {
            std::string pidStr = item.substr(pos + 1);
            SetCmdTraceIntParams(pidStr, traceParams.appPid);
            if (traceParams.appPid == 0) {
                HILOG_ERROR(LOG_CORE, "Illegal input, appPid(%{public}s) must be number and greater than 0.",
                    pidStr.c_str());
                return false;
            }
            OHOS::system::SetParameter(TRACE_KEY_APP_PID, pidStr);
        } else if (itemName == "filterPids") {
            traceParams.filterPids = Split(item.substr(pos + 1), ',');
        } else {
            HILOG_ERROR(LOG_CORE, "Extra trace command line options appear when ParseArgs: %{public}s, return false.",
                itemName.c_str());
            return false;
        }
    }
    return CheckTags(traceParams.tags, allTags) && CheckTagGroup(traceParams.tagGroups, tagGroupTable);
}

void WriteCpuFreqTrace()
{
    std::string freqsfmt = "cpu frequency: ";
    ReadCurrentCpuFrequencies(freqsfmt);
    HILOG_INFO(LOG_CORE, "hitracedump write trace(%{public}s)", freqsfmt.c_str());
    HITRACE_METER_NAME(HITRACE_TAG_OHOS, freqsfmt);
}

void SetTotalFileSizeLimitAndSliceMaxDuration(const uint64_t& totalFileSize, const uint64_t& sliceMaxDuration)
{
    if (totalFileSize == 0) {
        g_totalFileSizeLimit = DEFAULT_TOTAL_CACHE_FILE_SIZE * BYTE_PER_MB;
    } else {
        g_totalFileSizeLimit = totalFileSize * BYTE_PER_MB;
    }
    if (sliceMaxDuration == 0) {
        g_sliceMaxDuration = DEFAULT_TRACE_SLICE_DURATION;
    } else {
        g_sliceMaxDuration = sliceMaxDuration;
    }
}

void GetFileInCache(TraceRetInfo& traceRetInfo)
{
    HILOG_INFO(LOG_CORE, "DumpTrace: Trace is caching, get cache file.");
    SearchTraceFiles(g_utDestTraceStartTime, g_utDestTraceEndTime, traceRetInfo);
    if (traceRetInfo.outputFiles.empty()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: Trace is caching, but failed to retrieve target trace file.");
        traceRetInfo.errorCode = OUT_OF_TIME;
    } else {
        for (const auto& file: traceRetInfo.outputFiles) {
            HILOG_INFO(LOG_CORE, "dumptrace file is %{public}s.", file.c_str());
        }
        traceRetInfo.errorCode = SUCCESS;
    }
}

bool CheckTraceDumpStatus(const uint32_t maxDuration, const uint64_t utTraceEndTime, TraceRetInfo& ret)
{
    if (!IsTraceOpen() || IsRecordOn()) {
        HILOG_ERROR(LOG_CORE, "CheckTraceDumpStatus: WRONG_TRACE_MODE, current trace mode: %{public}u.",
            static_cast<uint32_t>(g_traceMode));
        ret.errorCode = WRONG_TRACE_MODE;
        return false;
    }
    if (maxDuration  == UINT32_MAX) {
        HILOG_ERROR(LOG_CORE, "CheckTraceDumpStatus: Illegal input: maxDuration = %{public}d is invalid.",
                    maxDuration);
        ret.errorCode = INVALID_MAX_DURATION;
        return false;
    }

    if (!CheckServiceRunning()) {
        HILOG_ERROR(LOG_CORE, "CheckTraceDumpStatus: TRACE_IS_OCCUPIED.");
        ret.errorCode = TRACE_IS_OCCUPIED;
        return false;
    }
    return true;
}
} // namespace

#ifdef HITRACE_UNITTEST
void SetSysInitParamTags(uint64_t sysInitParamTags)
{
    g_sysInitParamTags = sysInitParamTags;
}

bool SetCheckParam()
{
    int ret = CheckParam();
    return ret;
}
#endif

uint8_t GetTraceMode()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    return g_traceMode;
}

TraceErrorCode OpenTrace(const std::vector<std::string>& tagGroups)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    ClearFilterParam();
    if (g_traceMode != TraceMode::CLOSE) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: WRONG_TRACE_MODE, current trace mode: %{public}u.",
            static_cast<uint32_t>(g_traceMode));
        return WRONG_TRACE_MODE;
    }
    if (!IsTraceMounted(g_traceRootPath)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }

    if (IsHmKernel()) {
        TraceJsonParser& traceJsonParser = TraceJsonParser::Instance();
        const std::map<std::string, TraceTag>& allTags = traceJsonParser.GetAllTagInfos();
        if (allTags.size() == 0) {
            HILOG_ERROR(LOG_CORE, "OpenTrace: ParseTagInfo TAG_ERROR.");
            return TAG_ERROR;
        }
        auto iter = allTags.find("binder");
        if (iter != allTags.end()) {
            const std::vector<std::string> enablePath = iter->second.enablePath;
            auto noFilterEvents = GetNoFilterEvents(enablePath);
            if (noFilterEvents.size() > 0) {
                AddNoFilterEvents(noFilterEvents);
            }
        }
    }

    TraceErrorCode ret = HandleDefaultTraceOpen(tagGroups);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: failed.");
        return ret;
    }
    RefreshTraceVec(g_traceFileVec, TRACE_SNAPSHOT);
    std::vector<TraceFileInfo> cacheFileVec;
    RefreshTraceVec(cacheFileVec, TRACE_CACHE);
    ClearCacheTraceFileByDuration(cacheFileVec);
    g_sysInitParamTags = GetSysParamTags();
    g_traceMode = TraceMode::OPEN;
    HILOG_INFO(LOG_CORE, "OpenTrace: open by tag group success.");
    StartCpuBufferBalanceService();
    return ret;
}

TraceErrorCode OpenTrace(const std::string& args)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != TraceMode::CLOSE) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: WRONG_TRACE_MODE, current trace mode: %{public}u.",
            static_cast<uint32_t>(g_traceMode));
        return WRONG_TRACE_MODE;
    }

    if (!IsTraceMounted(g_traceRootPath)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }

    TraceJsonParser& traceJsonParser = TraceJsonParser::Instance();
    const std::map<std::string, TraceTag>& allTags = traceJsonParser.GetAllTagInfos();
    const std::map<std::string, std::vector<std::string>>& tagGroupTable = traceJsonParser.GetTagGroups();
    std::vector<std::string> traceFormats = traceJsonParser.GetBaseFmtPath();

    if (allTags.size() == 0 || tagGroupTable.size() == 0) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }
    // parse args
    TraceParams traceParams;
    if (!ParseArgs(args, traceParams, allTags, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }
    if (IsHmKernel()) {
        auto iter = allTags.find("binder");
        if (iter != allTags.end()) {
            const std::vector<std::string> enablePath = iter->second.enablePath;
            auto noFilterEvents = GetNoFilterEvents(enablePath);
            if (noFilterEvents.size() > 0) {
                AddNoFilterEvents(noFilterEvents);
            }
        }
    }

    TraceErrorCode ret = HandleTraceOpen(traceParams, allTags, tagGroupTable, traceFormats);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: open by args failed.");
        return FILE_ERROR;
    }
    g_sysInitParamTags = GetSysParamTags();
    g_traceMode = TraceMode::OPEN;
    HILOG_INFO(LOG_CORE, "OpenTrace: open by args success, args:%{public}s.", args.c_str());
    StartCpuBufferBalanceService();
    return ret;
}

TraceErrorCode CacheTraceOn(uint64_t totalFileSize, uint64_t sliceMaxDuration)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != TraceMode::OPEN) {
        HILOG_ERROR(LOG_CORE, "CacheTraceOn: WRONG_TRACE_MODE, current trace mode: %{public}u.",
            static_cast<uint32_t>(g_traceMode));
        return WRONG_TRACE_MODE;
    }
    if (!TraceDumpExecutor::GetInstance().PreCheckDumpTraceLoopStatus()) {
        HILOG_ERROR(LOG_CORE, "CacheTraceOn: cache trace is dumping now.");
        return WRONG_TRACE_MODE;
    }
    SetTotalFileSizeLimitAndSliceMaxDuration(totalFileSize, sliceMaxDuration);
    auto it = []() {
        ProcessCacheTask();
    };
    std::thread task(it);
    task.detach();
    HILOG_INFO(LOG_CORE, "Caching trace on.");
    g_traceMode |= TraceMode::CACHE;
    return SUCCESS;
}

TraceErrorCode CacheTraceOff()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != (TraceMode::OPEN | TraceMode::CACHE)) {
        HILOG_ERROR(LOG_CORE,
            "CacheTraceOff: WRONG_TRACE_MODE, current trace mode: %{public}u.", static_cast<uint32_t>(g_traceMode));
        return WRONG_TRACE_MODE;
    }
    TraceDumpExecutor::GetInstance().StopCacheTraceLoop();
    HILOG_INFO(LOG_CORE, "Caching trace off.");
    g_traceMode &= ~TraceMode::CACHE;
    return SUCCESS;
}

TraceRetInfo DumpTrace(uint32_t maxDuration, uint64_t utTraceEndTime)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    TraceRetInfo ret;
    ret.mode = g_traceMode;
    if (!CheckTraceDumpStatus(maxDuration, utTraceEndTime, ret)) {
        return ret;
    }
    FileAgeingUtils::HandleAgeing(g_traceFileVec, TraceDumpType::TRACE_SNAPSHOT);
    HILOG_INFO(LOG_CORE, "DumpTrace start, target duration is %{public}d, target endtime is (%{public}" PRIu64 ").",
        maxDuration, utTraceEndTime);
    SetDestTraceTimeAndDuration(maxDuration, utTraceEndTime);
    int32_t committedDuration =
        std::min(DEFAULT_FULL_TRACE_LENGTH, static_cast<int32_t>(g_utDestTraceEndTime - g_utDestTraceStartTime));
    if (UNEXPECTANTLY(IsCacheOn())) {
        GetFileInCache(ret);
        LoadDumpRet(ret, committedDuration);
        SanitizeRetInfo(ret);
        return ret;
    }

    ret.errorCode = SetTimeIntervalBoundary(maxDuration, utTraceEndTime);
    if (ret.errorCode != SUCCESS) {
        return ret;
    }
    g_firstPageTimestamp = UINT64_MAX;
    g_lastPageTimestamp = 0;

    ret.errorCode = ProcessDumpSync(ret);
    LoadDumpRet(ret, committedDuration);
    RestoreTimeIntervalBoundary();
    SanitizeRetInfo(ret);
    HILOG_INFO(LOG_CORE, "DumpTrace with time limit done.");
    return ret;
}

TraceRetInfo DumpTraceAsync(uint32_t maxDuration, uint64_t utTraceEndTime, int64_t fileSizeLimit,
    std::function<void(TraceRetInfo)> asyncCallback)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    TraceRetInfo ret;
    ret.mode = g_traceMode;
    if (!CheckTraceDumpStatus(maxDuration, utTraceEndTime, ret)) {
        return ret;
    }
    FileAgeingUtils::HandleAgeing(g_traceFileVec, TraceDumpType::TRACE_SNAPSHOT);

    HILOG_INFO(LOG_CORE, "DumpTraceAsync start, target duration is %{public}d, target endtime is %{public}" PRIu64 ".",
        maxDuration, utTraceEndTime);
    SetDestTraceTimeAndDuration(maxDuration, utTraceEndTime);
    int32_t committedDuration =
        std::min(DEFAULT_FULL_TRACE_LENGTH, static_cast<int32_t>(g_utDestTraceEndTime - g_utDestTraceStartTime));
    if (UNEXPECTANTLY(IsCacheOn())) {
        GetFileInCache(ret);
        LoadDumpRet(ret, committedDuration);
        SanitizeRetInfo(ret);
        if (asyncCallback != nullptr) {
            asyncCallback(ret);
        }
        return ret;
    }
    ret.errorCode = SetTimeIntervalBoundary(maxDuration, utTraceEndTime);
    if (ret.errorCode != SUCCESS) {
        return ret;
    }
    g_firstPageTimestamp = UINT64_MAX;
    g_lastPageTimestamp = 0;
    auto taskid = GetCurBootTime();
    std::unique_lock<std::mutex> retLck(g_traceRetAndCallbackMutex);
    g_callbacks[taskid] = asyncCallback;
    ret.errorCode = ProcessDumpAsync(taskid, fileSizeLimit, ret);
    if (ret.errorCode != TraceErrorCode::ASYNC_DUMP) {
        LoadDumpRet(ret, committedDuration);
        SanitizeRetInfo(ret);
        if (g_callbacks[taskid] != nullptr) {
            g_callbacks[taskid](ret);
        }
        g_callbacks.erase(taskid);
    } else {
        ret.errorCode = TraceErrorCode::SUCCESS;
        g_traceRetInfos[taskid] = ret;
    }
    retLck.unlock();
    RestoreTimeIntervalBoundary();
    HILOG_INFO(LOG_CORE, "DumpTraceAsync with time limit done. output file size: %{public}zu", ret.outputFiles.size());
    return ret;
}

TraceErrorCode RecordTraceOn()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    // check current trace status
    if (g_traceMode != TraceMode::OPEN) {
        HILOG_ERROR(LOG_CORE, "RecordTraceOn: WRONG_TRACE_MODE, current trace mode: %{public}u.",
            static_cast<uint32_t>(g_traceMode));
        return WRONG_TRACE_MODE;
    }
    if (!TraceDumpExecutor::GetInstance().PreCheckDumpTraceLoopStatus()) {
        HILOG_ERROR(LOG_CORE, "RecordTraceOn: record trace is dumping now.");
        return WRONG_TRACE_MODE;
    }
    auto it = []() {
        ProcessRecordTask();
    };
    std::thread task(it);
    task.detach();
    WriteCpuFreqTrace();
    HILOG_INFO(LOG_CORE, "Recording trace on.");
    g_traceMode |= TraceMode::RECORD;
    return SUCCESS;
}

TraceRetInfo RecordTraceOff()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    TraceRetInfo ret;
    // check current trace status
    if (!IsRecordOn()) {
        HILOG_ERROR(LOG_CORE, "RecordTraceOff: The current state is %{public}u, data exception.",
            static_cast<uint32_t>(g_traceMode));
        ret.errorCode = WRONG_TRACE_MODE;
        return ret;
    }

    ret.outputFiles = TraceDumpExecutor::GetInstance().StopDumpTraceLoop();
    ret.errorCode = SUCCESS;
    HILOG_INFO(LOG_CORE, "Recording trace off.");
    g_traceMode &= ~TraceMode::RECORD;
    return ret;
}

TraceErrorCode CloseTrace()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    ClearFilterParam();
    HILOG_INFO(LOG_CORE, "CloseTrace start.");
    if (g_traceMode == TraceMode::CLOSE) {
        HILOG_INFO(LOG_CORE, "Trace has already been closed.");
        return SUCCESS;
    }
    if (IsRecordOn() || IsCacheOn()) {
        TraceDumpExecutor::GetInstance().StopDumpTraceLoop();
    }
    g_traceMode = TraceMode::CLOSE;
    OHOS::system::SetParameter(TRACE_KEY_APP_PID, "-1");

    const std::map<std::string, TraceTag>& allTags = TraceJsonParser::Instance().GetAllTagInfos();

    if (allTags.size() == 0) {
        HILOG_ERROR(LOG_CORE, "CloseTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceInit(allTags);
    TruncateFile(TRACE_NODE);
    if (IsHmKernel()) {
        ClearNoFilterEvents();
    }
    HILOG_INFO(LOG_CORE, "CloseTrace done.");
    return SUCCESS;
}

TraceErrorCode SetTraceStatus(bool enable)
{
    HILOG_INFO(LOG_CORE, "SetTraceStatus %{public}d", enable);
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceRootPath.empty()) {
        if (!IsTraceMounted(g_traceRootPath)) {
            HILOG_ERROR(LOG_CORE, "SetTraceStatus: TRACE_NOT_SUPPORTED.");
            return TRACE_NOT_SUPPORTED;
        }
    }

    if (!SetTraceNodeStatus(TRACING_ON_NODE, enable)) {
        return WRITE_TRACE_INFO_ERROR;
    };

    return SUCCESS;
}

bool AddSymlinkXattr(const std::string& fileName)
{
    char realFilePath[PATH_MAX];
    if (!IsTraceFilePathLegal(fileName, realFilePath, sizeof(realFilePath))) {
        return false;
    }
    char valueStr[DEFAULT_XATTR_VALUE_SIZE];
    ssize_t len = TEMP_FAILURE_RETRY(getxattr(realFilePath, ATTR_NAME_LINK, valueStr, sizeof(valueStr)));
    if (len == -1) {
        std::string value = "1";
        if (TEMP_FAILURE_RETRY(setxattr(realFilePath, ATTR_NAME_LINK, value.c_str(), value.size(), 0)) == -1) {
            HILOG_ERROR(LOG_CORE, "AddSymlinkXattr: setxattr failed errno %{public}d", errno);
            return false;
        }
    } else {
        if (static_cast<uint32_t>(len) >= DEFAULT_XATTR_VALUE_SIZE) {
            HILOG_ERROR(LOG_CORE, "AddSymlinkXattr: value len not as expected");
            return false;
        }
        valueStr[len] = '\0';
        int val = 0;
        if (!StringToInt(valueStr, val)) {
            return false;
        }
        val++;
        std::string str = std::to_string(val);
        if (TEMP_FAILURE_RETRY(setxattr(realFilePath, ATTR_NAME_LINK, str.c_str(), str.size(), 0)) == -1) {
            HILOG_ERROR(LOG_CORE, "AddSymlinkXattr: modify xattr failed errno %{public}d", errno);
            return false;
        }
    }
    return true;
}

bool RemoveSymlinkXattr(const std::string& fileName)
{
    char realFilePath[PATH_MAX];
    if (!IsTraceFilePathLegal(fileName, realFilePath, sizeof(realFilePath))) {
        return false;
    }
    char valueStr[DEFAULT_XATTR_VALUE_SIZE];
    ssize_t len = TEMP_FAILURE_RETRY(getxattr(realFilePath, ATTR_NAME_LINK, valueStr, sizeof(valueStr)));
    if (len == -1) {
        HILOG_ERROR(LOG_CORE, "RemoveSymlinkXattr getxattr failed errno %{public}d", errno);
        return false;
    } else {
        if (static_cast<uint32_t>(len) >= DEFAULT_XATTR_VALUE_SIZE) {
            HILOG_ERROR(LOG_CORE, "RemoveSymlinkXattr: value len not as expected");
            return false;
        }
        valueStr[len] = '\0';
        int val = 0;
        if (!StringToInt(valueStr, val)) {
            return false;
        }
        if (val == 1) {
            if (TEMP_FAILURE_RETRY(removexattr(realFilePath, ATTR_NAME_LINK)) == -1) {
                HILOG_ERROR(LOG_CORE, "RemoveSymlinkXattr removexattr failed errno %{public}d", errno);
                return false;
            }
        } else if (val > 1) {
            val--;
            std::string str = std::to_string(val);
            if (TEMP_FAILURE_RETRY(setxattr(realFilePath, ATTR_NAME_LINK, str.c_str(), str.size(), 0)) == -1) {
                HILOG_ERROR(LOG_CORE, "RemoveSymlinkXattr: modify xattr failed errno %{public}d", errno);
                return false;
            }
        } else {
            HILOG_ERROR(LOG_CORE, "illegal value");
            return false;
        }
    }
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
