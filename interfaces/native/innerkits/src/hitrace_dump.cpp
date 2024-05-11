/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include "common_utils.h"
#include "dynamic_buffer.h"

#include <map>
#include <atomic>
#include <fstream>
#include <set>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <memory>

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <cinttypes>
#include <utility>
#include <dirent.h>

#include "cJSON.h"
#include "parameters.h"
#include "hilog/log.h"
#include "securec.h"


using OHOS::HiviewDFX::HiLog;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

namespace {

struct TraceParams {
    std::vector<std::string> tags;
    std::vector<std::string> tagGroups;
    std::string bufferSize;
    std::string clockType;
    std::string isOverWrite;
    std::string outputFile;
    std::string fileSize;
    std::string fileLimit;
    std::string appPid;
};

constexpr uint16_t MAGIC_NUMBER = 57161;
constexpr uint16_t VERSION_NUMBER = 1;
constexpr uint8_t FILE_RAW_TRACE = 0;
constexpr uint8_t HM_FILE_RAW_TRACE = 1;
constexpr int UNIT_TIME = 100000;
constexpr int ALIGNMENT_COEFFICIENT = 4;

const int DEFAULT_BUFFER_SIZE = 12 * 1024;
const int DEFAULT_FILE_SIZE = 100 * 1024;
const int HM_DEFAULT_BUFFER_SIZE = 144 * 1024;
const int SAVED_CMDLINES_SIZE = 3072; // 3M
const int KB_PER_MB = 1024;
const int S_TO_NS = 1000000000;
const int MAX_NEW_TRACE_FILE_LIMIT = 5;
const int JUDGE_FILE_EXIST = 10;  // Check whether the trace file exists every 10 times.

const std::string DEFAULT_OUTPUT_DIR = "/data/log/hitrace/";
const std::string SNAPSHOT_PREFIX = "trace_";
const std::string RECORDING_PREFIX = "record_trace_";
const std::string SAVED_EVENTS_FORMAT = "saved_events_format";

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

#ifndef PAGE_SIZE
constexpr size_t PAGE_SIZE = 4096;
#endif

const int BUFFER_SIZE = 256 * PAGE_SIZE; // 1M

std::atomic<bool> g_dumpFlag(false);
std::atomic<bool> g_dumpEnd(true);
std::mutex g_traceMutex;

bool g_serviceThreadIsStart = false;
uint64_t g_sysInitParamTags = 0;
TraceMode g_traceMode = TraceMode::CLOSE;
std::string g_traceRootPath;
std::string g_traceHmDir;
uint8_t g_buffer[BUFFER_SIZE] = {0};
std::vector<std::pair<std::string, int>> g_traceFilesTable;
std::vector<std::string> g_outputFilesForCmd;
int g_outputFileSize = 0;
int g_timeLimit = 0;
int g_newTraceFileLimit = 0;
int g_writeFileLimit = 0;
bool g_needGenerateNewTraceFile = false;

TraceParams g_currentTraceParams = {};

std::string GetFilePath(const std::string &fileName)
{
    if (access((g_traceRootPath + "hongmeng/" + fileName).c_str(), F_OK) == 0) {
        return g_traceRootPath + "hongmeng/" + fileName;
    } else {
        return g_traceRootPath + fileName;
    }
}

std::vector<std::string> Split(const std::string &str, char delimiter)
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

bool IsTraceMounted()
{
    const std::string debugfsPath = "/sys/kernel/debug/tracing/";
    const std::string tracefsPath = "/sys/kernel/tracing/";
    if (access((debugfsPath + "trace_marker").c_str(), F_OK) != -1) {
        g_traceRootPath = debugfsPath;
        return true;
    }
    if (access((tracefsPath + "trace_marker").c_str(), F_OK) != -1) {
        g_traceRootPath = tracefsPath;
        return true;
    }
    HILOG_ERROR(LOG_CORE, "IsTraceMounted: Did not find trace folder");
    return false;
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


int GetCpuProcessors()
{
    int processors = 0;
    processors = sysconf(_SC_NPROCESSORS_CONF);
    return (processors == 0) ? 1 : processors;
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

bool CheckTags(const std::vector<std::string> &tags, const std::map<std::string, TagCategory> &allTags)
{
    for (const auto &tag : tags) {
        if (allTags.find(tag) == allTags.end()) {
            HILOG_ERROR(LOG_CORE, "CheckTags: %{public}s is not provided.", tag.c_str());
            return false;
        }
    }
    return true;
}

bool CheckTagGroup(const std::vector<std::string> &tagGroups,
                   const std::map<std::string, std::vector<std::string>> &tagGroupTable)
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
    bool ret = false;
    if (access((g_traceRootPath + "hongmeng/" + filename).c_str(), W_OK) == 0) {
        if (WriteStrToFileInner(g_traceRootPath + "hongmeng/" + filename, str)) {
            ret = true;
        }
    }
    if (access((g_traceRootPath + filename).c_str(), W_OK) == 0) {
        if (WriteStrToFileInner(g_traceRootPath + filename, str)) {
            ret = true;
        }
    }

    return ret;
}

void SetFtraceEnabled(const std::string &path, bool enabled)
{
    WriteStrToFile(path, enabled ? "1" : "0");
}

void TruncateFile()
{
    int fd = creat((g_traceRootPath + g_traceHmDir + "trace").c_str(), 0);
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

void TraceInit(const std::map<std::string, TagCategory> &allTags)
{
    // close all ftrace events
    for (auto it = allTags.begin(); it != allTags.end(); it++) {
        if (it->second.type != 1) {
            continue;
        }
        for (size_t i = 0; i < it->second.sysFiles.size(); i++) {
            SetFtraceEnabled(it->second.sysFiles[i], false);
        }
    }
    // close all user tags
    SetProperty("debug.hitrace.tags.enableflags", std::to_string(0));

    // set buffer_size_kb 1
    WriteStrToFile("buffer_size_kb", "1");

    // close tracing_on
    SetFtraceEnabled("tracing_on", false);
}

void SetAllTags(const TraceParams &traceParams, const std::map<std::string, TagCategory> &allTags,
                const std::map<std::string, std::vector<std::string>> &tagGroupTable)
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
            for (const auto& path : iter->second.sysFiles) {
                SetFtraceEnabled(path, true);
            }
        }
    }
    SetProperty("debug.hitrace.tags.enableflags", std::to_string(enabledUserTags));
}

