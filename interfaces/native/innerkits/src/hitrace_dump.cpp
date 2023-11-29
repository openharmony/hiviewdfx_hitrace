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
};

constexpr uint16_t MAGIC_NUMBER = 57161;
constexpr uint16_t VERSION_NUMBER = 1;
constexpr uint8_t FILE_RAW_TRACE = 0;
constexpr uint8_t HM_FILE_RAW_TRACE = 1;
constexpr int UNIT_TIME = 100000;
constexpr int ALIGNMENT_COEFFICIENT = 4;

const int DEFAULT_BUFFER_SIZE = 12 * 1024;
const int DEFAULT_FILE_SIZE = 100 * 1024;
const int HM_DEFAULT_BUFFER_SIZE = 60 * 1024;
const int SAVED_CMDLINES_SIZE = 2048;
const int MAX_OUTPUT_FILE_SIZE = 20;

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

std::atomic<bool> g_dumpFlag(false);
std::atomic<bool> g_dumpEnd(true);
std::mutex g_traceMutex;

bool g_serviceThreadIsStart = false;
uint64_t g_sysInitParamTags = 0;
TraceMode g_traceMode = TraceMode::CLOSE;
std::string g_traceRootPath;
std::string g_traceHmDir;
std::vector<std::pair<std::string, int>> g_traceFilesTable;
std::vector<std::string> g_outputFilesForCmd;

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
    HiLog::Error(LABEL, "IsTraceMounted: Did not find trace folder");
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
    HiLog::Info(LABEL, "reserved with arch word info is %{public}d.", header.reserved);
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
        HiLog::Error(LABEL, "error: cpu_number is %{public}d.", cpuNums);
        return;
    }
    header.reserved |= (static_cast<uint64_t>(cpuNums) << 1);
    HiLog::Info(LABEL, "reserved with cpu number info is %{public}d.", header.reserved);
}

bool CheckTags(const std::vector<std::string> &tags, const std::map<std::string, TagCategory> &allTags)
{
    for (const auto &tag : tags) {
        if (allTags.find(tag) == allTags.end()) {
            HiLog::Error(LABEL, "CheckTags: %{public}s is not provided.", tag.c_str());
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
            HiLog::Error(LABEL, "CheckTagGroup: %{public}s is not provided.", groupName.c_str());
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
        HiLog::Error(LABEL, "WriteStrToFile: %{public}s open failed.", filename.c_str());
        return false;
    }
    out << str;
    if (out.bad()) {
        HiLog::Error(LABEL, "WriteStrToFile: %{public}s write failed.", filename.c_str());
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
        HiLog::Error(LABEL, "TruncateFile: clear old trace failed.");
        return;
    }
    close(fd);
    return;
}

bool SetProperty(const std::string& property, const std::string& value)
{
    bool result = OHOS::system::SetParameter(property, value);
    if (!result) {
        HiLog::Error(LABEL, "SetProperty: set %{public}s failed.", value.c_str());
    } else {
        HiLog::Info(LABEL, "SetProperty: set %{public}s success.", value.c_str());
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
            HiLog::Error(LABEL, "SetAllTags: default group is wrong.");
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
            HiLog::Error(LABEL, "tag<%{public}s> is invalid.", tagName.c_str());
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
        HiLog::Error(LABEL, "ReadFile: %{public}s open failed.", filename.c_str());
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
        HiLog::Error(LABEL, "SetClock: %{public}s is non-existent, set to boot", clockType.c_str());
        WriteStrToFile(traceClockPath, "boot"); // set default: boot
        return;
    }

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
            HiLog::Info(LABEL, "SetClock: set clock %{public}s success.", clockType.c_str());
            WriteStrToFile(traceClockPath, clockType);
            return;
        }
        if (i[0] == '[') {
            currentClockType = i;
        }
    }
    if (currentClockType.size() == 0) {
        HiLog::Info(LABEL, "SetClock: clockType is boot now.");
        return;
    }
    const int marks = 2;
    if (clockType.compare(currentClockType.substr(1, currentClockType.size() - marks)) == 0) {
        HiLog::Info(LABEL, "SetClock: set clock %{public}s success.", clockType.c_str());
        return;
    }

    HiLog::Info(LABEL, "SetClock: unknown %{public}s, change to default clock_type: boot.", clockType.c_str());
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

