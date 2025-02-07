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
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "dynamic_buffer.h"
#include "hitrace_meter.h"
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
    std::string bufferSize;
    std::string clockType;
    std::string isOverWrite;
    std::string outputFile;
    int fileLimit;
    int fileSize;
    int appPid;
};

constexpr uint16_t MAGIC_NUMBER = 57161;
constexpr uint16_t VERSION_NUMBER = 1;
constexpr uint8_t FILE_RAW_TRACE = 0;
constexpr uint8_t HM_FILE_RAW_TRACE = 1;
constexpr uint64_t CACHE_TRACE_LOOP_SLEEP_TIME = 1;
constexpr int UNIT_TIME = 100000;
constexpr int ALIGNMENT_COEFFICIENT = 4;

const int DEFAULT_BUFFER_SIZE = 12 * 1024;
const int DEFAULT_FILE_SIZE = 100 * 1024;
#ifdef HITRACE_UNITTEST
const int DEFAULT_CACHE_FILE_SIZE = 15 * 1024;
#else
const int DEFAULT_CACHE_FILE_SIZE = 150 * 1024;
#endif
#if defined(SNAPSHOT_TRACEBUFFER_SIZE) && (SNAPSHOT_TRACEBUFFER_SIZE != 0)
const int HM_DEFAULT_BUFFER_SIZE = SNAPSHOT_TRACEBUFFER_SIZE;
#else
const int HM_DEFAULT_BUFFER_SIZE = 144 * 1024;
#endif
const int SAVED_CMDLINES_SIZE = 3072; // 3M
const int BYTE_PER_KB = 1024;
const int BYTE_PER_MB = 1024 * 1024;
constexpr uint64_t S_TO_NS = 1000000000;
constexpr uint64_t S_TO_MS = 1000;
constexpr int32_t MAX_RATIO_UNIT = 1000;
const int MAX_NEW_TRACE_FILE_LIMIT = 5;
const int JUDGE_FILE_EXIST = 10;  // Check whether the trace file exists every 10 times.
const int SNAPSHOT_FILE_MAX_COUNT = 20;
constexpr int DEFAULT_FULL_TRACE_LENGTH = 30;

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
    CONTENT_TYPE_KALLSYMS = 32
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

std::atomic<bool> g_dumpFlag(false);
std::atomic<bool> g_dumpEnd(true);
std::atomic<bool> g_cacheFlag(false);
std::atomic<bool> g_cacheEnd(true);
std::mutex g_traceMutex;
std::mutex g_cacheTraceMutex;

bool g_serviceThreadIsStart = false;
uint64_t g_sysInitParamTags = 0;
TraceMode g_traceMode = TraceMode::CLOSE;
std::string g_traceRootPath;
uint8_t g_buffer[BUFFER_SIZE] = {0};
std::vector<std::pair<std::string, int>> g_traceFilesTable;
std::vector<std::string> g_outputFilesForCmd;
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
std::atomic<uint8_t> g_dumpStatus(TraceErrorCode::UNSET);
std::vector<std::string> g_tags{};
std::vector<TraceFileInfo> g_traceFileVec{};
std::vector<TraceFileInfo> g_cacheFileVec{};

TraceParams g_currentTraceParams = {};
std::shared_ptr<TraceJsonParser> g_traceJsonParser = nullptr;
std::atomic<uint8_t> g_interruptDump(0);

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
    HILOG_INFO(LOG_CORE, "reserved with arch word info is %{public}d.", header.reserved);
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
    HILOG_INFO(LOG_CORE, "reserved with cpu number info is %{public}d.", header.reserved);
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

void SetTraceNodeStatus(const std::string &path, bool enabled)
{
    WriteStrToFile(path, enabled ? "1" : "0");
}