std::string ReadFileInner(const std::string& filename)
{
    std::string resolvedPath = CanonicalizeSpecPath(filename.c_str());
    std::ifstream fileIn(resolvedPath.c_str());
    if (!fileIn.is_open()) {
        HILOG_ERROR(LOG_CORE, "ReadFile: %{public}s open failed.", filename.c_str());
        return "";
    }

    std::string str((std::istreambuf_iterator<char>(fileIn)), std::istreambuf_iterator<char>());
    fileIn.close();
    return str;
}

std::string ReadFile(const std::string& filename)
{
    std::string filePath = GetFilePath(filename);
    return ReadFileInner(filePath);
}

void SetClock(const std::string& clockType)
{
    const std::string traceClockPath = "trace_clock";
    if (clockType.size() == 0) {
        WriteStrToFile(traceClockPath, "boot"); //set default: boot
        return;
    }
    std::string allClocks = ReadFile(traceClockPath);
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

bool SetTraceSetting(const TraceParams &traceParams, const std::map<std::string, TagCategory> &allTags,
                     const std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    TraceInit(allTags);

    TruncateFile();

    SetAllTags(traceParams, allTags, tagGroupTable);

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

bool CheckPage(uint8_t contentType, uint8_t *page)
{
    const int pageThreshold = PAGE_SIZE / 2;

    // Check raw_trace page size.
    if (contentType >= CONTENT_TYPE_CPU_RAW && g_traceHmDir == "") {
        PageHeader *pageHeader = reinterpret_cast<PageHeader*>(&page);
        if (pageHeader->size < static_cast<uint64_t>(pageThreshold)) {
            return false;
        }
    }

    return true;
}

std::string GenerateName(bool isSnapshot = true)
{
    // eg: /data/log/hitrace/trace_localtime@boottime.sys
    std::string name = DEFAULT_OUTPUT_DIR;

    if (isSnapshot) {
        name += SNAPSHOT_PREFIX;
    } else {
        name += RECORDING_PREFIX;
    }

    // get localtime
    time_t currentTime;
    time(&currentTime);
    struct tm timeInfo = {};
    const int bufferSize = 16;
    char timeStr[bufferSize] = {0};
    if (localtime_r(&currentTime, &timeInfo) == nullptr) {
        HILOG_ERROR(LOG_CORE, "Get localtime failed.");
        return "";
    }
    strftime(timeStr, bufferSize, "%Y%m%d%H%M%S", &timeInfo);
    name += std::string(timeStr);
    // get boottime
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    name += "@" + std::to_string(bts.tv_sec) + "-" + std::to_string(bts.tv_nsec) + ".sys";

    struct timespec mts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &mts);
    HILOG_INFO(LOG_CORE, "output trace: %{public}s, boot_time(%{public}" PRId64 "), mono_time(%{public}" PRId64 ").",
                name.c_str(), static_cast<int64_t>(bts.tv_sec), static_cast<int64_t>(mts.tv_sec));
    return name;
}

bool CheckFileExist(const std::string &outputFile)
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

bool WriteFile(uint8_t contentType, const std::string &src, int outFd, const std::string &outputFile)
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
    int readLen = 0;
    int count = 0;
    const int maxCount = 2;

    struct timespec bts = {0, 0};
    int64_t traceStartTime = 0;

    if ((contentType == CONTENT_TYPE_CPU_RAW) && (g_timeLimit > 0)) {
        clock_gettime(CLOCK_BOOTTIME, &bts);
        traceStartTime = bts.tv_sec * S_TO_NS + bts.tv_nsec - g_timeLimit * S_TO_NS;
    }
    while (true) {
        int bytes = 0;
        bool endFlag = false;
        /* Write 1M at a time */
        while (bytes < BUFFER_SIZE) {
            ssize_t readBytes = TEMP_FAILURE_RETRY(read(srcFd, g_buffer + bytes, PAGE_SIZE));
            if (readBytes <= 0) {
                endFlag = true;
                HILOG_INFO(LOG_CORE, "WriteFile: read %{public}s end or failed.", src.c_str());
                break;
            }

            if (traceStartTime > 0) {
                uint64_t traceTime = *(reinterpret_cast<uint64_t *>(g_buffer));
                if (traceTime < static_cast<uint64_t>(traceStartTime)) {
                    continue;
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

        write(outFd, g_buffer, bytes);
        readLen += bytes;
        if (endFlag == true) {
            break;
        }
    }
    contentHeader.length = static_cast<uint32_t>(readLen);
    uint32_t offset = contentHeader.length + sizeof(contentHeader);
    off_t pos = lseek(outFd, 0, SEEK_CUR);
    lseek(outFd, pos - offset, SEEK_SET);
    write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    lseek(outFd, pos, SEEK_SET);
    close(srcFd);
    g_outputFileSize += static_cast<int>(offset);
    g_needGenerateNewTraceFile = false;
    HILOG_INFO(LOG_CORE, "WriteFile end, path: %{public}s, byte: %{public}d. g_writeFileLimit: %{public}d",
        src.c_str(), readLen, g_writeFileLimit);
    return true;
}

void WriteEventFile(std::string &srcPath, int outFd)
{
    uint8_t buffer[PAGE_SIZE] = {0};
    std::string srcSpecPath = CanonicalizeSpecPath(srcPath.c_str());
    int srcFd = open(srcSpecPath.c_str(), O_RDONLY);
    if (srcFd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteEventFile: open %{public}s failed.", srcPath.c_str());
        return;
    }
    ssize_t readLen = 0;
    do {
        ssize_t len = read(srcFd, buffer, PAGE_SIZE);
        if (len <= 0) {
            break;
        }
        write(outFd, buffer, len);
        readLen += len;
    } while (true);
    close(srcFd);
    HILOG_INFO(LOG_CORE, "WriteEventFile end, path: %{public}s, data size: %{public}zd.", srcPath.c_str(), readLen);
}

bool WriteEventsFormat(int outFd, const std::string &outputFile)
{
    const std::string savedEventsFormatPath = DEFAULT_OUTPUT_DIR + SAVED_EVENTS_FORMAT;
    if (access(savedEventsFormatPath.c_str(), F_OK) != -1) {
        return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, savedEventsFormatPath, outFd, outputFile);
    }
    std::string filePath = CanonicalizeSpecPath(savedEventsFormatPath.c_str());
    int fd = open(filePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteEventsFormat: open %{public}s failed.", savedEventsFormatPath.c_str());
        return false;
    }
    const std::vector<std::string> priorityTracingCategory = {
        "events/sched/sched_wakeup/format",
        "events/sched/sched_switch/format",
        "events/sched/sched_blocked_reason/format",
        "events/power/cpu_frequency/format",
        "events/power/clock_set_rate/format",
        "events/power/cpu_frequency_limits/format",
        "events/f2fs/f2fs_sync_file_enter/format",
        "events/f2fs/f2fs_sync_file_exit/format",
        "events/f2fs/f2fs_readpage/format",
        "events/f2fs/f2fs_readpages/format",
        "events/f2fs/f2fs_sync_fs/format",
        "events/hmdfs/hmdfs_syncfs_enter/format",
        "events/hmdfs/hmdfs_syncfs_exit/format",
        "events/erofs/erofs_readpage/format",
        "events/erofs/erofs_readpages/format",
        "events/ext4/ext4_da_write_begin/format",
        "events/ext4/ext4_da_write_end/format",
        "events/ext4/ext4_sync_file_enter/format",
        "events/ext4/ext4_sync_file_exit/format",
        "events/block/block_bio_remap/format",
        "events/block/block_rq_issue/format",
        "events/block/block_rq_complete/format",
        "events/block/block_rq_insert/format",
        "events/dma_fence/dma_fence_emit/format",
        "events/dma_fence/dma_fence_destroy/format",
        "events/dma_fence/dma_fence_enable_signal/format",
        "events/dma_fence/dma_fence_signaled/format",
        "events/dma_fence/dma_fence_wait_end/format",
        "events/dma_fence/dma_fence_wait_start/format",
        "events/dma_fence/dma_fence_init/format",
        "events/binder/binder_transaction/format",
        "events/binder/binder_transaction_received/format",
        "events/mmc/mmc_request_start/format",
        "events/mmc/mmc_request_done/format",
        "events/memory_bus/format",
        "events/cpufreq_interactive/format",
        "events/filemap/file_check_and_advance_wb_err/format",
        "events/filemap/filemap_set_wb_err/format",
        "events/filemap/mm_filemap_add_to_page_cache/format",
        "events/filemap/mm_filemap_delete_from_page_cache/format",
        "events/workqueue/workqueue_execute_end/format",
        "events/workqueue/workqueue_execute_start/format",
        "events/thermal_power_allocator/thermal_power_allocator/format",
        "events/thermal_power_allocator/thermal_power_allocator_pid/format",
        "events/ftrace/print/format",
        "events/tracing_mark_write/tracing_mark_write/format",
    };
    for (size_t i = 0; i < priorityTracingCategory.size(); i++) {
        std::string srcPath = g_traceRootPath + "hongmeng/" + priorityTracingCategory[i];
        if (access(srcPath.c_str(), R_OK) != -1) {
            WriteEventFile(srcPath, fd);
        }
        srcPath = g_traceRootPath + priorityTracingCategory[i];
        if (access(srcPath.c_str(), R_OK) != -1) {
            WriteEventFile(srcPath, fd);
        }
    }
    close(fd);
    HILOG_INFO(LOG_CORE, "WriteEventsFormat end. path: %{public}s.", filePath.c_str());
    return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, filePath, outFd, outputFile);
}

bool WriteHeaderPage(int outFd, const std::string &outputFile)
{
    if (g_traceHmDir != "") {
        return true;
    }
    std::string headerPagePath = GetFilePath("events/header_page");
    return WriteFile(CONTENT_TYPE_HEADER_PAGE, headerPagePath, outFd, outputFile);
}

bool WritePrintkFormats(int outFd, const std::string &outputFile)
{
    if (g_traceHmDir != "") {
        return true;
    }
    std::string printkFormatPath = GetFilePath("printk_formats");
    return WriteFile(CONTENT_TYPE_PRINTK_FORMATS, printkFormatPath, outFd, outputFile);
}

bool WriteKallsyms(int outFd)
{
    /* not implement in hongmeng */
    if (g_traceHmDir != "") {
        return true;
    }
    /* not implement in linux */
    return true;
}

bool HmWriteCpuRawInner(int outFd, const std::string &outputFile)
{
    uint8_t type = CONTENT_TYPE_CPU_RAW;
    std::string src = g_traceRootPath + "hongmeng/trace_pipe_raw";

    return WriteFile(type, src, outFd, outputFile);
}

bool WriteCpuRawInner(int outFd, const std::string &outputFile)
{
    int cpuNums = GetCpuProcessors();
    int ret = true;
    uint8_t type = CONTENT_TYPE_CPU_RAW;
    for (int i = 0; i < cpuNums; i++) {
        std::string src = g_traceRootPath + "per_cpu/cpu" + std::to_string(i) + "/trace_pipe_raw";
        if (!WriteFile(static_cast<uint8_t>(type + i), src, outFd, outputFile)) {
            ret = false;
            break;
        }
    }
    return ret;
}

bool WriteCpuRaw(int outFd, const std::string &outputFile)
{
    if (g_traceHmDir == "") {
        return WriteCpuRawInner(outFd, outputFile);
    } else {
        return HmWriteCpuRawInner(outFd, outputFile);
    }
}

bool WriteCmdlines(int outFd, const std::string &outputFile)
{
    std::string cmdlinesPath = GetFilePath("saved_cmdlines");
    return WriteFile(CONTENT_TYPE_CMDLINES, cmdlinesPath, outFd, outputFile);
}

bool WriteTgids(int outFd, const std::string &outputFile)
{
    std::string tgidsPath = GetFilePath("saved_tgids");
    return WriteFile(CONTENT_TYPE_TGIDS, tgidsPath, outFd, outputFile);
}

bool GenerateNewFile(int &outFd, std::string &outPath)
{
    if (access(outPath.c_str(), F_OK) == 0) {
        return true;
    }
    std::string outputFileName = GenerateName(false);
    outPath = CanonicalizeSpecPath(outputFileName.c_str());
    outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
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

bool DumpTraceLoop(const std::string &outputFileName, bool isLimited)
{
    const int sleepTime = 1;
    int fileSizeThreshold = DEFAULT_FILE_SIZE * KB_PER_MB;
    if (!g_currentTraceParams.fileSize.empty()) {
        fileSizeThreshold = std::stoi(g_currentTraceParams.fileSize) * KB_PER_MB;
    }
    g_outputFileSize = 0;
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (outFd < 0) {
        HILOG_ERROR(LOG_CORE, "open %{public}s failed, errno: %{public}d.", outPath.c_str(), errno);
        return false;
    }
    MarkClockSync(g_traceRootPath);
    struct TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (g_traceHmDir != "") {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    do {
        g_needGenerateNewTraceFile = false;
        write(outFd, reinterpret_cast<char *>(&header), sizeof(header));
        WriteEventsFormat(outFd, outPath);
        while (g_dumpFlag) {
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
        if (!GenerateNewFile(outFd, outPath)) {
            HILOG_INFO(LOG_CORE, "DumpTraceLoop access file:%{public}s failed, errno: %{public}d.",
                outPath.c_str(), errno);
            return false;
        }
    } while (g_needGenerateNewTraceFile);
    close(outFd);
    return true;
}


/**
 * When the raw trace is started, clear the saved_events_format files in the folder.
 */
void ClearSavedEventsFormat()
{
    const std::string savedEventsFormatPath = DEFAULT_OUTPUT_DIR + SAVED_EVENTS_FORMAT;
    if (access(savedEventsFormatPath.c_str(), F_OK) != 0) {
        // saved_events_format not exit
        return;
    }
    // saved_events_format exit
    if (remove(savedEventsFormatPath.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "Delete saved_events_format success.");
    } else {
        HILOG_ERROR(LOG_CORE, "Delete saved_events_format failed.");
    }
}

/**
 * open trace file aging mechanism
 */
void ClearOldTraceFile()
{
    if (g_currentTraceParams.fileLimit.empty()) {
        HILOG_INFO(LOG_CORE, "no activate aging mechanism.");
        return;
    }
    if (g_outputFilesForCmd.size() >= std::stoi(g_currentTraceParams.fileLimit)
        && access(g_outputFilesForCmd[0].c_str(), F_OK) == 0) {
        if (remove(g_outputFilesForCmd[0].c_str()) == 0) {
            g_outputFilesForCmd.erase(g_outputFilesForCmd.begin());
            HILOG_INFO(LOG_CORE, "delete first: %{public}s success.", g_outputFilesForCmd[0].c_str());
        } else {
            HILOG_ERROR(LOG_CORE, "delete first: %{public}s failed.", g_outputFilesForCmd[0].c_str());
        }
    }
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
    g_dumpFlag = true;
    g_dumpEnd = false;
    g_outputFilesForCmd = {};
    const std::string threadName = "TraceDumpTask";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "ProcessDumpTask: trace dump thread start.");
    ClearSavedEventsFormat();
    if (g_currentTraceParams.fileSize.empty()) {
        std::string outputFileName = g_currentTraceParams.outputFile.empty() ?
                                     GenerateName(false) : g_currentTraceParams.outputFile;
        if (DumpTraceLoop(outputFileName, false)) {
            g_outputFilesForCmd.push_back(outputFileName);
        }
        g_dumpEnd = true;
        return;
    }
    
    while (g_dumpFlag) {
        ClearOldTraceFile();
        // Generate file name
        std::string outputFileName = GenerateName(false);
        if (DumpTraceLoop(outputFileName, true)) {
            g_outputFilesForCmd.push_back(outputFileName);
        } else {
            break;
        }
    }
    HILOG_INFO(LOG_CORE, "ProcessDumpTask: trace dump thread exit.");
    g_dumpEnd = true;
}

void SearchFromTable(std::vector<std::string> &outputFiles, int nowSec)
{
    const int maxInterval = 30;
    const int agingTime = 30 * 60;

    for (auto iter = g_traceFilesTable.begin(); iter != g_traceFilesTable.end();) {
        if (nowSec - iter->second >= agingTime) {
            // delete outdated trace file
            if (access(iter->first.c_str(), F_OK) == 0) {
                remove(iter->first.c_str());
                HILOG_INFO(LOG_CORE, "delete old %{public}s file success.", iter->first.c_str());
            }
            iter = g_traceFilesTable.erase(iter);
            continue;
        }

        if (nowSec - iter->second <= maxInterval) {
            outputFiles.push_back(iter->first);
        }
        iter++;
    }
}

bool ReadRawTrace(std::string &outputFileName)
{
    // read trace data from /per_cpu/cpux/trace_pipe_raw
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (outFd < 0) {
        return false;
    }
    struct TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (g_traceHmDir != "") {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    write(outFd, reinterpret_cast<char*>(&header), sizeof(header));

    if (WriteEventsFormat(outFd, outPath) && WriteCpuRaw(outFd, outPath) &&
        WriteCmdlines(outFd, outPath) && WriteTgids(outFd, outPath) &&
        WriteHeaderPage(outFd, outPath) && WritePrintkFormats(outFd, outPath) &&
        WriteKallsyms(outFd)) {
        close(outFd);
        return true;
    }
    HILOG_ERROR(LOG_CORE, "ReadRawTrace failed.");
    close(outFd);
    return false;
}

bool WaitPidTimeout(pid_t pid, const int timeoutUsec)
{
    int delayTime = timeoutUsec;
    while (delayTime > 0) {
        usleep(UNIT_TIME);
        delayTime -= UNIT_TIME;
        int status = 0;
        int ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) {
            HILOG_INFO(LOG_CORE, "wait pid(%{public}d) exit success.", pid);
            return true;
        } else if (ret < 0) {
            HILOG_ERROR(LOG_CORE, "wait pid(%{public}d) exit failed, status: %{public}d.", pid, status);
            return false;
        }
        HILOG_INFO(LOG_CORE, "grasping trace, pid(%{public}d), ret(%{public}d).", pid, ret);
    }
    HILOG_ERROR(LOG_CORE, "wait pid(%{public}d) %{public}d us timeout.", pid, timeoutUsec);
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

TraceErrorCode DumpTraceInner(std::vector<std::string> &outputFiles)
{
    g_dumpEnd = false;
    std::string outputFileName = GenerateName();
    std::string reOutPath = CanonicalizeSpecPath(outputFileName.c_str());

    /*Child process handles task, Father process wait.*/
    pid_t pid = fork();
    if (pid < 0) {
        HILOG_ERROR(LOG_CORE, "fork error.");
        g_dumpEnd = true;
        return TraceErrorCode::WRITE_TRACE_INFO_ERROR;
    }
    bool ret = false;
    if (pid == 0) {
        std::string processName = "HitraceDump";
        SetProcessName(processName);
        MarkClockSync(g_traceRootPath);
        const int waitTime = 10000; // 10ms
        usleep(waitTime);
        ReadRawTrace(reOutPath);
        HILOG_INFO(LOG_CORE, "%{public}s exit.", processName.c_str());
        _exit(EXIT_SUCCESS);
    } else {
        const int timeoutUsec = 10000000; // 10s
        bool isTrue = WaitPidTimeout(pid, timeoutUsec);
        if (isTrue && access(reOutPath.c_str(), F_OK) == 0) {
            HILOG_INFO(LOG_CORE, "Output: %{public}s.", reOutPath.c_str());
            ret = true;
        } else {
            HILOG_ERROR(LOG_CORE, "Output error: %{public}s.", reOutPath.c_str());
        }
    }

    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    int nowSec = now.tv_sec;
    SearchFromTable(outputFiles, nowSec);
    if (ret) {
        outputFiles.push_back(outputFileName);
        g_traceFilesTable.push_back({outputFileName, nowSec});
    } else {
        HILOG_ERROR(LOG_CORE, "DumpTraceInner: write %{public}s failed.", outputFileName.c_str());
        g_dumpEnd = true;
        return TraceErrorCode::WRITE_TRACE_INFO_ERROR;
    }
    g_dumpEnd = true;
    return TraceErrorCode::SUCCESS;
}

uint64_t GetSysParamTags()
{
    return OHOS::system::GetUintParameter<uint64_t>("debug.hitrace.tags.enableflags", 0);
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
        RestartService();
        return false;
    }
    HILOG_ERROR(LOG_CORE, "trace is being used, restart later.");
    return false;
}

bool CheckTraceFile()
{
    const std::string enable = "1";
    if (ReadFile("tracing_on").substr(0, enable.size()) == enable) {
        return true;
    }
    HILOG_ERROR(LOG_CORE, "tracing_on is 0, restart it.");
    RestartService();
    return false;
}

/**
 * SERVICE_MODE is running, check param and tracing_on.
*/
bool CheckServiceRunning()
{
    if (CheckParam() && CheckTraceFile()) {
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

TraceErrorCode HandleTraceOpen(const TraceParams &traceParams,
                               const std::map<std::string, TagCategory> &allTags,
                               const std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    if (!SetTraceSetting(traceParams, allTags, tagGroupTable)) {
        return TraceErrorCode::FILE_ERROR;
    }
    SetFtraceEnabled("tracing_on", true);
    g_currentTraceParams = traceParams;
    return TraceErrorCode::SUCCESS;
}

TraceErrorCode HandleServiceTraceOpen(const std::vector<std::string> &tagGroups,
                                      const std::map<std::string, TagCategory> &allTags,
                                      const std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    TraceParams serviceTraceParams;
    serviceTraceParams.tagGroups = tagGroups;
    serviceTraceParams.bufferSize = std::to_string(DEFAULT_BUFFER_SIZE);
    if (g_traceHmDir != "") {
        serviceTraceParams.bufferSize = std::to_string(HM_DEFAULT_BUFFER_SIZE);
    }
    serviceTraceParams.clockType = "boot";
    serviceTraceParams.isOverWrite = "1";
    serviceTraceParams.fileSize = std::to_string(DEFAULT_FILE_SIZE);
    return HandleTraceOpen(serviceTraceParams, allTags, tagGroupTable);
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

/**
 * args: tags:tag1,tags2... tagGroups:group1,group2... clockType:boot bufferSize:1024 overwrite:1 output:filename
 * cmdTraceParams:  Save the above parameters
*/
bool ParseArgs(const std::string &args, TraceParams &cmdTraceParams, const std::map<std::string, TagCategory> &allTags,
               const std::map<std::string, std::vector<std::string>> &tagGroupTable)
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
            cmdTraceParams.fileSize = item.substr(pos + 1);
        } else if (itemName == "fileLimit") {
            cmdTraceParams.fileLimit = item.substr(pos + 1);
        } else if (itemName == "appPid") {
            std::string pidStr = item.substr(pos + 1);
            if (!IsNumber(pidStr)) {
                HILOG_ERROR(LOG_CORE, "Illegal input, appPid(%{public}s) must be number.", pidStr.c_str());
                return false;
            }
            int appPid = std::stoi(pidStr);
            if (appPid <= 0) {
                HILOG_ERROR(LOG_CORE, "Illegal input, appPid(%{public}d) must be greater than 0.", appPid);
                return false;
            }
            OHOS::system::SetParameter("debug.hitrace.app_pid", pidStr);
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

/**
 * When the SERVICE_MODE is started, clear the remaining trace files in the folder.
*/
void ClearRemainingTrace()
{
    if (access(DEFAULT_OUTPUT_DIR.c_str(), F_OK) != 0) {
        return;
    }
    DIR* dirPtr = opendir(DEFAULT_OUTPUT_DIR.c_str());
    if (dirPtr == nullptr) {
        HILOG_ERROR(LOG_CORE, "opendir failed.");
        return;
    }
    struct dirent* ptr = nullptr;
    while ((ptr = readdir(dirPtr)) != nullptr) {
        if (ptr->d_type == DT_REG) {
            std::string name = std::string(ptr->d_name);
            if (name.compare(0, SNAPSHOT_PREFIX.size(), SNAPSHOT_PREFIX) != 0 &&
                name.compare(0, SAVED_EVENTS_FORMAT.size(), SAVED_EVENTS_FORMAT) != 0) {
                continue;
            }
            std::string subFileName = DEFAULT_OUTPUT_DIR + name;
            int dataFile = open(subFileName.c_str(), O_RDONLY | O_NONBLOCK);
            if (dataFile == -1) {
                HILOG_INFO(LOG_CORE, "open old trace file failed:  %{public}s", subFileName.c_str());
                continue;
            }
            if (flock(dataFile, LOCK_EX | LOCK_NB) < 0) {
                HILOG_INFO(LOG_CORE, "get old trace file lock failed, skip remove: %{public}s", subFileName.c_str());
                continue;
            }
            if (remove(subFileName.c_str()) == 0) {
                HILOG_INFO(LOG_CORE, "Delete old trace file: %{public}s success.", subFileName.c_str());
            } else {
                HILOG_ERROR(LOG_CORE, "Delete old trace file: %{public}s failed.", subFileName.c_str());
            }
            flock(dataFile, LOCK_UN);
            close(dataFile);
        }
    }
    closedir(dirPtr);
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

TraceErrorCode OpenTrace(const std::vector<std::string> &tagGroups)
{
    if (g_traceMode != CLOSE) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: CALL_ERROR, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return CALL_ERROR;
    }
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (!IsTraceMounted()) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }

    if (access((g_traceRootPath + "hongmeng/").c_str(), F_OK) != -1) {
        g_traceHmDir = "hongmeng/";
    }

    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }

    if (tagGroups.size() == 0 || !CheckTagGroup(tagGroups, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceErrorCode ret = HandleServiceTraceOpen(tagGroups, allTags, tagGroupTable);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: open fail.");
        return ret;
    }
    g_traceMode = SERVICE_MODE;

    ClearRemainingTrace();
    if (g_traceHmDir == "" && !g_serviceThreadIsStart) {
        // open SERVICE_MODE monitor thread
        std::thread auxiliaryTask(MonitorServiceTask);
        auxiliaryTask.detach();
    }
    g_sysInitParamTags = GetSysParamTags();
    HILOG_INFO(LOG_CORE, "OpenTrace: SERVICE_MODE open success.");
    return ret;
}

TraceErrorCode OpenTrace(const std::string &args)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != CLOSE) {
        HILOG_ERROR(LOG_CORE, "OpenTrace: CALL_ERROR, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return CALL_ERROR;
    }

    if (!IsTraceMounted()) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }
    if (access((g_traceRootPath + "hongmeng/").c_str(), F_OK) != -1) {
        g_traceHmDir = "hongmeng/";
    }

    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable) || allTags.size() == 0 || tagGroupTable.size() == 0) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }
    // parse args
    TraceParams cmdTraceParams;
    if (!ParseArgs(args, cmdTraceParams, allTags, tagGroupTable)) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceErrorCode ret = HandleTraceOpen(cmdTraceParams, allTags, tagGroupTable);
    if (ret != SUCCESS) {
        HILOG_ERROR(LOG_CORE, "Hitrace OpenTrace: CMD_MODE open failed.");
        return FILE_ERROR;
    }
    g_traceMode = CMD_MODE;
    HILOG_INFO(LOG_CORE, "Hitrace OpenTrace: CMD_MODE open success.");
    return ret;
}

