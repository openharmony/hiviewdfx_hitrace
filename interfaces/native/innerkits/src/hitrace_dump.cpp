/*
 * Copyright (C) 2023-2024 Huawei Device Co., Ltd.
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
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "dynamic_buffer.h"
#include "hitrace_meter.h"
#include "hitrace_option/hitrace_option.h"
#include "hilog/log.h"
#include "parameters.h"
#include "securec.h"
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

constexpr uint16_t MAGIC_NUMBER = 57161;
constexpr uint16_t VERSION_NUMBER = 2;
constexpr uint8_t FILE_RAW_TRACE = 0;
constexpr uint8_t HM_FILE_RAW_TRACE = 1;
constexpr uint64_t CACHE_TRACE_LOOP_SLEEP_TIME = 1;
constexpr int UNIT_TIME = 100000;
constexpr int ALIGNMENT_COEFFICIENT = 4;
constexpr int RECORD_LOOP_SLEEP = 1;

std::shared_ptr<ProductConfigJsonParser> g_ProductConfigParser
    = std::make_shared<ProductConfigJsonParser>("/sys_prod/etc/hiview/hitrace/hitrace_param.json");
const int DEFAULT_FILE_SIZE = 100 * 1024;
#ifdef HITRACE_UNITTEST
const int DEFAULT_CACHE_FILE_SIZE = 15 * 1024;
#else
const int DEFAULT_CACHE_FILE_SIZE = 150 * 1024;
#endif
int GetDefaultBufferSize()
{
    constexpr int defaultBufferSize = 12 * 1024;
#if defined(SNAPSHOT_TRACEBUFFER_SIZE) && (SNAPSHOT_TRACEBUFFER_SIZE != 0)
    constexpr int hmDefaultBufferSize = SNAPSHOT_TRACEBUFFER_SIZE;
#else
    constexpr int hmDefaultBufferSize = 144 * 1024;
#endif

    if (!IsHmKernel()) {
        return defaultBufferSize;
    }

    int bufferSize = g_ProductConfigParser->GetDefaultBufferSize();
    if (bufferSize != 0) {
        return bufferSize;
    }

    return hmDefaultBufferSize;
}
const int SAVED_CMDLINES_SIZE = 3072; // 3M
const int BYTE_PER_KB = 1024;
const int BYTE_PER_MB = 1024 * 1024;
constexpr uint64_t S_TO_NS = 1000000000;
constexpr uint64_t S_TO_MS = 1000;
constexpr int32_t MAX_RATIO_UNIT = 1000;
const int MAX_NEW_TRACE_FILE_LIMIT = 5;
const int JUDGE_FILE_EXIST = 10;  // Check whether the trace file exists every 10 times.
constexpr uint32_t DURATION_TOLERANCE = 100;
#if defined(SNAPSHOT_FILE_LIMIT) && (SNAPSHOT_FILE_LIMIT != 0)
const int SNAPSHOT_FILE_MAX_COUNT = SNAPSHOT_FILE_LIMIT;
#else
const int SNAPSHOT_FILE_MAX_COUNT = 20;
#endif

constexpr int DEFAULT_FULL_TRACE_LENGTH = 30;
constexpr uint64_t SNAPSHOT_MIN_REMAINING_SPACE = 300 * 1024 * 1024;     // 300M

const char* const KERNEL_VERSION = "KERNEL_VERSION: ";

struct alignas(ALIGNMENT_COEFFICIENT) TraceFileHeader {
    uint16_t magicNumber {MAGIC_NUMBER};
    uint8_t fileType {FILE_RAW_TRACE};
    uint16_t versionNumber {VERSION_NUMBER};
    uint32_t reserved {0};
};

enum ContentType : uint8_t {
    CONTENT_TYPE_DEFAULT = 0,
    CONTENT_TYPE_EVENTS_FORMAT = 1,
    CONTENT_TYPE_CMDLINES  = 2,
    CONTENT_TYPE_TGIDS = 3,
    CONTENT_TYPE_CPU_RAW = 4,
    CONTENT_TYPE_HEADER_PAGE = 30,
    CONTENT_TYPE_PRINTK_FORMATS = 31,
    CONTENT_TYPE_KALLSYMS = 32,
    CONTENT_TYPE_BASE_INFO = 33,
};

struct alignas(ALIGNMENT_COEFFICIENT) TraceFileContentHeader {
    uint8_t type = CONTENT_TYPE_DEFAULT;
    uint32_t length = 0;
};

struct PageHeader {
    uint64_t timestamp = 0;
    uint64_t size = 0;
    uint8_t overwrite = 0;
    uint8_t *startPos = nullptr;
    uint8_t *endPos = nullptr;
};

struct ChildProcessRet {
    uint8_t dumpStatus;
    uint64_t traceStartTime;
    uint64_t traceEndTime;
};

#ifndef PAGE_SIZE
constexpr size_t PAGE_SIZE = 4096;
#endif

const int BUFFER_SIZE = 256 * PAGE_SIZE; // 1M

std::atomic<bool> g_recordFlag(false);
std::atomic<bool> g_recordEnd(true);
std::atomic<bool> g_cacheFlag(false);
std::atomic<bool> g_cacheEnd(true);
std::mutex g_traceMutex;
std::mutex g_cacheTraceMutex;
std::mutex g_recordingOutputMutex;

bool g_serviceThreadIsStart = false;
uint64_t g_sysInitParamTags = 0;
uint8_t g_traceMode = TraceMode::CLOSE;
std::string g_traceRootPath;
uint8_t g_buffer[BUFFER_SIZE] = {0};
std::vector<std::pair<std::string, int>> g_traceFilesTable;
std::vector<FileWithTime> g_recordingOutput;
int g_outputFileSize = 0;
int g_newTraceFileLimit = 0;
int g_writeFileLimit = 0;
bool g_needGenerateNewTraceFile = false;
bool g_needLimitFileSize = true;
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
std::vector<TraceFileInfo> g_cacheFileVec{};

TraceParams g_currentTraceParams = {};
std::shared_ptr<TraceJsonParser> g_traceJsonParser = nullptr;
std::atomic<uint8_t> g_interruptDump(0);

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

uint64_t GetCurBootTime()
{
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    return static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
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

// Arch is 64bit when reserved = 0; Arch is 32bit when reserved = 1.
void GetArchWordSize(TraceFileHeader& header)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        header.reserved |= 0;
    } else if (sizeof(void*) == sizeof(uint32_t)) {
        header.reserved |= 1;
    }
}

void GetCpuNums(TraceFileHeader& header)
{
    const int maxCpuNums = 24;
    int cpuNums = GetCpuProcessors();
    if (cpuNums > maxCpuNums || cpuNums <= 0) {
        HILOG_ERROR(LOG_CORE, "error: cpu_number is %{public}d.", cpuNums);
        return;
    }
    header.reserved |= (static_cast<uint64_t>(cpuNums) << 1);
}

TraceFileHeader GenerateTraceHeaderContent()
{
    TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (IsHmKernel()) {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    return header;
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
    if (out.bad()) {
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
            SetTraceNodeStatus(it->second.enablePath[i], false);
        }
    }
    // close all user tags
    SetProperty(TRACE_TAG_ENABLE_FLAGS, std::to_string(0));

    // set buffer_size_kb 1
    WriteStrToFile("buffer_size_kb", "1");

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
                SetTraceNodeStatus(path, true);
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
        WriteStrToFile(traceClockPath, "boot"); //set default: boot
        return;
    }
    std::string allClocks = ReadFile(traceClockPath, g_traceRootPath);
    if (allClocks.find(clockType) == std::string::npos) {
        HILOG_ERROR(LOG_CORE, "SetClock: %{public}s is non-existent, set to boot", clockType.c_str());
        WriteStrToFile(traceClockPath, "boot"); // set default: boot
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
            WriteStrToFile(traceClockPath, clockType);
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
    WriteStrToFile(traceClockPath, "boot"); // set default: boot
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

    WriteStrToFile("current_tracer", "nop");
    WriteStrToFile("buffer_size_kb", traceParams.bufferSize);

    SetClock(traceParams.clockType);

    if (traceParams.isOverWrite == "1") {
        WriteStrToFile("options/overwrite", "1");
    } else {
        WriteStrToFile("options/overwrite", "0");
    }

    WriteStrToFile("saved_cmdlines_size", std::to_string(SAVED_CMDLINES_SIZE));
    WriteStrToFile("options/record-tgid", "1");
    WriteStrToFile("options/record-cmd", "1");
    return true;
}

bool CheckPage(uint8_t contentType, uint8_t* page)
{
    const int pageThreshold = PAGE_SIZE / 2;

    // Check raw_trace page size.
    if (contentType >= CONTENT_TYPE_CPU_RAW && !IsHmKernel()) {
        PageHeader *pageHeader = reinterpret_cast<PageHeader*>(&page);
        if (pageHeader->size < static_cast<uint64_t>(pageThreshold)) {
            return false;
        }
    }

    return true;
}

bool CheckFileExist(const std::string& outputFile)
{
    g_writeFileLimit++;
    if (g_writeFileLimit > JUDGE_FILE_EXIST) {
        g_writeFileLimit = 0;
        if (access(outputFile.c_str(), F_OK) != 0) {
            g_needGenerateNewTraceFile = true;
            HILOG_INFO(LOG_CORE, "CheckFileExist access file:%{public}s failed, errno: %{public}d.",
                outputFile.c_str(), errno);
            return false;
        }
    }
    return true;
}

TraceErrorCode SetTimeIntervalBoundary(int inputMaxDuration, uint64_t utTraceEndTime)
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

    uint64_t maxDuration = inputMaxDuration > 0 ? static_cast<uint64_t>(inputMaxDuration) + 1 : 0;
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

void GetFileSizeThresholdAndTraceTime(bool &isCpuRaw, uint8_t contentType, uint64_t &traceStartTime,
                                      uint64_t &traceEndTime, int &fileSizeThreshold)
{
    isCpuRaw = contentType >= CONTENT_TYPE_CPU_RAW && contentType < CONTENT_TYPE_HEADER_PAGE;
    if (isCpuRaw) {
        traceStartTime = g_traceStartTime;
        traceEndTime = g_traceEndTime;
        HILOG_INFO(LOG_CORE, "traceStartTime:(%{public}" PRIu64 "), traceEndTime:(%{public}" PRIu64 ").",
            traceStartTime, traceEndTime);
    }
    if (g_cacheFlag.load()) {
        fileSizeThreshold = DEFAULT_CACHE_FILE_SIZE * BYTE_PER_KB;
        return;
    }
    if (g_currentTraceParams.fileSize != 0) {
        fileSizeThreshold = g_currentTraceParams.fileSize * BYTE_PER_KB;
    }
}

bool IsWriteFileOverflow(const bool isCpuRaw, const int &outputFileSize, const ssize_t& writeLen,
                         const int& fileSizeThreshold)
{
    // attention: we only check file size threshold in CMD_MODE
    if (!isCpuRaw || (!IsRecordOn() && !IsCacheOn()) || !g_needLimitFileSize) {
        return false;
    }
    if (outputFileSize + writeLen + static_cast<int>(sizeof(TraceFileContentHeader)) >= fileSizeThreshold) {
        HILOG_ERROR(LOG_CORE, "Failed to write, current round write file size exceeds the file size limit.");
        return true;
    }
    if (writeLen > INT_MAX - BUFFER_SIZE) {
        HILOG_ERROR(LOG_CORE, "Failed to write, write file length is nearly overflow.");
        return true;
    }
    return false;
}

bool WriteFile(uint8_t contentType, const std::string& src, int outFd, const std::string& outputFile)
{
    std::string srcPath = CanonicalizeSpecPath(src.c_str());
    int srcFd = open(srcPath.c_str(), O_RDONLY | O_NONBLOCK);
    if (srcFd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteFile: open %{public}s failed.", src.c_str());
        return false;
    }
    if (!CheckFileExist(outputFile)) {
        HILOG_ERROR(LOG_CORE, "need generate new trace file, old file:%{public}s.", outputFile.c_str());
        close(srcFd);
        return false;
    }
    struct TraceFileContentHeader contentHeader;
    contentHeader.type = contentType;
    write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    ssize_t writeLen = 0;
    int count = 0;
    const int maxCount = 2;

    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = std::numeric_limits<uint64_t>::max();
    int fileSizeThreshold = DEFAULT_FILE_SIZE * BYTE_PER_KB;
    bool isCpuRaw = false;
    GetFileSizeThresholdAndTraceTime(isCpuRaw, contentType, traceStartTime, traceEndTime, fileSizeThreshold);
    bool printFirstPageTime = false;
    while (true) {
        int bytes = 0;
        bool endFlag = false;
        /* Write 1M at a time */
        while (bytes <= (BUFFER_SIZE - static_cast<int>(PAGE_SIZE))) {
            ssize_t readBytes = TEMP_FAILURE_RETRY(read(srcFd, g_buffer + bytes, PAGE_SIZE));
            if (readBytes == 0) {
                endFlag = true;
                HILOG_DEBUG(LOG_CORE, "WriteFile: read %{public}s end.", src.c_str());
                break;
            } else if (readBytes < 0) {
                endFlag = true;
                HILOG_DEBUG(LOG_CORE, "WriteFile: read %{public}s, data size: %{public}zd failed, errno: %{public}d.",
                    src.c_str(), readBytes, errno);
                break;
            }

            uint64_t pageTraceTime = 0;
            if (memcpy_s(&pageTraceTime, sizeof(uint64_t), g_buffer + bytes, sizeof(uint64_t)) != EOK) {
                HILOG_ERROR(LOG_CORE, "Failed to memcpy g_buffer to pageTraceTime.");
                break;
            }
            if (traceEndTime < pageTraceTime) {
                endFlag = true;
                bytes += (printFirstPageTime == true ? readBytes : 0);
                HILOG_INFO(LOG_CORE,
                    "Current pageTraceTime:(%{public}" PRIu64 ") is larger than traceEndTime:(%{public}" PRIu64 ")",
                    pageTraceTime, traceEndTime);
                break;
            }
            if (pageTraceTime < traceStartTime) {
                continue;
            }
            if (isCpuRaw) {
                g_lastPageTimestamp = std::max(pageTraceTime, g_lastPageTimestamp);
                if (UNEXPECTANTLY(!printFirstPageTime)) {
                    HILOG_INFO(LOG_CORE, "first page trace time:(%{public}" PRIu64 ")", pageTraceTime);
                    printFirstPageTime = true;
                    g_firstPageTimestamp = std::min(g_firstPageTimestamp, pageTraceTime);
                }
            }

            if (CheckPage(contentType, g_buffer + bytes) == false) {
                count++;
            }
            bytes += readBytes;
            if (count >= maxCount) {
                endFlag = true;
                break;
            }
        }

        ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, g_buffer, bytes));
        if (writeRet < 0) {
            HILOG_WARN(LOG_CORE, "WriteFile Fail, errno: %{public}d.", errno);
        } else {
            if (writeRet != static_cast<ssize_t>(bytes)) {
                HILOG_WARN(LOG_CORE, "Failed to write full info, writeLen: %{public}zd, FullLen: %{public}d.",
                    writeRet, bytes);
            }
            writeLen += writeRet;
        }

        if (IsWriteFileOverflow(isCpuRaw, g_outputFileSize, writeLen, fileSizeThreshold)) {
            HILOG_WARN(LOG_CORE, "Write file over flow, fileZise: %{public}d, writeLen: %{public}zd, "
                "FullLen: %{public}d.", g_outputFileSize, writeRet, bytes);
            break;
        }

        if (endFlag == true) {
            break;
        }
    }
    contentHeader.length = static_cast<uint32_t>(writeLen);
    uint32_t offset = contentHeader.length + sizeof(contentHeader);
    off_t pos = lseek(outFd, 0, SEEK_CUR);
    lseek(outFd, pos - offset, SEEK_SET);
    write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    lseek(outFd, pos, SEEK_SET);
    close(srcFd);
    if (isCpuRaw) {
        if (writeLen > 0) {
            g_dumpStatus = TraceErrorCode::SUCCESS;
        } else if (g_dumpStatus == TraceErrorCode::UNSET) {
            g_dumpStatus = TraceErrorCode::OUT_OF_TIME;
        }
    }
    g_outputFileSize += static_cast<int>(offset);
    g_needGenerateNewTraceFile = false;
    HILOG_INFO(LOG_CORE, "WriteFile end, path: %{public}s, byte: %{public}zd. g_writeFileLimit: %{public}d",
        src.c_str(), writeLen, g_writeFileLimit);
    return true;
}