void TruncateFile()
{
    int fd = creat((g_traceRootPath + TRACE_NODE).c_str(), 0);
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

    // if set tagGroup, need to append default group
    if (traceParams.tagGroups.size() > 0) {
        auto iter = tagGroupTable.find("default");
        if (iter == tagGroupTable.end()) {
            HILOG_ERROR(LOG_CORE, "SetAllTags: default group is wrong.");
        } else {
            for (auto defaultTag : iter->second) {
                readyEnableTagList.insert(defaultTag);
            }
        }
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
    TraceInit(allTags);

    TruncateFile();

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
    if (inputMaxDuration < 0) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: Illegal input: maxDuration = %d < 0.", inputMaxDuration);
        return INVALID_MAX_DURATION;
    }

    uint64_t utNow = static_cast<uint64_t>(std::time(nullptr));
    if (utTraceEndTime >= utNow) {
        HILOG_WARN(LOG_CORE, "DumpTrace: Warning: traceEndTime is later than current time, set to current.");
        utTraceEndTime = 0;
    }
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    uint64_t btNow = bts.tv_sec + (bts.tv_nsec != 0 ? 1 : 0);
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
    if (!isCpuRaw || (g_traceMode != TraceMode::CMD_MODE && !g_cacheFlag.load()) || !g_needLimitFileSize) {
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
        while (bytes < BUFFER_SIZE) {
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
        HILOG_ERROR(LOG_CORE, "HmWriteCpuRawInner failed, errno: %{public}d.", static_cast<int>(g_dumpStatus.load()));
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
        HILOG_ERROR(LOG_CORE, "WriteCpuRawInner failed, errno: %{public}d.", static_cast<int>(g_dumpStatus.load()));
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
    time_t firstPageTime = 0;
    time_t lastPageTime = 0;
    if (!ConvertPageTraceTimeToUtTime(firstPageTimestamp, firstPageTime) ||
        !ConvertPageTraceTimeToUtTime(lastPageTimestamp, lastPageTime)) {
        return false;
    }
    uint64_t traceFileSize = 0;
    if (!GetFileSize(newFileName, traceFileSize)) {
        return false;
    }
    traceFileInfo.filename = newFileName;
    traceFileInfo.traceStartTime = static_cast<uint64_t>(firstPageTime);
    traceFileInfo.traceEndTime = static_cast<uint64_t>(lastPageTime);
    traceFileInfo.fileSize = traceFileSize;
    return true;
}

void GetTraceFileFromVec(const uint64_t& inputTraceStartTime, const uint64_t& inputTraceEndTime,
    std::vector<TraceFileInfo>& fileVec, std::vector<std::string>& outputFiles)
{
    if (!fileVec.empty()) {
        for (auto it = fileVec.begin(); it != fileVec.end(); it++) {
            HILOG_INFO(LOG_CORE, "GetTraceFileFromVec: %{public}s, [(%{public}" PRIu64 ", %{public}" PRIu64 "].",
                (*it).filename.c_str(), (*it).traceStartTime, (*it).traceEndTime);
            if ((((*it).traceStartTime >= inputTraceStartTime && (*it).traceStartTime <= inputTraceEndTime) ||
                 ((*it).traceEndTime >= inputTraceStartTime && (*it).traceEndTime <= inputTraceEndTime) ||
                 ((*it).traceStartTime <= inputTraceStartTime && (*it).traceEndTime >= inputTraceEndTime)) &&
                 ((*it).traceEndTime - (*it).traceStartTime < 2000)) { // 2000 : max trace duration 2000s
                outputFiles.push_back((*it).filename);
                HILOG_INFO(LOG_CORE, "Put file: %{public}s into outputFiles.", (*it).filename.c_str());
            }
        }
    }
}

void SearchTraceFiles(const uint64_t& inputTraceStartTime, const uint64_t& inputTraceEndTime,
    std::vector<std::string>& outputFiles)
{
    GetTraceFileFromVec(inputTraceStartTime, inputTraceEndTime, g_cacheFileVec, outputFiles);
    GetTraceFileFromVec(inputTraceStartTime, inputTraceEndTime, g_traceFileVec, outputFiles);
}

bool CacheTraceLoop(const std::string &outputFileName)
{
    std::lock_guard<std::mutex> lock(g_cacheTraceMutex);
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
    struct TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (IsHmKernel()) {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    uint64_t sliceDuration = 0;
    do {
        g_needGenerateNewTraceFile = false;
        ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, reinterpret_cast<char *>(&header), sizeof(header)));
        if (writeRet < 0) {
            HILOG_WARN(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
                strerror(errno), sizeof(header));
            close(outFd);
            return false;
        }
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
            ClearCacheTraceFileBySize(g_cacheFileVec, g_totalFileSizeLimit);
            HILOG_INFO(LOG_CORE, "ProcessCacheTask: save cache file.");
        } else {
            break;
        }
    }
    g_cacheEnd.store(true);
    HILOG_INFO(LOG_CORE, "ProcessCacheTask: trace cache thread exit.");
}