TraceRetInfo DumpTrace()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    HILOG_DEBUG(LOG_CORE, "DumpTrace start.");
    TraceRetInfo ret;
    if (g_traceMode != SERVICE_MODE) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: CALL_ERROR, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        ret.errorCode = CALL_ERROR;
        return ret;
    }

    if (!CheckServiceRunning()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: TRACE_IS_OCCUPIED.");
        ret.errorCode = TRACE_IS_OCCUPIED;
        return ret;
    }

    ret.errorCode = DumpTraceInner(ret.outputFiles);
    HILOG_DEBUG(LOG_CORE, "DumpTrace done.");
    return ret;
}

TraceRetInfo DumpTrace(int timeLimit)
{
    HILOG_INFO(LOG_CORE, "DumpTrace with time limit start, time limit is %d.", timeLimit);
    TraceRetInfo ret;
    if (timeLimit <= 0) {
        HILOG_ERROR(LOG_CORE, "DumpTrace: Illegal input.");
        ret.errorCode = CALL_ERROR;
        return ret;
    }
    {
        std::lock_guard<std::mutex> lock(g_traceMutex);
        g_timeLimit = timeLimit;
    }
    ret = DumpTrace();
    {
        std::lock_guard<std::mutex> lock(g_traceMutex);
        g_timeLimit = 0;
    }
    HILOG_INFO(LOG_CORE, "DumpTrace with time limit done.");
    return ret;
}