void WriteEventFile(std::string& srcPath, int outFd)
{
    uint8_t buffer[PAGE_SIZE] = {0};
    std::string srcSpecPath = CanonicalizeSpecPath(srcPath.c_str());
    int srcFd = open(srcSpecPath.c_str(), O_RDONLY);
    if (srcFd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteEventFile: open %{public}s failed.", srcPath.c_str());
        return;
    }
    int64_t readLen = 0;
    do {
        int64_t len = read(srcFd, buffer, PAGE_SIZE);
        if (len <= 0) {
            break;
        }
        write(outFd, buffer, len);
        readLen += len;
    } while (true);
    close(srcFd);
    HILOG_INFO(LOG_CORE, "WriteEventFile end, path: %{public}s, data size: (%{public}" PRIu64 ").",
        srcPath.c_str(), static_cast<uint64_t>(readLen));
}

bool WriteEventsFormat(int outFd, const std::string& outputFile)
{
    const std::string savedEventsFormatPath = TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT;
    if (access(savedEventsFormatPath.c_str(), F_OK) != -1) {
        return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, savedEventsFormatPath, outFd, outputFile);
    }

    // write all trace formats into TRACE_SAVED_EVENTS_FORMAT file.
    int fd = open(savedEventsFormatPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (fd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: open %{public}s failed.", savedEventsFormatPath.c_str());
        return false;
    }
    if (g_traceJsonParser == nullptr) {
        g_traceJsonParser = std::make_shared<TraceJsonParser>();
    }
    if (!g_traceJsonParser->ParseTraceJson(PARSE_TRACE_FORMAT_INFO)) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: Failed to parse trace format infos.");
        close(fd);
        return false;
    }
    auto allTags = g_traceJsonParser->GetAllTagInfos();
    auto traceFormats = g_traceJsonParser->GetBaseFmtPath();
    for (auto& tag : allTags) {
        for (auto& fmt : tag.second.formatPath) {
            traceFormats.emplace_back(fmt);
        }
    }
    for (auto& traceFmt : traceFormats) {
        std::string srcPath = g_traceRootPath + traceFmt;
        if (access(srcPath.c_str(), R_OK) != -1) {
            WriteEventFile(srcPath, fd);
        }
    }
    close(fd);
    HILOG_INFO(LOG_CORE, "WriteEventsFormat end. path: %{public}s.", savedEventsFormatPath.c_str());
    return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, savedEventsFormatPath, outFd, outputFile);
}