bool DumpTraceLoop(const std::string& outputFileName, bool isLimited)
{
    const int sleepTime = 1;
    int fileSizeThreshold = DEFAULT_FILE_SIZE * BYTE_PER_KB;
    if (g_currentTraceParams.fileSize != 0) {
        fileSizeThreshold = g_currentTraceParams.fileSize * BYTE_PER_KB;
    }
    g_outputFileSize = 0;
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (outFd < 0) {
        HILOG_ERROR(LOG_CORE, "open %{public}s failed, errno: %{public}d.", outPath.c_str(), errno);
        return false;
    }
    MarkClockSync(g_traceRootPath);
    struct TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (IsHmKernel()) {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    do {
        g_needGenerateNewTraceFile = false;
        ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, reinterpret_cast<char *>(&header), sizeof(header)));
        if (writeRet < 0) {
            HILOG_WARN(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
                strerror(errno), sizeof(header));
            close(outFd);
            return false;
        }
        WriteEventsFormat(outFd, outPath);
        while (g_dumpFlag.load()) {
            if (isLimited && g_outputFileSize > fileSizeThreshold) {
                break;
            }
            sleep(sleepTime);
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
            HILOG_INFO(LOG_CORE, "DumpTraceLoop access file:%{public}s failed, errno: %{public}d.",
                outPath.c_str(), errno);
            close(outFd);
            return false;
        }
    } while (g_needGenerateNewTraceFile);
    close(outFd);
    return true;
}

/**
 * read trace data loop
 * g_dumpFlag: true = open，false = close
 * g_dumpEnd: true = end，false = not end
 * if user has own output file, Output all data to the file specified by the user;
 * if not, Then place all the result files in /data/log/hitrace/ and package them once in 96M.
*/
void ProcessDumpTask()
{
    g_dumpFlag.store(true);
    g_dumpEnd.store(false);
    g_outputFilesForCmd = {};
    const std::string threadName = "TraceDumpTask";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "ProcessDumpTask: trace dump thread start.");

    if (!IsRootVersion()) {
        // clear old record file before record tracing start.
        DelOldRecordTraceFile(g_currentTraceParams.fileLimit);
    }

    // if input filesize = 0, trace file should not be cut in root version.
    if (g_currentTraceParams.fileSize == 0 && IsRootVersion()) {
        g_needLimitFileSize = false;
        std::string outputFileName = g_currentTraceParams.outputFile.empty() ?
                                     GenerateTraceFileName(TRACE_RECORDING) : g_currentTraceParams.outputFile;
        if (DumpTraceLoop(outputFileName, g_needLimitFileSize)) {
            g_outputFilesForCmd.push_back(outputFileName);
        }
        g_dumpEnd.store(true);
        g_needLimitFileSize = true;
        return;
    }

    while (g_dumpFlag.load()) {
        if (!IsRootVersion()) {
            ClearOldTraceFile(g_outputFilesForCmd, g_currentTraceParams.fileLimit);
        }
        // Generate file name
        std::string outputFileName = GenerateTraceFileName(TRACE_RECORDING);
        if (DumpTraceLoop(outputFileName, true)) {
            g_outputFilesForCmd.push_back(outputFileName);
        } else {
            break;
        }
    }
    HILOG_INFO(LOG_CORE, "ProcessDumpTask: trace dump thread exit.");
    g_dumpEnd.store(true);
}

