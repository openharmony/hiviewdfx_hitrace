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

#include <map>
#include <atomic>
#include <fstream>
#include <set>
#include <thread>
#include <vector>
#include <string>

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cinttypes>
#include <utility>
#include <json/json.h>

#include "parameters.h"
#include "hilog/log.h"
#include "securec.h"


using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::Hitrace::TraceErrorCode;
using OHOS::HiviewDFX::Hitrace::TraceRetInfo;
using OHOS::HiviewDFX::Hitrace::TraceMode;

namespace {

struct TagCategory {
    std::string description;
    uint64_t tag;
    int type;
    std::vector<std::string> sysFiles;
};

struct TraceParams {
    std::vector<std::string> tags;
    std::vector<std::string> tagGroups;
    std::string bufferSize;
    std::string clockType;
    std::string isOverWrite;
    std::string outputFile;
};

constexpr uint16_t MAGIC_NUMBER = 57161;
constexpr uint16_t VERSION_NUMBER = 1;
constexpr uint8_t FILE_RAW_TRACE = 0;
constexpr int UNIT_TIME = 100;

const int DEFAULT_BUFFER_SIZE = 12 * 1024;
const int HIGHER_BUFFER_SIZE = 18 * 1024;

const std::string DEFAULT_OUTPUT_DIR = "/data/log/hitrace/";
const std::string LOG_DIR = "/data/log/";

struct TraceFileHeader {
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
    CONTENT_TYPE_CPU_RAW = 4
};

struct TraceFileContentHeader {
    uint8_t type = CONTENT_TYPE_DEFAULT;
    uint32_t length = 0;
};

struct LastCpuInfo {
    std::pair<uint64_t, uint64_t> idleAndTotal = {0, 0};
    bool isNormal = true;
};

struct CpuStat {
    int8_t cpuId = -1; // 总的为-1
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;
    uint64_t guest = 0;
    uint64_t guestNice = 0;
};

const size_t PAGE_SIZE = getpagesize();
constexpr uint64_t HITRACE_TAG = 0xD002D33;
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HITRACE_TAG, "HitraceDump"};
std::atomic<bool> g_dumpFlag(false);
std::atomic<bool> g_dumpEnd(true);
bool g_monitor = false; // close service monitor for now
TraceMode g_traceMode = TraceMode::CLOSE;
std::string g_traceRootPath;
std::vector<std::string> g_outputFilesForCmd;

TraceParams g_currentTraceParams = {};

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

bool ParseTagInfo(std::map<std::string, TagCategory> &allTags,
                  std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    std::string traceUtilsPath = "/system/etc/hiview/hitrace_utils.json";
    std::ifstream inFile(traceUtilsPath, std::ios::in);
    if (!inFile.is_open()) {
        HiLog::Error(LABEL, "ParseTagInfo: %{pubilc}s is not existed.", traceUtilsPath.c_str());
        return false;
    }
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(inFile, root)) {
        HiLog::Error(LABEL, "ParseTagInfo: %{pubilc}s is not in JSON format.", traceUtilsPath.c_str());
        return false;
    }
    // parse tag_category
    Json::Value::Members tags = root["tag_category"].getMemberNames();
    for (Json::Value::Members::iterator it = tags.begin(); it != tags.end(); it++) {
        Json::Value tagInfo = root["tag_category"][*it];
        TagCategory tagCategory;
        tagCategory.description = tagInfo["description"].asString();
        tagCategory.tag = 1ULL << tagInfo["tag_offset"].asInt();
        tagCategory.type = tagInfo["type"].asInt();
        for (unsigned int i = 0; i < tagInfo["sysFiles"].size(); i++) {
            tagCategory.sysFiles.push_back(tagInfo["sysFiles"][i].asString());
        }
        allTags.insert(std::pair<std::string, TagCategory>(*it, tagCategory));
    }

    // parse tagGroup
    Json::Value::Members tagGroups = root["tag_groups"].getMemberNames();
    for (Json::Value::Members::iterator it = tagGroups.begin(); it != tagGroups.end(); it++) {
        std::string tagGroupName = *it;
        std::vector<std::string> tagList;
        for (unsigned int i = 0; i < root["tag_groups"][tagGroupName].size(); i++) {
            tagList.push_back(root["tag_groups"][tagGroupName][i].asString());
        }
        tagGroupTable.insert(std::pair<std::string, std::vector<std::string>>(tagGroupName, tagList));
    }
    HiLog::Info(LABEL, "ParseTagInfo: parse done.");
    return true;
}