bool WriteHeaderPage(int outFd, const std::string& outputFile)
{
    if (IsHmKernel()) {
        return true;
    }
    std::string headerPagePath = GetFilePath("events/header_page", g_traceRootPath);
    return WriteFile(CONTENT_TYPE_HEADER_PAGE, headerPagePath, outFd, outputFile);
}

bool WritePrintkFormats(int outFd, const std::string &outputFile)
{
    if (IsHmKernel()) {
        return true;
    }
    std::string printkFormatPath = GetFilePath("printk_formats", g_traceRootPath);
    return WriteFile(CONTENT_TYPE_PRINTK_FORMATS, printkFormatPath, outFd, outputFile);
}

bool WriteKallsyms(int outFd)
{
    /* not implement in hmkernel */
    if (IsHmKernel()) {
        return true;
    }
    /* not implement in linux */
    return true;
}

bool HmWriteCpuRawInner(int outFd, const std::string& outputFile)
{
    uint8_t type = CONTENT_TYPE_CPU_RAW;
    std::string src = g_traceRootPath + "/trace_pipe_raw";

    if (!WriteFile(type, src, outFd, outputFile)) {
        return false;
    }

    if (g_dumpStatus) {
        HILOG_ERROR(LOG_CORE, "HmWriteCpuRawInner failed, errno: %{public}d.", static_cast<int>(g_dumpStatus));
        return false;
    }

    return true;
}