size_t GetFileSize(const std::string &fileName)
{
    if (fileName.empty()) {
        return 0;
    }
    if (access(fileName.c_str(), 0) == -1) {
        return 0;
    }

    struct stat statbuf;
    stat(fileName.c_str(), &statbuf);
    return statbuf.st_size;
}

bool WriteFile(uint8_t contentType, const std::string &src, int outFd)
{
    std::string srcPath = CanonicalizeSpecPath(src.c_str());
    int srcFd = open(srcPath.c_str(), O_RDONLY | O_NONBLOCK);
    if (srcFd < 0) {
        HiLog::Error(LABEL, "WriteFile: open %{public}s failed.", src.c_str());
        return false;
    }
    struct TraceFileContentHeader contentHeader;
    contentHeader.type = contentType;
    write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    uint32_t readLen = 0;
    uint8_t buffer[PAGE_SIZE] = {0};
    const int pageThreshold = PAGE_SIZE / 2;
    PageHeader *pageHeader = nullptr;
    int count = 0;
    const int maxCount = 2;
    while (true) {
        ssize_t readBytes = TEMP_FAILURE_RETRY(read(srcFd, buffer, PAGE_SIZE));
        if (readBytes <= 0) {
            break;
        }
        write(outFd, buffer, readBytes);
        readLen += readBytes;

        // Check raw_trace page size.
        if (contentType >= CONTENT_TYPE_CPU_RAW && g_traceHmDir == "") {
            pageHeader = reinterpret_cast<PageHeader*>(&buffer);
            if (pageHeader->size < static_cast<uint64_t>(pageThreshold)) {
                count++;
            }
            if (count >= maxCount) {
                break;
            }
        }
    }
    contentHeader.length = readLen;
    uint32_t offset = contentHeader.length + sizeof(contentHeader);
    off_t pos = lseek(outFd, 0, SEEK_CUR);
    lseek(outFd, pos - offset, SEEK_SET);
    write(outFd, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    lseek(outFd, pos, SEEK_SET);
    close(srcFd);
    return true;
}

void WriteEventFile(std::string &srcPath, int outFd)
{
    uint8_t buffer[PAGE_SIZE] = {0};
    std::string srcSpecPath = CanonicalizeSpecPath(srcPath.c_str());
    int srcFd = open(srcSpecPath.c_str(), O_RDONLY);
    if (srcFd < 0) {
        HiLog::Error(LABEL, "WriteEventsFormat: open %{public}s failed.", srcPath.c_str());
        return;
    }
    do {
        int len = read(srcFd, buffer, PAGE_SIZE);
        if (len <= 0) {
            break;
        }
        write(outFd, buffer, len);
    } while (true);
    close(srcFd);
}

bool WriteEventsFormat(int outFd)
{
    const std::string savedEventsFormatPath = DEFAULT_OUTPUT_DIR + SAVED_EVENTS_FORMAT;
    if (access(savedEventsFormatPath.c_str(), F_OK) != -1) {
        return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, savedEventsFormatPath, outFd);
    }
    std::string filePath = CanonicalizeSpecPath(savedEventsFormatPath.c_str());
    int fd = open(filePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        HiLog::Error(LABEL, "WriteEventsFormat: open %{public}s failed.", savedEventsFormatPath.c_str());
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
    return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, filePath, outFd);
}

bool WriteHeaderPage(int outFd)
{
    if (g_traceHmDir != "") {
        return true;
    }
    std::string headerPagePath = GetFilePath("events/header_page");
    return WriteFile(CONTENT_TYPE_HEADER_PAGE, headerPagePath, outFd);
}