bool ReadRawTrace(std::string& outputFileName)
{
    // read trace data from /per_cpu/cpux/trace_pipe_raw
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644:-rw-r--r--
    if (outFd < 0) {
        return false;
    }
    struct TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (IsHmKernel()) {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    ssize_t writeRet = TEMP_FAILURE_RETRY(write(outFd, reinterpret_cast<char*>(&header), sizeof(header)));
    if (writeRet < 0) {
        HILOG_WARN(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
            strerror(errno), sizeof(header));
        close(outFd);
        return false;
    }

    if (WriteEventsFormat(outFd, outPath) && WriteCpuRaw(outFd, outPath) &&
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
        close(pipefd);
        close(epollfd);
        return false;
    }
    ChildProcessRet retVal;
    read(pipefd, &retVal, sizeof(retVal));
    g_dumpStatus = retVal.dumpStatus;
    g_firstPageTimestamp = retVal.traceStartTime;
    g_lastPageTimestamp = retVal.traceEndTime;

    close(pipefd);
    close(epollfd);
    if (waitpid(pid, nullptr, 0) <= 0) {
        HILOG_ERROR(LOG_CORE, "wait HitraceDump(%{public}d) exit failed, errno: (%{public}d)", pid, errno);
    }
    return true;
}

TraceErrorCode DumpTraceInner(std::vector<std::string>& outputFiles)
{
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
        if (g_traceJsonParser == nullptr) {
            g_traceJsonParser = std::make_shared<TraceJsonParser>();
        }
        if (!g_traceJsonParser->ParseTraceJson(TRACE_SNAPSHOT_FILE_AGE)) {
            HILOG_WARN(LOG_CORE, "DumpTraceInner: Failed to parse TRACE_SNAPSHOT_FILE_AGE.");
        }
        if ((!IsRootVersion()) || g_traceJsonParser->GetSnapShotFileAge()) {
            DelSnapshotTraceFile(SNAPSHOT_FILE_MAX_COUNT, g_traceFileVec);
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
        return TraceErrorCode::EPOLL_WAIT_ERROR;
    }

    if (g_dumpStatus) {
        if (remove(reOutPath.c_str()) == 0) {
            HILOG_INFO(LOG_CORE, "Delete outpath:%{public}s success.", reOutPath.c_str());
        } else {
            HILOG_INFO(LOG_CORE, "Delete outpath:%{public}s failed.", reOutPath.c_str());
        }
        SearchTraceFiles(g_utDestTraceStartTime, g_utDestTraceEndTime, outputFiles);
        if (outputFiles.empty()) {
            return static_cast<TraceErrorCode>(g_dumpStatus.load());
        }
        return TraceErrorCode::SUCCESS;
    }

    if (access(reOutPath.c_str(), F_OK) != 0) {
        HILOG_ERROR(LOG_CORE, "DumpTraceInner: write %{public}s failed.", outputFileName.c_str());
        return TraceErrorCode::WRITE_TRACE_INFO_ERROR;
    }

    HILOG_INFO(LOG_CORE, "Output: %{public}s.", reOutPath.c_str());
    TraceFileInfo traceFileInfo;
    if (!SetFileInfo(reOutPath, g_firstPageTimestamp, g_lastPageTimestamp, traceFileInfo)) {
        RemoveFile(reOutPath);
        return TraceErrorCode::WRITE_TRACE_INFO_ERROR;
    }
    g_traceFileVec.push_back(traceFileInfo);
    SearchTraceFiles(g_utDestTraceStartTime, g_utDestTraceEndTime, outputFiles);
    return TraceErrorCode::SUCCESS;
}

uint64_t GetSysParamTags()
{
    return OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
}

void RestartService()
{
    CloseTrace();
    const std::vector<std::string> tagGroups = {"scene_performance"};
    OpenTrace(tagGroups);
}

bool CheckParam()
{
    uint64_t currentTags = GetSysParamTags();
    if (currentTags == g_sysInitParamTags) {
        return true;
    }

    if (currentTags == 0) {
        HILOG_ERROR(LOG_CORE, "tag is 0, restart it.");
        return false;
    }
    HILOG_ERROR(LOG_CORE, "trace is being used, restart later.");
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

void MonitorServiceTask()
{
    g_serviceThreadIsStart = true;
    const std::string threadName = "TraceMonitor";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "MonitorServiceTask: monitor thread start.");
    const int intervalTime = 15;
    while (true) {
        sleep(intervalTime);
        if (g_traceMode != TraceMode::SERVICE_MODE) {
            break;
        }

        if (!CheckServiceRunning()) {
            RestartService();
            continue;
        }

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
    HILOG_INFO(LOG_CORE, "MonitorServiceTask: monitor thread exit.");
    g_serviceThreadIsStart = false;
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
    g_currentTraceParams = traceParams;
    return TraceErrorCode::SUCCESS;
}

TraceErrorCode HandleServiceTraceOpen(const std::vector<std::string>& tagGroups,
                                      const std::map<std::string, TraceTag>& allTags,
                                      const std::map<std::string, std::vector<std::string>>& tagGroupTable,
                                      std::vector<std::string>& tagFmts, const int custBufSz)
{
    TraceParams serviceTraceParams;
    serviceTraceParams.tagGroups = tagGroups;
    // attention: the buffer size value in the configuration file is preferred to set.
    if (custBufSz > 0) {
        serviceTraceParams.bufferSize = std::to_string(custBufSz);
    } else {
        int traceBufSz = IsHmKernel() ? HM_DEFAULT_BUFFER_SIZE : DEFAULT_BUFFER_SIZE;
        serviceTraceParams.bufferSize = std::to_string(traceBufSz);
    }
    serviceTraceParams.clockType = "boot";
    serviceTraceParams.isOverWrite = "1";
    serviceTraceParams.fileSize = DEFAULT_FILE_SIZE;
    return HandleTraceOpen(serviceTraceParams, allTags, tagGroupTable, tagFmts);
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
    if (traceParamsStr.empty() || !IsNumber(traceParamsStr)) {
        HILOG_WARN(LOG_CORE, "Illegal input, traceParams initialized to null.");
        traceParams = 0;
        return;
    }
    traceParams = std::stoi(traceParamsStr);
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
    if (maxDuration == 0) {
        maxDuration = DEFAULT_FULL_TRACE_LENGTH;
    }
    if (g_utDestTraceEndTime < static_cast<uint64_t>(maxDuration)) {
        g_utDestTraceStartTime = 0;
    } else {
        g_utDestTraceStartTime = g_utDestTraceEndTime - static_cast<uint64_t>(maxDuration);
    }
    HILOG_INFO(LOG_CORE, "g_utDestTraceStartTime:(%{public}" PRIu64 "), g_utDestTraceEndTime:(%{public}" PRIu64 ").",
        g_utDestTraceStartTime, g_utDestTraceEndTime);
}

/**
 * args: tags:tag1,tags2... tagGroups:group1,group2... clockType:boot bufferSize:1024 overwrite:1 output:filename
 * cmdTraceParams:  Save the above parameters
*/
bool ParseArgs(const std::string& args, TraceParams& cmdTraceParams, const std::map<std::string, TraceTag>& allTags,
               const std::map<std::string, std::vector<std::string>>& tagGroupTable)
{
    std::string userArgs = args;
    std::string str = ":";
    RemoveUnSpace(str, userArgs);
    str = ",";
    RemoveUnSpace(str, userArgs);
    std::vector<std::string> argList = Split(userArgs, ' ');
    for (std::string item : argList) {
        size_t pos = item.find(":");
        if (pos == std::string::npos) {
            HILOG_ERROR(LOG_CORE, "trace command line without colon appears: %{public}s, continue.", item.c_str());
            continue;
        }
        std::string itemName = item.substr(0, pos);
        if (itemName == "tags") {
            cmdTraceParams.tags = Split(item.substr(pos + 1), ',');
        } else if (itemName == "tagGroups") {
            cmdTraceParams.tagGroups = Split(item.substr(pos + 1), ',');
        } else if (itemName == "clockType") {
            cmdTraceParams.clockType = item.substr(pos + 1);
        } else if (itemName == "bufferSize") {
            cmdTraceParams.bufferSize = item.substr(pos + 1);
        } else if (itemName == "overwrite") {
            cmdTraceParams.isOverWrite = item.substr(pos + 1);
        } else if (itemName == "output") {
            cmdTraceParams.outputFile = item.substr(pos + 1);
        } else if (itemName == "fileSize") {
            std::string fileSizeStr = item.substr(pos + 1);
            SetCmdTraceIntParams(fileSizeStr, cmdTraceParams.fileSize);
        } else if (itemName == "fileLimit") {
            std::string fileLimitStr = item.substr(pos + 1);
            SetCmdTraceIntParams(fileLimitStr, cmdTraceParams.fileLimit);
        } else if (itemName == "appPid") {
            std::string pidStr = item.substr(pos + 1);
            SetCmdTraceIntParams(pidStr, cmdTraceParams.appPid);
            if (cmdTraceParams.appPid == 0) {
                HILOG_ERROR(LOG_CORE, "Illegal input, appPid(%{public}s) must be number and greater than 0.",
                    pidStr.c_str());
                return false;
            }
            OHOS::system::SetParameter(TRACE_KEY_APP_PID, pidStr);
        } else {
            HILOG_ERROR(LOG_CORE, "Extra trace command line options appear when ParseArgs: %{public}s, return false.",
                itemName.c_str());
            return false;
        }
    }
    if (CheckTags(cmdTraceParams.tags, allTags) && CheckTagGroup(cmdTraceParams.tagGroups, tagGroupTable)) {
        return true;
    }
    return false;
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
    std::lock_guard<std::mutex> lock(g_cacheTraceMutex);
    SearchTraceFiles(g_utDestTraceStartTime, g_utDestTraceEndTime, traceRetInfo.outputFiles);
    if (traceRetInfo.outputFiles.empty()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: Trace is caching, search file failed.");
        traceRetInfo.errorCode = OUT_OF_TIME;
    } else {
        for (const auto& file: traceRetInfo.outputFiles) {
            HILOG_INFO(LOG_CORE, "dumptrace file is %{public}s.", file.c_str());
        }
        traceRetInfo.errorCode = SUCCESS;
    }
    g_interruptDump.store(0);
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

TraceMode GetTraceMode()
{
    return g_traceMode;
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

TraceErrorCode OpenTrace(const std::vector<std::string>& tagGroups)
{
    if (g_traceMode != CLOSE) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: WRONG_TRACE_MODE, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return WRONG_TRACE_MODE;
    }
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (!IsTraceMounted(g_traceRootPath)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }

    if (g_traceJsonParser == nullptr) {
        g_traceJsonParser = std::make_shared<TraceJsonParser>();
    }
    if (!g_traceJsonParser->ParseTraceJson(PARSE_ALL_INFO)) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: Failed to parse trace tag total infos.");
        return FILE_ERROR;
    }
    auto allTags = g_traceJsonParser->GetAllTagInfos();
    auto tagGroupTable = g_traceJsonParser->GetTagGroups();
    auto traceFormats = g_traceJsonParser->GetBaseFmtPath();
    auto customizedBufSz = g_traceJsonParser->GetSnapShotBufSzKb();

    if (tagGroups.size() == 0 || !CheckTagGroup(tagGroups, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceErrorCode ret = HandleServiceTraceOpen(tagGroups, allTags, tagGroupTable, traceFormats, customizedBufSz);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: open fail.");
        return ret;
    }
    g_traceMode = SERVICE_MODE;
    if (!g_traceJsonParser->ParseTraceJson(TRACE_SNAPSHOT_FILE_AGE)) {
        HILOG_WARN(LOG_CORE, "OpenTrace: Failed to parse TRACE_SNAPSHOT_FILE_AGE.");
    }
    RefreshTraceVec(g_traceFileVec, TRACE_SNAPSHOT);
    RefreshTraceVec(g_cacheFileVec, TRACE_CACHE);
    std::unique_lock<std::mutex> lck(g_cacheTraceMutex);
    ClearCacheTraceFileByDuration(g_cacheFileVec);
    lck.unlock();
    PreWriteEventsFormat(traceFormats);
    if (!IsHmKernel() && !g_serviceThreadIsStart) {
        // open SERVICE_MODE monitor thread
        auto it = []() {
            MonitorServiceTask();
        };
        std::thread auxiliaryTask(it);
        auxiliaryTask.detach();
    }
    g_sysInitParamTags = GetSysParamTags();
    HILOG_INFO(LOG_CORE, "OpenTrace: SERVICE_MODE open success.");
    g_tags = tagGroups;
    return ret;
}

TraceErrorCode OpenTrace(const std::string& args)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != CLOSE) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: WRONG_TRACE_MODE, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return WRONG_TRACE_MODE;
    }

    if (!IsTraceMounted(g_traceRootPath)) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: TRACE_NOT_SUPPORTED.");
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
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }
    // parse args
    TraceParams cmdTraceParams;
    if (!ParseArgs(args, cmdTraceParams, allTags, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceErrorCode ret = HandleTraceOpen(cmdTraceParams, allTags, tagGroupTable, traceFormats);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: CMD_MODE open failed.");
        return FILE_ERROR;
    }
    g_traceMode = CMD_MODE;
    PreWriteEventsFormat(traceFormats);
    HILOG_INFO(LOG_CORE, "Hitrace OpenTrace: CMD_MODE open success, args:%{public}s.", args.c_str());
    return ret;
}