bool WriteCpuRawInner(int outFd, const std::string& outputFile)
{
    int cpuNums = GetCpuProcessors();
    uint8_t type = CONTENT_TYPE_CPU_RAW;
    for (int i = 0; i < cpuNums; i++) {
        std::string src = g_traceRootPath + "per_cpu/cpu" + std::to_string(i) + "/trace_pipe_raw";
        if (!WriteFile(static_cast<uint8_t>(type + i), src, outFd, outputFile)) {
            return false;
        }
    }
    if (g_dumpStatus) {
        HILOG_ERROR(LOG_CORE, "WriteCpuRawInner failed, errno: %{public}d.", static_cast<int>(g_dumpStatus));
        return false;
    }
    return true;
}

bool WriteCpuRaw(int outFd, const std::string& outputFile)
{
    if (!IsHmKernel()) {
        return WriteCpuRawInner(outFd, outputFile);
    } else {
        return HmWriteCpuRawInner(outFd, outputFile);
    }
}

bool WriteCmdlines(int outFd, const std::string& outputFile)
{
    std::string cmdlinesPath = GetFilePath("saved_cmdlines", g_traceRootPath);
    return WriteFile(CONTENT_TYPE_CMDLINES, cmdlinesPath, outFd, outputFile);
}

bool WriteTgids(int outFd, const std::string& outputFile)
{
    std::string tgidsPath = GetFilePath("saved_tgids", g_traceRootPath);
    return WriteFile(CONTENT_TYPE_TGIDS, tgidsPath, outFd, outputFile);
}

ssize_t WriteKernelVersion(int outFd)
{
    static std::string kernelVersion = KERNEL_VERSION + GetKernelVersion() + "\n";
    ssize_t writeRet = write(outFd, kernelVersion.data(), kernelVersion.size());
    if (writeRet < 0) {
        HILOG_WARN(LOG_CORE, "WriteKernelVersion fail, errno: %{public}d.", errno);
        return 0;
    } else {
        return writeRet;
    }
}

bool WriteBaseInfo(int outFd)
{
    struct TraceFileContentHeader contentHeader;
    contentHeader.type = CONTENT_TYPE_BASE_INFO;
    ssize_t writeRet = write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    if (writeRet < 0) {
        HILOG_WARN(LOG_CORE, "Write BaseInfo contentHeader fail, errno: %{public}d.", errno);
        return false;
    }
    contentHeader.length += static_cast<uint32_t>(WriteKernelVersion(outFd));
    uint32_t offset = contentHeader.length + sizeof(contentHeader);
    off_t pos = lseek(outFd, 0, SEEK_CUR);
    lseek(outFd, pos - offset, SEEK_SET);
    write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    lseek(outFd, pos, SEEK_SET);
    g_outputFileSize += static_cast<int>(offset);
    return true;
}

bool GenerateNewFile(int& outFd, std::string& outPath, const TRACE_TYPE traceType)
{
    if (access(outPath.c_str(), F_OK) == 0) {
        return true;
    }
    std::string outputFileName = GenerateTraceFileName(traceType);
    outPath = CanonicalizeSpecPath(outputFileName.c_str());
    outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (outFd < 0) {
        g_newTraceFileLimit++;
        HILOG_ERROR(LOG_CORE, "open %{public}s failed, errno: %{public}d.", outPath.c_str(), errno);
    }
    if (g_newTraceFileLimit > MAX_NEW_TRACE_FILE_LIMIT) {
        HILOG_ERROR(LOG_CORE, "create new trace file %{public}s limited.", outPath.c_str());
        return false;
    }
    g_needGenerateNewTraceFile = true;
    return true;
}

bool SetFileInfo(const std::string outPath, const uint64_t& firstPageTimestamp,
    const uint64_t& lastPageTimestamp, TraceFileInfo& traceFileInfo)
{
    std::string newFileName;
    if (!RenameTraceFile(outPath, newFileName, firstPageTimestamp, lastPageTimestamp)) {
        HILOG_INFO(LOG_CORE, "rename failed, outPath: %{public}s.", outPath.c_str());
        return false;
    }
    uint64_t traceFileSize = 0;
    if (!GetFileSize(newFileName, traceFileSize)) {
        return false;
    }
    traceFileInfo.filename = newFileName;
    traceFileInfo.traceStartTime = ConvertPageTraceTimeToUtTimeMs(firstPageTimestamp);
    traceFileInfo.traceEndTime = ConvertPageTraceTimeToUtTimeMs(lastPageTimestamp);
    traceFileInfo.fileSize = traceFileSize;
    return true;
}

int32_t GetTraceFileFromVec(std::vector<std::string>& outputFiles, const uint64_t& inputTraceStartTime,
    const uint64_t& inputTraceEndTime, std::vector<TraceFileInfo>& fileVec)
{
    int32_t coverDuration = 0;
    uint64_t utTargetStartTimeMs = inputTraceStartTime * S_TO_MS;
    uint64_t utTargetEndTimeMs = inputTraceEndTime * S_TO_MS;
    for (auto it = fileVec.begin(); it != fileVec.end(); it++) {
        HILOG_INFO(LOG_CORE, "GetTraceFileFromVec: %{public}s, [(%{public}" PRIu64 ", %{public}" PRIu64 "].",
            it->filename.c_str(), it->traceStartTime, it->traceEndTime);
        if (((it->traceEndTime >= utTargetStartTimeMs && it->traceStartTime <= utTargetEndTimeMs)) &&
            (it->traceEndTime - it->traceStartTime < 2000 * S_TO_MS)) { // 2000 : max trace duration 2000s
            outputFiles.push_back(it->filename);
            coverDuration += static_cast<int32_t>(std::min(it->traceEndTime, utTargetEndTimeMs + DURATION_TOLERANCE) -
                std::max(it->traceStartTime, utTargetStartTimeMs - DURATION_TOLERANCE));
        }
    }
    return coverDuration;
}