bool CheckTags(const std::vector<std::string> &tags, const std::map<std::string, TagCategory> &allTags)
{
    for (auto tag : tags) {
        if (allTags.find(tag) == allTags.end()) {
            HiLog::Error(LABEL, "CheckTags: %{pubilc}s is not provided.", tag.c_str());
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
            HiLog::Error(LABEL, "CheckTagGroup: %{pubilc}s is not provided.", groupName.c_str());
            return false;
        }
    }
    return true;
}

bool WriteStrToFile(const std::string& filename, const std::string& str)
{
    std::ofstream out;
    out.open(g_traceRootPath + filename, std::ios::out);
    if (out.fail()) {
        HiLog::Error(LABEL, "WriteStrToFile: %{pubilc}s open failed.", filename.c_str());
        return false;
    }
    out << str;
    if (out.bad()) {
        HiLog::Error(LABEL, "WriteStrToFile: %{pubilc}s write failed.", filename.c_str());
        out.close();
        return false;
    }
    out.flush();
    out.close();
    return true;
}

void SetFtraceEnabled(const std::string &path, bool enabled)
{
    WriteStrToFile(path, enabled ? "1" : "0");
}

void TruncateFile()
{
    int fd = creat((g_traceRootPath + "trace").c_str(), 0);
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
        HiLog::Error(LABEL, "SetProperty: set %{pubilc}s failed.", value.c_str());
    } else {
        HiLog::Info(LABEL, "SetProperty: set %{pubilc}s success.", value.c_str());
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
            continue;
        }

        if (iter->second.type == 0) {
            enabledUserTags |= iter->second.tag;
        }

        if (iter->second.type == 1) {
            for (const auto& path : iter->second.sysFiles) {
                SetFtraceEnabled(path, true);
                HiLog::Info(LABEL, "Ftrace Enabled: set %{pubilc}s success.", path.c_str());
            }
        }
    }
    SetProperty("debug.hitrace.tags.enableflags", std::to_string(enabledUserTags));
}

std::string CanonicalizeSpecPath(const char* src)
{
    if (src == nullptr || strlen(src) >= PATH_MAX) {
        HiLog::Error(LABEL, "CanonicalizeSpecPath: %{pubilc}s failed.", src);
        return "";
    }
    char resolvedPath[PATH_MAX] = { 0 };
#if defined(_WIN32)
    if (!_fullpath(resolvedPath, src, PATH_MAX)) {
        return "";
    }
#else
    if (access(src, F_OK) == 0) {
        if (realpath(src, resolvedPath) == nullptr) {
            HiLog::Error(LABEL, "CanonicalizeSpecPath: realpath %{pubilc}s failed.", src);
            return "";
        }
    } else {
        std::string fileName(src);
        if (fileName.find("..") == std::string::npos) {
            if (sprintf_s(resolvedPath, PATH_MAX, "%s", src) == -1) {
                HiLog::Error(LABEL, "CanonicalizeSpecPath: sprintf_s %{pubilc}s failed.", src);
                return "";
            }
        } else {
            HiLog::Error(LABEL, "CanonicalizeSpecPath: find .. src failed.");
            return "";
        }
    }
#endif
    std::string res(resolvedPath);
    return res;
}