TraceErrorCode CacheTraceOn(uint64_t totalFileSize, uint64_t sliceMaxDuration)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != SERVICE_MODE) {
        HILOG_ERROR(LOG_CORE, "CacheTraceOn: WRONG_TRACE_MODE, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
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
    return SUCCESS;
}

TraceErrorCode CacheTraceOff()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != SERVICE_MODE) {
        HILOG_ERROR(LOG_CORE,
            "CacheTraceOff: WRONG_TRACE_MODE, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return WRONG_TRACE_MODE;
    }
    g_cacheFlag.store(false);
    while (!g_cacheEnd.load()) {
        g_cacheFlag.store(false);
        usleep(UNIT_TIME);
    }
    HILOG_INFO(LOG_CORE, "Caching trace off.");
    return SUCCESS;
}

TraceRetInfo DumpTrace(int maxDuration, uint64_t utTraceEndTime)
{
    std::unique_lock<std::mutex> lock(g_traceMutex);
    HILOG_INFO(LOG_CORE, "DumpTrace with timelimit start, timelimit is %{public}d, endtime is (%{public}" PRIu64 ").",
        maxDuration, utTraceEndTime);
    TraceRetInfo ret;
    if (g_traceMode != SERVICE_MODE) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: WRONG_TRACE_MODE, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        ret.errorCode = WRONG_TRACE_MODE;
        return ret;
    }

    if (!CheckServiceRunning()) {
        lock.unlock();
        RestartService();
        HILOG_ERROR(LOG_CORE, "DumpTrace: TRACE_IS_OCCUPIED.");
        ret.errorCode = TRACE_IS_OCCUPIED;
        return ret;
    }
    SetDestTraceTimeAndDuration(maxDuration, utTraceEndTime);
    if (UNEXPECTANTLY(g_cacheFlag.load())) {
        GetFileInCache(ret);
        return ret;
    }

    ret.errorCode = SetTimeIntervalBoundary(maxDuration, utTraceEndTime);
    if (ret.errorCode != SUCCESS) {
        return ret;
    }
    g_firstPageTimestamp = UINT64_MAX;
    g_lastPageTimestamp = 0;

    uint32_t committedDuration = static_cast<uint32_t>(maxDuration == 0 ? DEFAULT_FULL_TRACE_LENGTH :
        std::min(maxDuration, DEFAULT_FULL_TRACE_LENGTH));
    ret.errorCode = DumpTraceInner(ret.outputFiles);
    if (g_traceEndTime <= g_firstPageTimestamp) {
        ret.coverRatio = 0;
    } else {
        ret.coverDuration = static_cast<int32_t>(std::min((g_traceEndTime - g_firstPageTimestamp) *
            S_TO_MS / S_TO_NS, committedDuration * S_TO_MS));
        ret.coverRatio = static_cast<int32_t>((g_traceEndTime - g_firstPageTimestamp) *
            MAX_RATIO_UNIT / S_TO_NS / committedDuration);
    }
    if (ret.coverRatio > MAX_RATIO_UNIT) {
        ret.coverRatio = MAX_RATIO_UNIT;
    }
    ret.tags = g_tags;
    RestoreTimeIntervalBoundary();
    HILOG_INFO(LOG_CORE, "DumpTrace with time limit done.");
    return ret;
}