std::string RenameCacheFile(const std::string& cacheFile)
{
    std::string fileName = cacheFile.substr(cacheFile.find_last_of("/") + 1);
    std::string cacheFileSuffix = "cache_";
    std::string::size_type pos = fileName.find(cacheFileSuffix);
    if (pos == std::string::npos) {
        return cacheFile;
    }
    std::string dirPath = cacheFile.substr(0, cacheFile.find_last_of("/") + 1);
    std::string newFileName = fileName.substr(pos + cacheFileSuffix.size());
    std::string newFilePath = dirPath + newFileName;
    if (rename(cacheFile.c_str(), newFilePath.c_str()) != 0) {
        HILOG_ERROR(LOG_CORE, "rename %{public}s to %{public}s failed, errno: %{public}d.",
            cacheFile.c_str(), newFilePath.c_str(), errno);
        return cacheFile;
    }
    HILOG_INFO(LOG_CORE, "rename %{public}s to %{public}s success.", cacheFile.c_str(), newFilePath.c_str());
    return newFilePath;
}

int32_t SearchTraceFiles(std::vector<std::string>& outputFiles, const uint64_t& inputTraceStartTime,
    const uint64_t& inputTraceEndTime)
{
    if (g_traceJsonParser == nullptr) {
        g_traceJsonParser = std::make_shared<TraceJsonParser>();
    }
    if (!g_traceJsonParser->ParseTraceJson(TRACE_SNAPSHOT_FILE_AGE)) {
        HILOG_WARN(LOG_CORE, "ProcessDump: Failed to parse TRACE_SNAPSHOT_FILE_AGE.");
    }
    if ((!IsRootVersion()) || g_ProductConfigParser->GetRootAgeingStatus() == ConfigStatus::ENABLE ||
        (g_ProductConfigParser->GetRootAgeingStatus() == ConfigStatus::UNKNOWN &&
        g_traceJsonParser->GetSnapShotFileAge())) {
        DelSnapshotTraceFile(SNAPSHOT_FILE_MAX_COUNT, g_traceFileVec, g_ProductConfigParser->GetSnapshotFileSizeKb());
    }
    HILOG_INFO(LOG_CORE, "target trace time: [%{public}" PRIu64 ", %{public}" PRIu64 "].",
        inputTraceStartTime, inputTraceEndTime);
    uint64_t curTime = GetCurUnixTimeMs();
    HILOG_INFO(LOG_CORE, "current time: %{public}" PRIu64 ".", curTime);
    int32_t coverDuration = 0;
    coverDuration += GetTraceFileFromVec(outputFiles, inputTraceStartTime, inputTraceEndTime, g_traceFileVec);
    std::vector<std::string> outputCacheFiles;
    coverDuration += GetTraceFileFromVec(outputCacheFiles, inputTraceStartTime, inputTraceEndTime, g_cacheFileVec);
    for (const auto& file: outputCacheFiles) {
        std::string newFile = RenameCacheFile(file);
        for (auto it = g_cacheFileVec.begin(); it != g_cacheFileVec.end();) {
            if (it->filename == file) {
                it->filename = newFile;
                g_traceFileVec.push_back(*it);
                it = g_cacheFileVec.erase(it);
                break;
            } else {
                ++it;
            }
        }
        outputFiles.emplace_back(newFile);
        HILOG_INFO(LOG_CORE, "dumptrace cache file is %{public}s, new file is %{public}s.",
            file.c_str(), newFile.c_str());
    }
    return coverDuration;
}

bool CacheTraceLoop(const std::string &outputFileName)
{
    int fileSizeThreshold = DEFAULT_CACHE_FILE_SIZE * BYTE_PER_KB;
    g_firstPageTimestamp = UINT64_MAX;
    g_lastPageTimestamp = 0;
    g_outputFileSize = 0;
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (outFd < 0) {
        HILOG_ERROR(LOG_CORE, "open %{public}s failed, errno: %{public}d.", outPath.c_str(), errno);
        return false;
    }
    MarkClockSync(g_traceRootPath);
    struct TraceFileHeader header = GenerateTraceHeaderContent();
    uint64_t sliceDuration = 0;
    std::lock_guard<std::mutex> lock(g_cacheTraceMutex);
    do {
        g_needGenerateNewTraceFile = false;
        ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, reinterpret_cast<char *>(&header), sizeof(header)));
        if (writeRet < 0) {
            HILOG_WARN(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
                strerror(errno), sizeof(header));
            close(outFd);
            return false;
        }
        WriteBaseInfo(outFd);
        WriteEventsFormat(outFd, outPath);
        while (g_cacheFlag.load()) {
            if (g_outputFileSize > fileSizeThreshold) {
                break;
            }
            struct timespec bts = {0, 0};
            clock_gettime(CLOCK_BOOTTIME, &bts);
            uint64_t startTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
            sleep(CACHE_TRACE_LOOP_SLEEP_TIME);
            if (!WriteCpuRaw(outFd, outPath)) {
                break;
            }
            clock_gettime(CLOCK_BOOTTIME, &bts);
            uint64_t endTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
            uint64_t timeDiff = (endTime - startTime) / S_TO_NS;
            sliceDuration += timeDiff;
            if (sliceDuration >= g_sliceMaxDuration || g_interruptDump.load() == 1) {
                sliceDuration = 0;
                break;
            }
        }
        WriteCmdlines(outFd, outPath);
        WriteTgids(outFd, outPath);
        WriteHeaderPage(outFd, outPath);
        WritePrintkFormats(outFd, outPath);
        WriteKallsyms(outFd);
        if (!GenerateNewFile(outFd, outPath, TRACE_CACHE)) {
            HILOG_INFO(LOG_CORE, "CacheTraceLoop access file:%{public}s failed, errno: %{public}d.",
                outPath.c_str(), errno);
            close(outFd);
            return false;
        }
    } while (g_needGenerateNewTraceFile);
    close(outFd);
    TraceFileInfo traceFileInfo;
    if (!SetFileInfo(outPath, g_firstPageTimestamp, g_lastPageTimestamp, traceFileInfo)) {
        RemoveFile(outPath);
        return false;
    }
    g_cacheFileVec.push_back(traceFileInfo);
    return true;
}