TraceErrorCode DumpTraceOn()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    // check current trace status
    if (g_traceMode != CMD_MODE) {
        HILOG_ERROR(LOG_CORE, "DumpTraceOn: CALL_ERROR, g_traceMode:%{public}d.", static_cast<int>(g_traceMode));
        return CALL_ERROR;
    }

    // start task thread
    std::thread task(ProcessDumpTask);
    task.detach();
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
        ret.errorCode = CALL_ERROR;
        ret.outputFiles = g_outputFilesForCmd;
        return ret;
    }
    g_dumpFlag = false;
    while (!g_dumpEnd) {
        usleep(UNIT_TIME);
    }
    ret.errorCode = SUCCESS;
    ret.outputFiles = g_outputFilesForCmd;
    HILOG_INFO(LOG_CORE, "Recording trace off.");
    return ret;
}

TraceErrorCode CloseTrace()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode == CLOSE) {
        HILOG_INFO(LOG_CORE, "Trace already close.");
        return SUCCESS;
    }

    g_traceMode = CLOSE;
    // Waiting for the data drop task to end
    g_dumpFlag = false;
    while (!g_dumpEnd) {
        usleep(UNIT_TIME);
    }
    OHOS::system::SetParameter("debug.hitrace.app_pid", "-1");
    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable) || allTags.size() == 0 || tagGroupTable.size() == 0) {
        HILOG_ERROR(LOG_CORE, "CloseTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }
    TraceInit(allTags);
    TruncateFile();
    return SUCCESS;
}

std::vector<std::pair<std::string, int>> GetTraceFilesTable()
{
    return g_traceFilesTable;
}

void SetTraceFilesTable(std::vector<std::pair<std::string, int>>& traceFilesTable)
{
    g_traceFilesTable = traceFilesTable;
}

} // Hitrace
} // HiviewDFX
} // OHOS