TraceErrorCode DumpTraceOn()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    // check current trace status
    if (g_traceMode != CMD_MODE) {
        HILOG_ERROR(LOG_CORE, "DumpTraceOn: WRONG_TRACE_MODE, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return WRONG_TRACE_MODE;
    }

    if (!g_dumpEnd.load()) {
        HILOG_ERROR(LOG_CORE, "DumpTraceOn: WRONG_TRACE_MODE, record trace is dumping now.");
        return WRONG_TRACE_MODE;
    }

    // start task thread
    auto it = []() {
        ProcessDumpTask();
    };
    std::thread task(it);
    task.detach();
    WriteCpuFreqTrace();
    HILOG_INFO(LOG_CORE, "Recording trace on.");
    return SUCCESS;
}

TraceRetInfo DumpTraceOff()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    TraceRetInfo ret;
    // check current trace status
    if (g_traceMode != CMD_MODE) {
        HILOG_ERROR(LOG_CORE, "DumpTraceOff: The current state is %{public}d, data exception.",
            static_cast<int>(g_traceMode));
        ret.errorCode = WRONG_TRACE_MODE;
        ret.outputFiles = g_outputFilesForCmd;
        return ret;
    }

    g_dumpFlag.store(false);
    while (!g_dumpEnd.load()) {
        usleep(UNIT_TIME);
        g_dumpFlag.store(false);
    }
    ret.errorCode = SUCCESS;
    ret.outputFiles = g_outputFilesForCmd;
    HILOG_INFO(LOG_CORE, "Recording trace off.");
    return ret;
}

TraceErrorCode CloseTrace()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    HILOG_INFO(LOG_CORE, "CloseTrace start.");
    if (g_traceMode == CLOSE) {
        HILOG_INFO(LOG_CORE, "Trace already close.");
        return SUCCESS;
    }

    g_traceMode = CLOSE;
    // Waiting for the data drop task to end
    g_dumpFlag.store(false);
    g_cacheFlag.store(false);
    while (!g_dumpEnd.load()) {
        usleep(UNIT_TIME);
        g_dumpFlag.store(false);
    }
    while (!g_cacheEnd.load()) {
        usleep(UNIT_TIME);
        g_cacheFlag.store(false);
    }
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
    TruncateFile();
    g_tags.clear();
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
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