void ProcessCacheTask()
{
    const std::string threadName = "CacheTraceTask";
    prctl(PR_SET_NAME, threadName.c_str());
    while (g_cacheFlag.load()) {
        std::string outputFileName = GenerateTraceFileName(TRACE_CACHE);
        if (CacheTraceLoop(outputFileName)) {
            std::lock_guard<std::mutex> cacheLock(g_cacheTraceMutex);
            ClearCacheTraceFileBySize(g_cacheFileVec, g_totalFileSizeLimit);
            HILOG_INFO(LOG_CORE, "ProcessCacheTask: save cache file.");
        } else {
            break;
        }
    }
    g_cacheEnd.store(true);
    HILOG_INFO(LOG_CORE, "ProcessCacheTask: trace cache thread exit.");
}

bool RecordTraceLoop(const std::string& outputFileName, bool isLimited)
{
    g_outputFileSize = 0;
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (outFd < 0) {
        HILOG_ERROR(LOG_CORE, "open %{public}s failed, errno: %{public}d.", outPath.c_str(), errno);
        return false;
    }
    int fileSizeThreshold = DEFAULT_FILE_SIZE * BYTE_PER_KB;
    if (g_currentTraceParams.fileSize != 0) {
        fileSizeThreshold = g_currentTraceParams.fileSize * BYTE_PER_KB;
    }
    MarkClockSync(g_traceRootPath);
    struct TraceFileHeader header = GenerateTraceHeaderContent();
    do {
        g_needGenerateNewTraceFile = false;
        ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, reinterpret_cast<char *>(&header), sizeof(header)));
        if (writeRet < 0) {
            HILOG_WARN(LOG_CORE, "Failed to write trace file header, errno: %{public}s.", strerror(errno));
            close(outFd);
            return false;
        }
        WriteBaseInfo(outFd);
        WriteEventsFormat(outFd, outPath);
        while (g_recordFlag.load()) {
            if (isLimited && g_outputFileSize > fileSizeThreshold) {
                break;
            }
            sleep(RECORD_LOOP_SLEEP);
            g_traceEndTime = GetCurBootTime();
            if (!WriteCpuRaw(outFd, outPath)) {
                break;
            }
        }
        WriteCmdlines(outFd, outPath);
        WriteTgids(outFd, outPath);
        WriteHeaderPage(outFd, outPath);
        WritePrintkFormats(outFd, outPath);
        WriteKallsyms(outFd);
        if (!GenerateNewFile(outFd, outPath, TRACE_RECORDING)) {
            HILOG_INFO(LOG_CORE, "RecordTraceLoop access file:%{public}s failed, errno: %{public}d.",
                outPath.c_str(), errno);
            close(outFd);
            return false;
        }
    } while (g_needGenerateNewTraceFile);
    close(outFd);
    g_traceEndTime = std::numeric_limits<uint64_t>::max();
    return true;
}

/**
 * read trace data loop
 * g_recordFlag: true = open，false = close
 * g_recordEnd: true = end，false = not end
 * if user has own output file, Output all data to the file specified by the user;
 * if not, Then place all the result files in /data/log/hitrace/ and package them once in 96M.
*/
void ProcessRecordTask()
{
    {
        std::lock_guard<std::mutex> lock(g_recordingOutputMutex);
        g_recordingOutput.clear();
        GetTraceFilesInDir(g_recordingOutput, TRACE_RECORDING);
        if ((!IsRootVersion()) || g_ProductConfigParser->GetRootAgeingStatus() == ConfigStatus::ENABLE) {
            // clear old record file before record tracing start.
            DelOldRecordTraceFile(g_recordingOutput, g_currentTraceParams.fileLimit,
                                  g_ProductConfigParser->GetRecordFileSizeKb());
        }
    }
    const std::string threadName = "TraceDumpTask";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "ProcessRecordTask: trace dump thread start.");

    // if input filesize = 0, trace file should not be cut in root version.
    if (g_currentTraceParams.fileSize == 0 && IsRootVersion()) {
        g_needLimitFileSize = false;
        std::string outputFileName = g_currentTraceParams.outputFile.empty() ?
                                     GenerateTraceFileName(TRACE_RECORDING) : g_currentTraceParams.outputFile;
        if (RecordTraceLoop(outputFileName, g_needLimitFileSize)) {
            std::lock_guard<std::mutex> lock(g_recordingOutputMutex);
            g_recordingOutput.emplace_back(outputFileName);
        }
        g_recordEnd.store(true);
        g_needLimitFileSize = true;
        return;
    }

    while (g_recordFlag.load()) {
        if (!IsRootVersion() || g_ProductConfigParser->GetRootAgeingStatus() == ConfigStatus::ENABLE) {
            std::lock_guard<std::mutex> lock(g_recordingOutputMutex);
            ClearOldTraceFile(g_recordingOutput, g_currentTraceParams.fileLimit,
                g_ProductConfigParser->GetRecordFileSizeKb());
        }
        // Generate file name
        std::string outputFileName = GenerateTraceFileName(TRACE_RECORDING);
        if (RecordTraceLoop(outputFileName, true)) {
            std::lock_guard<std::mutex> lock(g_recordingOutputMutex);
            g_recordingOutput.emplace_back(outputFileName);
        } else {
            break;
        }
    }
    HILOG_INFO(LOG_CORE, "ProcessRecordTask: trace dump thread exit.");
    g_recordEnd.store(true);
}

bool ReadRawTrace(std::string& outputFileName)
{
    // read trace data from /per_cpu/cpux/trace_pipe_raw
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (outFd < 0) {
        return false;
    }
    struct TraceFileHeader header = GenerateTraceHeaderContent();
    ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, reinterpret_cast<char*>(&header), sizeof(header)));
    if (writeRet < 0) {
        HILOG_WARN(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
            strerror(errno), sizeof(header));
        close(outFd);
        return false;
    }

    if (WriteBaseInfo(outFd) && WriteEventsFormat(outFd, outPath) && WriteCpuRaw(outFd, outPath) &&
        WriteCmdlines(outFd, outPath) && WriteTgids(outFd, outPath) &&
        WriteHeaderPage(outFd, outPath) && WritePrintkFormats(outFd, outPath) &&
        WriteKallsyms(outFd)) {
        fsync(outFd);
        close(outFd);
        return true;
    }
    HILOG_ERROR(LOG_CORE, "ReadRawTrace failed.");
    fsync(outFd);
    close(outFd);
    return false;
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
    }
}