bool WritePrintkFormats(int outFd)
{
    if (g_traceHmDir != "") {
        return true;
    }
    std::string printkFormatPath = GetFilePath("printk_formats");
    return WriteFile(CONTENT_TYPE_PRINTK_FORMATS, printkFormatPath, outFd);
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

bool HmWriteCpuRawInner(int outFd)
{
    uint8_t type = CONTENT_TYPE_CPU_RAW;
    std::string src = g_traceRootPath + "hongmeng/trace_pipe_raw";

    return WriteFile(type, src, outFd);
}

bool WriteCpuRawInner(int outFd)
{
    int cpuNums = GetCpuProcessors();
    int ret = true;
    uint8_t type = CONTENT_TYPE_CPU_RAW;
    for (int i = 0; i < cpuNums; i++) {
        std::string src = g_traceRootPath + "per_cpu/cpu" + std::to_string(i) + "/trace_pipe_raw";
        if (!WriteFile(static_cast<uint8_t>(type + i), src, outFd)) {
            ret = false;
            break;
        }
    }
    return ret;
}

bool WriteCpuRaw(int outFd)
{
    if (g_traceHmDir == "") {
        return WriteCpuRawInner(outFd);
    } else {
        return HmWriteCpuRawInner(outFd);
    }
}

bool WriteCmdlines(int outFd)
{
    if (g_traceHmDir != "") {
        return true;
    }
    std::string cmdlinesPath = GetFilePath("saved_cmdlines");
    return WriteFile(CONTENT_TYPE_CMDLINES, cmdlinesPath, outFd);
}

bool WriteTgids(int outFd)
{
    if (g_traceHmDir != "") {
        return true;
    }
    std::string tgidsPath = GetFilePath("saved_tgids");
    return WriteFile(CONTENT_TYPE_TGIDS, tgidsPath, outFd);
}

bool DumpTraceLoop(const std::string &outputFileName, bool isLimited)
{
    const int sleepTime = 1;
    if (g_currentTraceParams.fileSize.empty()) {
        g_currentTraceParams.fileSize = std::to_string(DEFAULT_FILE_SIZE);
    }
    const int fileSizeThreshold = std::stoi(g_currentTraceParams.fileSize) * 1024;
    std::string outPath = CanonicalizeSpecPath(outputFileName.c_str());
    int outFd = open(outPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (outFd < 0) {
        HiLog::Error(LABEL, "open %{public}s failed, errno: %{public}d.", outPath.c_str(), errno);
        return false;
    }
    MarkClockSync(g_traceRootPath);
    struct TraceFileHeader header;
    GetArchWordSize(header);
    GetCpuNums(header);
    if (g_traceHmDir != "") {
        header.fileType = HM_FILE_RAW_TRACE;
    }
    write(outFd, reinterpret_cast<char*>(&header), sizeof(header));
    WriteEventsFormat(outFd);
    while (g_dumpFlag) {
        if (isLimited && GetFileSize(outPath) > static_cast<size_t>(fileSizeThreshold)) {
            break;
        }
        sleep(sleepTime);
        WriteCpuRaw(outFd);
    }
    WriteCmdlines(outFd);
    WriteTgids(outFd);
    WriteHeaderPage(outFd);
    WritePrintkFormats(outFd);
    WriteKallsyms(outFd);
    close(outFd);
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
        HiLog::Error(LABEL, "Get localtime failed.");
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
    HiLog::Info(LABEL, "output trace: %{public}s, boot_time(%{public}" PRId64 "), mono_time(%{public}" PRId64 ").",
                name.c_str(), static_cast<int64_t>(bts.tv_sec), static_cast<int64_t>(mts.tv_sec));
    return name;
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
    if (g_currentTraceParams.outputFile.size() > 0) {
        if (DumpTraceLoop(g_currentTraceParams.outputFile, false)) {
            g_outputFilesForCmd.push_back(g_currentTraceParams.outputFile);
        }
        g_dumpEnd = true;
        return;
    }
    
    while (g_dumpFlag) {
        if (g_outputFilesForCmd.size() >= MAX_OUTPUT_FILE_SIZE && access(g_outputFilesForCmd[0].c_str(), F_OK) == 0) {
            if (remove(g_outputFilesForCmd[0].c_str()) == 0) {
                g_outputFilesForCmd.erase(g_outputFilesForCmd.begin());
                HiLog::Info(LABEL, "delete first: %{public}s success.", g_outputFilesForCmd[0].c_str());
            } else {
                HiLog::Error(LABEL, "delete first: %{public}s failed.", g_outputFilesForCmd[0].c_str());
            }
        }
        // Generate file name
        std::string outputFileName = GenerateName(false);
        if (DumpTraceLoop(outputFileName, true)) {
            g_outputFilesForCmd.push_back(outputFileName);
        } else {
            break;
        }
    }
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
                HiLog::Info(LABEL, "delete old %{public}s file success.", iter->first.c_str());
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

    if (WriteEventsFormat(outFd) && WriteCpuRaw(outFd) &&
        WriteCmdlines(outFd) && WriteTgids(outFd) &&
        WriteHeaderPage(outFd) && WritePrintkFormats(outFd) &&
        WriteKallsyms(outFd)) {
        close(outFd);
        return true;
    }
    HiLog::Error(LABEL, "ReadRawTrace failed.");
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
            HiLog::Info(LABEL, "wait pid(%{public}d) exit success.", pid);
            return true;
        } else if (ret < 0) {
            HiLog::Error(LABEL, "wait pid(%{public}d) exit failed, status: %{public}d.", pid, status);
            return false;
        }
    }
    HiLog::Error(LABEL, "wait pid(%{public}d) %{public}d us timeout.", pid, timeoutUsec);
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
    HiLog::Info(LABEL, "New process: %{public}s.", setName.c_str());
}