std::string ReadFile(const std::string& filename)
{
    std::string resolvedPath = CanonicalizeSpecPath((g_traceRootPath + filename).c_str());
    std::ifstream fileIn(resolvedPath.c_str());
    if (!fileIn.is_open()) {
        HiLog::Error(LABEL, "ReadFile: %{pubilc}s open failed.", filename.c_str());
        return "";
    }

    std::string str((std::istreambuf_iterator<char>(fileIn)), std::istreambuf_iterator<char>());
    fileIn.close();
    return str;
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
        HiLog::Error(LABEL, "SetClock: %{pubilc}s is non-existent, set to boot", clockType.c_str());
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

    WriteStrToFile("options/record-tgid", "1");
    WriteStrToFile("options/record-cmd", "1");

    if (traceParams.outputFile.size() > 0) {
        const mode_t defaultMode = S_IRUSR | S_IWUSR | S_IRGRP;
        int fd = creat(traceParams.outputFile.c_str(), defaultMode);
        if (fd == -1) {
            HiLog::Error(LABEL, "SetTraceSetting: create %{public}s failed.", traceParams.outputFile.c_str());
            return false;
        } else {
            close(fd);
        }
    }
    return true;
}

int GetCpuProcessors()
{
    int processors = 0;
    processors = sysconf(_SC_NPROCESSORS_ONLN);
    return (processors == 0) ? 1 : processors;
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
    uint8_t buffer[PAGE_SIZE];
    const int maxReadSize = DEFAULT_BUFFER_SIZE * 1024;
    while (readLen < maxReadSize) {
        ssize_t readBytes = TEMP_FAILURE_RETRY(read(srcFd, buffer, PAGE_SIZE));
        if (readBytes <= 0) {
            break;
        }
        write(outFd, buffer, readBytes);
        readLen += readBytes;
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

bool WriteEventsFormat(int outFd)
{
    const std::string savedEventsFormatPath = DEFAULT_OUTPUT_DIR + "/saved_events_format";
    if (access(savedEventsFormatPath.c_str(), F_OK) != -1) {
        return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, savedEventsFormatPath, outFd);
    }
    int fd = open(savedEventsFormatPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        HiLog::Error(LABEL, "WriteEventsFormat: open %{public}s failed.", savedEventsFormatPath.c_str());
        return false;
    }
    const std::vector<std::string> priorityTracingCategory = {
        "events/sched/sched_waking/format",
        "events/sched/sched_wakeup/format",
        "events/sched/sched_switch/format",
        "events/sched/sched_blocked_reason/format",
        "events/power/cpu_frequency/format",
        "events/power/clock_set_rate/format",
        "events/power/cpu_frequency_limits/format",
        "events/f2fs/f2fs_sync_file_enter/format",
        "events/f2fs/f2fs_sync_file_exit/format",
        "events/ext4/ext4_da_write_begin/format",
        "events/ext4/ext4_da_write_end/format",
        "events/ext4/ext4_sync_file_enter/format",
        "events/ext4/ext4_sync_file_exit/format",
        "events/block/block_rq_issue/format",
        "events/block/block_rq_complete/format",
        "events/binder/binder_transaction/format",
        "events/binder/binder_transaction_received/format",
    };
    uint8_t buffer[PAGE_SIZE];
    for (size_t i = 0; i < priorityTracingCategory.size(); i++) {
        std::string srcPath = g_traceRootPath + priorityTracingCategory[i];
        std::string srcSpecPath = CanonicalizeSpecPath(srcPath.c_str());
        int srcFd = open(srcSpecPath.c_str(), O_RDONLY);
        if (srcFd < 0) {
            HiLog::Error(LABEL, "WriteEventsFormat: open %{public}s failed.", srcPath.c_str());
            continue;
        }
        do {
            int len = read(srcFd, buffer, PAGE_SIZE);
            if (len <= 0) {
                close(srcFd);
                break;
            }
            write(fd, buffer, len);
        } while (true);
    }
    close(fd);
    return WriteFile(CONTENT_TYPE_EVENTS_FORMAT, savedEventsFormatPath, outFd);
}

bool WriteCpuRaw(int outFd)
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

bool WriteCmdlines(int outFd)
{
    std::string cmdlinesPath = g_traceRootPath + "saved_cmdlines";
    return WriteFile(CONTENT_TYPE_CMDLINES, cmdlinesPath, outFd);
}

bool WriteTgids(int outFd)
{
    std::string tgidsPath = g_traceRootPath + "saved_tgids";
    return WriteFile(CONTENT_TYPE_TGIDS, tgidsPath, outFd);
}

bool DumpTraceLoop(const std::string &outputFileName, bool isLimited)
{
    const int sleepTime = 1;
    const int fileSizeThreshold = 96 * 1024 * 1024;
    int outFd = open(outputFileName.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (outFd < 0) {
        return false;
    }
    struct TraceFileHeader header;
    write(outFd, reinterpret_cast<char*>(&header), sizeof(header));
    while (g_dumpFlag) {
        if (isLimited && GetFileSize(outputFileName) > fileSizeThreshold) {
            break;
        }
        sleep(sleepTime);
        WriteCpuRaw(outFd);
    }
    WriteCmdlines(outFd);
    WriteTgids(outFd);
    WriteEventsFormat(outFd);
    close(outFd);
    return true;
}

/**
 * read trace data loop
 * g_dumpFlag: true = open，false = close
 * g_dumpEnd: true = end，false = not end
 * if user has own output file, Output all data to the file specified by the user;
 * if not, Then place all the result files in/data/local/tmp/and package them once in 96M.
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
        return;
    }
    
    while (g_dumpFlag) {
        // Generate file name
        struct timeval now = {0, 0};
        gettimeofday(&now, nullptr);
        std::string outputFileName = "/data/local/tmp/trace_" + std::to_string(now.tv_sec) + ".sys";
        if (DumpTraceLoop(outputFileName, true)) {
            g_outputFilesForCmd.push_back(outputFileName);
        }
    }
    g_dumpEnd = true;
}

void SearchFromTable(std::vector<std::string> &outputFiles, int nowSec)
{
    const int maxInterval = 20;
    const int agingTime = 30 * 60;

    for (auto iter = OHOS::HiviewDFX::Hitrace::g_traceFilesTable.begin();
            iter != OHOS::HiviewDFX::Hitrace::g_traceFilesTable.end();) {
        if (nowSec - iter->second >= agingTime) {
            // delete outdated trace file
            if (access(iter->first.c_str(), F_OK) == 0) {
                remove(iter->first.c_str());
                HiLog::Info(LABEL, "delete old %{public}s file success.", iter->first.c_str());
            }
            iter = OHOS::HiviewDFX::Hitrace::g_traceFilesTable.erase(iter);
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
    int outFd = open(outputFileName.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (outFd < 0) {
        return false;
    }
    struct TraceFileHeader header;
    write(outFd, reinterpret_cast<char*>(&header), sizeof(header));
    if (!WriteCpuRaw(outFd)) {
        HiLog::Error(LABEL, "WriteCpuRaw failed.");
        close(outFd);
        return false;
    }
    WriteCmdlines(outFd);
    WriteTgids(outFd);
    WriteEventsFormat(outFd);
    close(outFd);
    return true;
}

TraceErrorCode DumpTraceInner(std::vector<std::string> &outputFiles)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    int nowSec = now.tv_sec;
    int nowUsec = now.tv_usec;
    if (!g_dumpEnd) {
        const int maxSleepTime = 10 * 2000; // 2s
        int cur = 0;
        while (!g_dumpEnd && cur < maxSleepTime) {
            cur += 1;
            usleep(UNIT_TIME);
        }
        SearchFromTable(outputFiles, nowSec);
        if (outputFiles.size() == 0) {
            return TraceErrorCode::WRITE_TRACE_INFO_ERROR;
        }
        return TraceErrorCode::SUCCESS;
    }
    g_dumpEnd = false;
    // create DEFAULT_OUTPUT_DIR dir
    if (access(DEFAULT_OUTPUT_DIR.c_str(), F_OK) != 0) {
        if (access(LOG_DIR.c_str(), F_OK) != 0) {
            mkdir(LOG_DIR.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
        }
        mkdir(DEFAULT_OUTPUT_DIR.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH);
        HiLog::Info(LABEL, "DumpTraceInner: creat %{public}s success.", DEFAULT_OUTPUT_DIR.c_str());
    }

    std::string outputFileName = DEFAULT_OUTPUT_DIR + "trace_" + std::to_string(nowSec)
                                 + "_" + std::to_string(nowUsec) + ".sys";
    std::string reOutPath = CanonicalizeSpecPath(outputFileName.c_str());
    bool ret = ReadRawTrace(reOutPath);

    SearchFromTable(outputFiles, nowSec);
    if (ret) {
        outputFiles.push_back(outputFileName);
        OHOS::HiviewDFX::Hitrace::g_traceFilesTable.push_back({outputFileName, nowSec});
    } else {
        HiLog::Error(LABEL, "DumpTraceInner: write %{public}s failed.", outputFileName.c_str());
        g_dumpEnd = true;
        return TraceErrorCode::WRITE_TRACE_INFO_ERROR;
    }
    g_dumpEnd = true;
    return TraceErrorCode::SUCCESS;
}

void AdjustInner(CpuStat &cpuStat, LastCpuInfo &lastCpuInfo, int i)
{
    const int cpuUsageThreshold = 80;
    const int percentage = 100;
    uint64_t totalCpuTime = cpuStat.user + cpuStat.nice + cpuStat.system + cpuStat.idle + cpuStat.iowait +
                            cpuStat.irq + cpuStat.softirq;
    uint64_t cpuUsage = percentage - percentage * (cpuStat.idle - lastCpuInfo.idleAndTotal.first) /
                        (totalCpuTime - lastCpuInfo.idleAndTotal.second);
    if (cpuUsage >= cpuUsageThreshold && lastCpuInfo.isNormal) {
        std::string subPath = "per_cpu/cpu" + std::to_string(i) + "/buffer_size_kb";
        WriteStrToFile(subPath, std::to_string(HIGHER_BUFFER_SIZE));
        lastCpuInfo.isNormal = false;
    }
    if (!lastCpuInfo.isNormal && cpuUsage < cpuUsageThreshold) {
        std::string subPath = "per_cpu/cpu" + std::to_string(i) + "/buffer_size_kb";
        WriteStrToFile(subPath, std::to_string(DEFAULT_BUFFER_SIZE));
        lastCpuInfo.isNormal = true;
    }
    lastCpuInfo.idleAndTotal.first = cpuStat.idle;
    lastCpuInfo.idleAndTotal.second = totalCpuTime;
}

bool CpuTraceBufferSizeAdjust(std::vector<LastCpuInfo> &lastData, const int cpuNums)
{
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        HiLog::Error(LABEL, "CpuTraceBufferSizeAdjust: open /proc/stat failed.");
        return false;
    }
    std::string data;
    std::vector<CpuStat> cpuStats;
    
    const int pos = 3;
    const int formatNumber = 10;
    while (std::getline(statFile, data)) {
        if (data.substr(0, pos) == "cpu" && data[pos] != ' ') {
            CpuStat cpuStat = {};
            int ret = sscanf_s(data.c_str(), "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &cpuStat.user,
                               &cpuStat.nice, &cpuStat.system, &cpuStat.idle, &cpuStat.iowait, &cpuStat.irq,
                               &cpuStat.softirq, &cpuStat.steal, &cpuStat.guest, &cpuStat.guestNice);
            if (ret != formatNumber) {
                HiLog::Error(LABEL, "CpuTraceBufferSizeAdjust: format error.");
                return false;
            }
            cpuStats.push_back(cpuStat);
        }
    }
    statFile.close();
    if (cpuNums != cpuStats.size()) {
        HiLog::Error(LABEL, "CpuTraceBufferSizeAdjust: read /proc/stat error.");
        return false;
    }
    for (size_t i = 0; i < cpuStats.size(); i++) {
        AdjustInner(cpuStats[i], lastData[i], i);
    }
    return true;
}

void MonitorServiceTask()
{
    HiLog::Info(LABEL, "MonitorServiceTask: monitor thread start.");
    const int maxServiceTimes = 3 * 1000 * 1000 / UNIT_TIME; // 3s
    int curServiceTimes = 0;
    const int cpuNums = GetCpuProcessors();
    std::vector<LastCpuInfo> lastData;
    for (int i = 0; i < cpuNums; i++) {
        lastData.push_back({{0, 0}, true});
    }

    while (true) {
        if (g_traceMode != TraceMode::SERVICE_MODE || !g_monitor) {
            HiLog::Info(LABEL, "MonitorServiceTask: monitor thread exit.");
            break;
        }
        if (curServiceTimes >= maxServiceTimes) {
            // trace ringbuffer dynamic tuning
            if (!CpuTraceBufferSizeAdjust(lastData, cpuNums)) {
                HiLog::Info(LABEL, "MonitorServiceTask: monitor thread exit.");
                break;
            }
            curServiceTimes = 0;
        } else {
            curServiceTimes++;
        }
        usleep(UNIT_TIME);
    }
}

void MonitorCmdTask()
{
    int curCmdTimes = 0;
    const int maxCmdTimes = 5 * 60 * 1000 * 1000 / UNIT_TIME; // 5min exit
    HiLog::Info(LABEL, "MonitorCmdTask: monitor thread start.");
    while (true) {
        if (g_traceMode != TraceMode::CMD_MODE) {
            HiLog::Info(LABEL, "MonitorCmdTask: monitor thread exit.");
            return;
        }

        if (curCmdTimes >= maxCmdTimes) {
            HiLog::Error(LABEL, "MonitorCmdTask: CMD_MODE Timeout exit.");
            g_dumpFlag = false;
            while (!g_dumpEnd) {
                usleep(UNIT_TIME);
            }
            OHOS::HiviewDFX::Hitrace::CloseTrace();
            break;
        } else {
            curCmdTimes++;
        }
        usleep(UNIT_TIME);
    }
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
    serviceTraceParams.clockType = "boot";
    serviceTraceParams.isOverWrite = "1";
    return HandleTraceOpen(serviceTraceParams, allTags, tagGroupTable);
}

/**
 * args:  tags:tag1,tags2... tagGroups:group1,group2... clockType:boot bufferSize:1024 overwrite:1 output:filename
 * cmdTraceParams:  Save the above parameters
*/
bool ParseArgs(const std::string &args, TraceParams &cmdTraceParams, const std::map<std::string, TagCategory> &allTags,
               const std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    std::vector<std::string> argList = Split(args, ' ');
    for (std::string item : argList) {
        size_t pos = item.find(":");
        if (pos == std::string::npos) {
            HiLog::Error(LABEL, "ParseArgs: failed.");
            return false;
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
        } else {
            HiLog::Error(LABEL, "ParseArgs: failed.");
            return false;
        }
    }
    if (CheckTags(cmdTraceParams.tags, allTags) && CheckTagGroup(cmdTraceParams.tagGroups, tagGroupTable)) {
        return true;
    }
    return false;
}

} // namespace

namespace OHOS {
namespace HiviewDFX {

namespace Hitrace {

TraceMode GetTraceMode()
{
    return g_traceMode;
}

TraceErrorCode OpenTrace(const std::vector<std::string> &tagGroups)
{
    if (!IsTraceMounted()) {
        HiLog::Error(LABEL, "OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
    }
    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable)) {
        HiLog::Error(LABEL, "OpenTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }

    if (tagGroups.size() == 0 && !CheckTagGroup(tagGroups, tagGroupTable)) {
        HiLog::Error(LABEL, "OpenTrace: TAG_ERROR.");
        return TAG_ERROR;
    }

    if (g_traceMode == CMD_MODE) {
        HiLog::Error(LABEL, "OpenTrace: TRACE_IS_OCCUPIED.");
        return TRACE_IS_OCCUPIED;
    }

    if (g_traceMode == SERVICE_MODE) {
        HiLog::Error(LABEL, "OpenTrace: CALL_ERROR.");
        return CALL_ERROR;
    }

    TraceErrorCode ret = HandleServiceTraceOpen(tagGroups, allTags, tagGroupTable);
    if (ret != SUCCESS) {
        HiLog::Error(LABEL, "OpenTrace: open fail.");
        return ret;
    }
    g_traceMode = SERVICE_MODE;
    // open SERVICE_MODE monitor thread
    std::thread auxiliaryTask(MonitorServiceTask);
    auxiliaryTask.detach();
    HiLog::Info(LABEL, "OpenTrace: SERVICE_MODE open success.");
    return ret;
}

TraceErrorCode OpenTrace(const std::string &args)
{
    if (!IsTraceMounted()) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: TRACE_NOT_SUPPORTED.");
        return TRACE_NOT_SUPPORTED;
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
    
    if (g_traceMode != CLOSE) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: CALL_ERROR.");
        return CALL_ERROR;
    }

    TraceErrorCode ret = HandleTraceOpen(cmdTraceParams, allTags, tagGroupTable);
    if (ret != SUCCESS) {
        HiLog::Error(LABEL, "Hitrace OpenTrace: CMD_MODE open failed.");
        return FILE_ERROR;
    }
    g_traceMode = CMD_MODE;
    // open SERVICE_MODE monitor thread
    std::thread auxiliaryTask(MonitorCmdTask);
    auxiliaryTask.detach();
    HiLog::Info(LABEL, "Hitrace OpenTrace: CMD_MODE open success.");
    return ret;
}

TraceRetInfo DumpTrace()
{
    TraceRetInfo ret;
    if (g_traceMode != SERVICE_MODE) {
        HiLog::Error(LABEL, "DumpTrace: CALL_ERROR.");
        ret.errorCode = CALL_ERROR;
        return ret;
    }
    ret.errorCode = DumpTraceInner(ret.outputFiles);
    return ret;
}

TraceErrorCode DumpTraceOn()
{
    // check current trace status
    if (g_traceMode != CMD_MODE) {
        HiLog::Error(LABEL, "DumpTraceOn: CALL_ERROR.");
        return CALL_ERROR;
    }

    // start task thread
    std::thread task(ProcessDumpTask);
    task.detach();
    HiLog::Info(LABEL, "DumpTraceOn: Dumping trace.");
    return SUCCESS;
}

TraceRetInfo DumpTraceOff()
{
    TraceRetInfo ret;
    g_dumpFlag = false;
    const int waitTime = 10000;
    while (!g_dumpEnd) {
        usleep(waitTime);
    }
    ret.errorCode = SUCCESS;
    ret.outputFiles = g_outputFilesForCmd;
    HiLog::Info(LABEL, "DumpTraceOff: trace files generated success.");
    return ret;
}

TraceErrorCode CloseTrace()
{
    if (g_traceMode == CLOSE) {
        HiLog::Error(LABEL, "CloseTrace: CALL_ERROR.");
        return CALL_ERROR;
    }
    std::map<std::string, TagCategory> allTags;
    std::map<std::string, std::vector<std::string>> tagGroupTable;
    if (!ParseTagInfo(allTags, tagGroupTable) || allTags.size() == 0 || tagGroupTable.size() == 0) {
        HiLog::Error(LABEL, "CloseTrace: ParseTagInfo TAG_ERROR.");
        return TAG_ERROR;
    }
    TraceInit(allTags);
    TruncateFile();
    g_traceMode = CLOSE;
    usleep(UNIT_TIME);
    return SUCCESS;
}

} // Hitrace

} // HiviewDFX
} // OHOS