bool EpollWaitforChildProcess(pid_t& pid, int& pipefd)
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
    constexpr int waitTimeoutMs = 10000; // 10000ms = 10s
    int numEvents = TEMP_FAILURE_RETRY(epoll_wait(epollfd, events, 1, waitTimeoutMs));
    if (numEvents <= 0) {
        if (numEvents == -1) {
            HILOG_ERROR(LOG_CORE, "epoll_wait error, error: (%{public}s).", strerror(errno));
        } else {
            HILOG_ERROR(LOG_CORE, "epoll_wait timeout.");
        }
        if (waitpid(pid, nullptr, WNOHANG) <= 0) {
            HILOG_ERROR(LOG_CORE, "kill timeout child process.");
            kill(pid, SIGUSR1);
        }
        close(epollfd);
        return false;
    }
    ChildProcessRet retVal;
    read(pipefd, &retVal, sizeof(retVal));
    g_dumpStatus = retVal.dumpStatus;
    g_firstPageTimestamp = retVal.traceStartTime;
    g_lastPageTimestamp = retVal.traceEndTime;

    close(epollfd);
    if (waitpid(pid, nullptr, 0) <= 0) {
        HILOG_ERROR(LOG_CORE, "wait HitraceDump(%{public}d) exit failed, errno: (%{public}d)", pid, errno);
    }
    return true;
}

TraceErrorCode HandleDumpResult(TraceRetInfo& traceRetInfo, std::string& reOutPath)
{
    {
        std::lock_guard<std::mutex> lock(g_cacheTraceMutex);
        traceRetInfo.coverDuration +=
            SearchTraceFiles(traceRetInfo.outputFiles, g_utDestTraceStartTime, g_utDestTraceEndTime);
    }
    if (g_dumpStatus) { // trace generation error
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
        if (!SetFileInfo(reOutPath, g_firstPageTimestamp, g_lastPageTimestamp, traceFileInfo)) {
            // trace rename error
            HILOG_ERROR(LOG_CORE, "SetFileInfo: set %{public}s info failed.", reOutPath.c_str());
            RemoveFile(reOutPath);
        } else { // success
            g_traceFileVec.push_back(traceFileInfo);
            traceRetInfo.outputFiles.push_back(traceFileInfo.filename);
            traceRetInfo.coverDuration +=
                static_cast<int32_t>(traceFileInfo.traceEndTime - traceFileInfo.traceStartTime);
        }
    }

    if (traceRetInfo.outputFiles.empty()) {
        return (g_dumpStatus != 0) ? static_cast<TraceErrorCode>(g_dumpStatus) : TraceErrorCode::FILE_ERROR;
    }
    return TraceErrorCode::SUCCESS;
}