TraceErrorCode DumpTraceInner(std::vector<std::string> &outputFiles)
{
    g_dumpEnd = false;
    std::string outputFileName = GenerateName();
    std::string reOutPath = CanonicalizeSpecPath(outputFileName.c_str());

    /*Child process handles task, Father process wait.*/
    pid_t pid = fork();
    if (pid < 0) {
        HiLog::Error(LABEL, "fork error.");
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
        HiLog::Info(LABEL, "%{public}s exit.", processName.c_str());
        _exit(EXIT_SUCCESS);
    } else {
        const int timeoutUsec = 3000000; // 3s
        bool isTrue = WaitPidTimeout(pid, timeoutUsec);
        if (isTrue && access(reOutPath.c_str(), F_OK) == 0) {
            HiLog::Info(LABEL, "Output: %{public}s.", reOutPath.c_str());
            ret = true;
        } else {
            HiLog::Error(LABEL, "Output error: %{public}s.", reOutPath.c_str());
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
        HiLog::Error(LABEL, "DumpTraceInner: write %{public}s failed.", outputFileName.c_str());
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
        HiLog::Error(LABEL, "tag is 0, restart it.");
        RestartService();
        return false;
    }
    HiLog::Error(LABEL, "trace is being used, restart later.");
    return false;
}

bool CheckTraceFile()
{
    const std::string enable = "1";
    if (ReadFile("tracing_on").substr(0, enable.size()) == enable) {
        return true;
    }
    HiLog::Error(LABEL, "tracing_on is 0, restart it.");
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
    HiLog::Info(LABEL, "MonitorServiceTask: monitor thread start.");
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
            HiLog::Error(LABEL, "CalculateAllNewBufferSize failed.");
            break;
        }

        for (size_t i = 0; i < result.size(); i++) {
            HiLog::Debug(LABEL, "cpu%{public}zu set size %{public}d.", i, result[i]);
            std::string path = "per_cpu/cpu" + std::to_string(i) + "/buffer_size_kb";
            WriteStrToFile(path, std::to_string(result[i]));
        }
    }
    HiLog::Info(LABEL, "MonitorServiceTask: monitor thread exit."); 
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
            HiLog::Error(LABEL, "trace command line without colon appears: %{public}s, continue.", item.c_str());
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
        } else {
            HiLog::Error(LABEL, "Extra trace command line options appear when ParseArgs: %{public}s, return false.",
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
        HiLog::Error(LABEL, "opendir failed.");
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
            if (remove(subFileName.c_str()) == 0) {
                HiLog::Info(LABEL, "Delete old trace file: %{public}s success.", subFileName.c_str());
            } else {
                HiLog::Error(LABEL, "Delete old trace file: %{public}s failed.", subFileName.c_str());
            }
        }
    }
    closedir(dirPtr);
}

} // namespace


TraceMode GetTraceMode()
{
    return g_traceMode;
}