TraceErrorCode ProcessDump(TraceRetInfo& traceRetInfo)
{
    if (GetRemainingSpace("/data") <= SNAPSHOT_MIN_REMAINING_SPACE) {
        HILOG_ERROR(LOG_CORE, "ProcessDump: remaining space not enough");
        return TraceErrorCode::FILE_ERROR;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        HILOG_ERROR(LOG_CORE, "pipe creation error.");
        return TraceErrorCode::PIPE_CREATE_ERROR;
    }

    std::string outputFileName = GenerateTraceFileName(TRACE_SNAPSHOT);
    std::string reOutPath = CanonicalizeSpecPath(outputFileName.c_str());
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
        MarkClockSync(g_traceRootPath);
        constexpr int waitTime = 10000; // 10ms
        usleep(waitTime);
        if (ReadRawTrace(reOutPath)) {
            g_dumpStatus = TraceErrorCode::SUCCESS;
        }
        HILOG_DEBUG(LOG_CORE, "%{public}s exit.", processName.c_str());
        ChildProcessRet retVal;
        retVal.dumpStatus = g_dumpStatus;
        retVal.traceStartTime = g_firstPageTimestamp;
        retVal.traceEndTime = g_lastPageTimestamp;
        write(pipefd[1], &retVal, sizeof(retVal));
        _exit(EXIT_SUCCESS);
    } else {
        close(pipefd[1]);
    }

    if (!EpollWaitforChildProcess(pid, pipefd[0])) {
        close(pipefd[0]);
        return TraceErrorCode::EPOLL_WAIT_ERROR;
    }
    
    close(pipefd[0]);
    return HandleDumpResult(traceRetInfo, reOutPath);
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
            WriteStrToFile(path, std::to_string(result[i]));
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
    if (g_traceJsonParser == nullptr) {
        g_traceJsonParser = std::make_shared<TraceJsonParser>();
    }
    if (!g_traceJsonParser->ParseTraceJson(PARSE_ALL_INFO)) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: Failed to parse trace tag total infos.");
        return FILE_ERROR;
    }
    auto allTags = g_traceJsonParser->GetAllTagInfos();
    auto tagGroupTable = g_traceJsonParser->GetTagGroups();
    auto tagFmts = g_traceJsonParser->GetBaseFmtPath();
    auto custBufSz = g_traceJsonParser->GetSnapShotBufSzKb();

    if (tagGroups.size() == 0 || !CheckTagGroup(tagGroups, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceParams defaultTraceParams;
    defaultTraceParams.tagGroups = tagGroups;
    // attention: the buffer size value in the configuration file is preferred to set.
    if (custBufSz > 0) {
        defaultTraceParams.bufferSize = std::to_string(custBufSz);
    } else {
        int traceBufSz = GetDefaultBufferSize();
        defaultTraceParams.bufferSize = std::to_string(traceBufSz);
    }
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

void SetDestTraceTimeAndDuration(int maxDuration, const uint64_t& utTraceEndTime)
{
    if (utTraceEndTime == 0) {
        time_t currentTime;
        time(&currentTime);
        g_utDestTraceEndTime = static_cast<uint64_t>(currentTime);
    } else {
        g_utDestTraceEndTime = utTraceEndTime;
    }
    if (maxDuration <= 0) {
        maxDuration = DEFAULT_FULL_TRACE_LENGTH;
    }
    if (g_utDestTraceEndTime <= static_cast<uint64_t>(maxDuration)) {
        g_utDestTraceStartTime = 1; // theoretical impossible value, to avoid overflow while minus tolerance 100ms
    } else {
        g_utDestTraceStartTime = g_utDestTraceEndTime - static_cast<uint64_t>(maxDuration);
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
    g_interruptDump.store(1);
    HILOG_INFO(LOG_CORE, "DumpTrace: Trace is caching, get cache file.");
    {
        std::lock_guard<std::mutex> lock(g_cacheTraceMutex);
        traceRetInfo.coverDuration +=
            SearchTraceFiles(traceRetInfo.outputFiles, g_utDestTraceStartTime, g_utDestTraceEndTime);
        g_interruptDump.store(0);
    }
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

void SanitizeRetInfo(TraceRetInfo& traceRetInfo)
{
    traceRetInfo.coverDuration =
        std::min(traceRetInfo.coverDuration, static_cast<int>(DEFAULT_FULL_TRACE_LENGTH * S_TO_MS));
    traceRetInfo.coverRatio = std::min(traceRetInfo.coverRatio, MAX_RATIO_UNIT);
}

void FilterRecordResult(std::vector<std::string>& outputFiles, const std::vector<FileWithTime>& recordingOutput)
{
    outputFiles.clear();
    for (const auto& output : recordingOutput) {
        if (output.isNewFile) {
            outputFiles.emplace_back(output.filename);
        }
    }
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

    TraceErrorCode ret = HandleDefaultTraceOpen(tagGroups);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: failed.");
        return ret;
    }
    if (!g_traceJsonParser->ParseTraceJson(TRACE_SNAPSHOT_FILE_AGE)) {
        HILOG_WARN(LOG_CORE, "OpenTrace: Failed to parse TRACE_SNAPSHOT_FILE_AGE.");
    }
    RefreshTraceVec(g_traceFileVec, TRACE_SNAPSHOT);
    {
        std::lock_guard<std::mutex> cacheLock(g_cacheTraceMutex);
        RefreshTraceVec(g_cacheFileVec, TRACE_CACHE);
        ClearCacheTraceFileByDuration(g_cacheFileVec);
    }
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

    if (g_traceJsonParser == nullptr) {
        g_traceJsonParser = std::make_shared<TraceJsonParser>();
    }
    if (!g_traceJsonParser->ParseTraceJson(PARSE_TRACE_GROUP_INFO)) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: Failed to parse trace tag format and group infos.");
        return FILE_ERROR;
    }
    auto allTags = g_traceJsonParser->GetAllTagInfos();
    auto tagGroupTable = g_traceJsonParser->GetTagGroups();
    auto traceFormats = g_traceJsonParser->GetBaseFmtPath();

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
    if (!g_cacheEnd.load()) {
        HILOG_ERROR(LOG_CORE, "CacheTraceOn: cache trace is dumping now.");
        return WRONG_TRACE_MODE;
    }

    SetTotalFileSizeLimitAndSliceMaxDuration(totalFileSize, sliceMaxDuration);
    g_cacheFlag.store(true);
    g_cacheEnd.store(false);
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
    g_cacheFlag.store(false);
    while (!g_cacheEnd.load()) {
        g_cacheFlag.store(false);
        usleep(UNIT_TIME);
    }
    HILOG_INFO(LOG_CORE, "Caching trace off.");
    g_traceMode &= ~TraceMode::CACHE;
    return SUCCESS;
}

TraceRetInfo DumpTrace(int maxDuration, uint64_t utTraceEndTime)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    TraceRetInfo ret;
    ret.mode = g_traceMode;
    if (!IsTraceOpen() || IsRecordOn()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: WRONG_TRACE_MODE, current trace mode: %{public}u.",
            static_cast<uint32_t>(g_traceMode));
        ret.errorCode = WRONG_TRACE_MODE;
        return ret;
    }
    HILOG_INFO(LOG_CORE, "DumpTrace start, target duration is %{public}d, target endtime is (%{public}" PRIu64 ").",
        maxDuration, utTraceEndTime);

    if (maxDuration < 0) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: Illegal input: maxDuration = %d < 0.", maxDuration);
        ret.errorCode = INVALID_MAX_DURATION;
        return ret;
    }

    if (!CheckServiceRunning()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: TRACE_IS_OCCUPIED.");
        ret.errorCode = TRACE_IS_OCCUPIED;
        return ret;
    }
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

    ret.errorCode = ProcessDump(ret);
    LoadDumpRet(ret, committedDuration);
    RestoreTimeIntervalBoundary();
    SanitizeRetInfo(ret);
    HILOG_INFO(LOG_CORE, "DumpTrace with time limit done.");
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

    if (!g_recordEnd.load()) {
        HILOG_ERROR(LOG_CORE, "RecordTraceOn: WRONG_TRACE_MODE, record trace is dumping now.");
        return WRONG_TRACE_MODE;
    }

    // start task thread
    g_recordFlag.store(true);
    g_recordEnd.store(false);
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

        std::lock_guard<std::mutex> lock(g_recordingOutputMutex);
        FilterRecordResult(ret.outputFiles, g_recordingOutput);
        return ret;
    }

    g_recordFlag.store(false);
    while (!g_recordEnd.load()) {
        usleep(UNIT_TIME);
        g_recordFlag.store(false);
    }
    ret.errorCode = SUCCESS;

    std::lock_guard<std::mutex> outputFileslock(g_recordingOutputMutex);
    FilterRecordResult(ret.outputFiles, g_recordingOutput);
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
    if (IsRecordOn()) {
        g_recordFlag.store(false);
        while (!g_recordEnd.load()) {
            usleep(UNIT_TIME);
            g_recordFlag.store(false);
        }
    }
    if (IsCacheOn()) {
        g_cacheFlag.store(false);
        while (!g_cacheEnd.load()) {
            usleep(UNIT_TIME);
            g_cacheFlag.store(false);
        }
    }
    g_traceMode = TraceMode::CLOSE;
    OHOS::system::SetParameter(TRACE_KEY_APP_PID, "-1");

    if (g_traceJsonParser == nullptr) {
        g_traceJsonParser = std::make_shared<TraceJsonParser>();
    }
    if (!g_traceJsonParser->ParseTraceJson(PARSE_TRACE_ENABLE_INFO)) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: Failed to parse trace tag enable infos.");
        return FILE_ERROR;
    }
    auto allTags = g_traceJsonParser->GetAllTagInfos();
    if (allTags.size() == 0) {
        HILOG_ERROR(LOG_CORE, "CloseTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceInit(allTags);
    TruncateFile(TRACE_NODE);
    HILOG_INFO(LOG_CORE, "CloseTrace done.");
    return SUCCESS;
}

std::vector<std::pair<std::string, int>> GetTraceFilesTable()
{
    return g_traceFilesTable;
}

void SetTraceFilesTable(const std::vector<std::pair<std::string, int>>& traceFilesTable)
{
    g_traceFilesTable = traceFilesTable;
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
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