TraceErrorCode OpenTrace(const std::vector<std::string> &tagGroups)
{
    if (g_traceMode != CLOSE) {
        HiLog::Error(LABEL, "OpenTrace: CALL_ERROR.");
        return CALL_ERROR;
    }
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (!IsTraceMounted()) {
        HiLog::Error(LABEL, "OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }

    if (access((g_traceRootPath + "hongmeng/").c_str(), F_OK) != -1) {
        g_traceHmDir = "hongmeng/";
    }

    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable)) {
        HiLog::Error(LABEL, "OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }

    if (tagGroups.size() == 0 || !CheckTagGroup(tagGroups, tagGroupTable)) {
        HiLog::Error(LABEL, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceErrorCode ret = HandleServiceTraceOpen(tagGroups, allTags, tagGroupTable);
    if (ret != SUCCESS) {
        HiLog::Error(LABEL, "OpenTrace: open fail.");
        return ret;
    }
    g_traceMode = SERVICE_MODE;

    ClearRemainingTrace();
    if (!g_serviceThreadIsStart) {
        // open SERVICE_MODE monitor thread
        std::thread auxiliaryTask(MonitorServiceTask);
        auxiliaryTask.detach();
    }
    g_sysInitParamTags = GetSysParamTags();
    HiLog::Info(LABEL, "OpenTrace: SERVICE_MODE open success.");
    return ret;
}

TraceErrorCode OpenTrace(const std::string &args)
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode != CLOSE) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: CALL_ERROR.");
        return CALL_ERROR;
    }

    if (!IsTraceMounted()) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }
    if (access((g_traceRootPath + "hongmeng/").c_str(), F_OK) != -1) {
        g_traceHmDir = "hongmeng/";
    }

    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable) || allTags.size() == 0 || tagGroupTable.size() == 0) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }
    // parse args
    TraceParams cmdTraceParams;
    if (!ParseArgs(args, cmdTraceParams, allTags, tagGroupTable)) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    TraceErrorCode ret = HandleTraceOpen(cmdTraceParams, allTags, tagGroupTable);
    if (ret != SUCCESS) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: CMD_MODE open failed.");
        return FILE_ERROR;
    }
    g_traceMode = CMD_MODE;
    HiLog::Info(LABEL, "Hitrace OpenTrace: CMD_MODE open success.");
    return ret;
}

TraceRetInfo DumpTrace()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    HiLog::Debug(LABEL, "DumpTrace start.");
    TraceRetInfo ret;
    if (g_traceMode != SERVICE_MODE) {
        HiLog::Error(LABEL, "DumpTrace: CALL_ERROR.");
        ret.errorCode = CALL_ERROR;
        return ret;
    }

    if (!CheckServiceRunning()) {
        HiLog::Error(LABEL, "DumpTrace: TRACE_IS_OCCUPIED.");
        ret.errorCode = TRACE_IS_OCCUPIED;
        return ret;
    }

    ret.errorCode = DumpTraceInner(ret.outputFiles);
    HiLog::Debug(LABEL, "DumpTrace done.");
    return ret;
}

TraceErrorCode DumpTraceOn()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    // check current trace status
    if (g_traceMode != CMD_MODE) {
        HiLog::Error(LABEL, "DumpTraceOn: CALL_ERROR.");
        return CALL_ERROR;
    }

    // start task thread
    std::thread task(ProcessDumpTask);
    task.detach();
    HiLog::Info(LABEL, "Recording trace on.");
    return SUCCESS;
}

TraceRetInfo DumpTraceOff()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    TraceRetInfo ret;
    // check current trace status
    if (g_traceMode != CMD_MODE) {
        HiLog::Error(LABEL, "DumpTraceOff: The current state is not Recording, data exception.");
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
    HiLog::Info(LABEL, "Recording trace off.");
    return ret;
}

TraceErrorCode CloseTrace()
{
    std::lock_guard<std::mutex> lock(g_traceMutex);
    if (g_traceMode == CLOSE) {
        HiLog::Info(LABEL, "Trace already close.");
        return SUCCESS;
    }

    g_traceMode = CLOSE;
    // Waiting for the data drop task to end
    g_dumpFlag = false;
    while (!g_dumpEnd) {
        usleep(UNIT_TIME);
    }
    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable) || allTags.size() == 0 || tagGroupTable.size() == 0) {
        HiLog::Error(LABEL, "CloseTrace: ParseTagInfo TAG_ERROR.");
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