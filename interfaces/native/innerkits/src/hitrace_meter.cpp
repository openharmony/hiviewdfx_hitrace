/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include <asm/unistd.h>
#include <atomic>
#include <cinttypes>
#include <climits>
#include <ctime>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <linux/perf_event.h>
#include <mutex>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "securec.h"
#include "hilog/log.h"
#include "param/sys_param.h"
#include "parameter.h"
#include "parameters.h"
#include "hitrace_meter.h"
#include "hitrace/tracechain.h"

using namespace std;
using namespace OHOS::HiviewDFX;

#define EXPECTANTLY(exp) (__builtin_expect(!!(exp), true))
#define UNEXPECTANTLY(exp) (__builtin_expect(!!(exp), false))
#define UNUSED_PARAM __attribute__((__unused__))

namespace {
int g_markerFd = -1;
int g_appFd = -1;
std::once_flag g_onceFlag;
std::once_flag g_onceWriteMarkerFailedFlag;
CachedHandle g_cachedHandle;
CachedHandle g_appPidCachedHandle;

std::atomic<bool> g_isHitraceMeterDisabled(false);
std::atomic<bool> g_isHitraceMeterInit(false);
std::atomic<bool> g_needReloadPid(false);
std::atomic<bool> g_pidHasReload(false);

std::atomic<uint64_t> g_tagsProperty(HITRACE_TAG_NOT_READY);
std::atomic<uint64_t> g_appTag(HITRACE_TAG_NOT_READY);
std::atomic<int64_t> g_appTagMatchPid(-1);

const std::string KEY_TRACE_TAG = "debug.hitrace.tags.enableflags";
const std::string KEY_APP_NUMBER = "debug.hitrace.app_number";
const std::string KEY_APP_PID = "debug.hitrace.app_pid";
const std::string KEY_RO_DEBUGGABLE = "ro.debuggable";
const std::string KEY_PREFIX = "debug.hitrace.app_";
const std::string SANDBOX_PATH = "/data/storage/el2/log/";
const std::string PHYSICAL_PATH = "/data/app/el2/100/log/";

constexpr int VAR_NAME_MAX_SIZE = 400;
constexpr int NAME_NORMAL_LEN = 512;
constexpr int BUFFER_LEN = 640;
constexpr int HITRACEID_LEN = 64;

static const int PID_BUF_SIZE = 6;
static char g_pid[PID_BUF_SIZE];
static const std::string EMPTY_TRACE_NAME;
static char g_appName[NAME_NORMAL_LEN + 1] = {0};
static std::string g_appTracePrefix = "";
constexpr const int COMM_STR_MAX = 14;
constexpr const int PID_STR_MAX = 7;
constexpr const int PREFIX_MAX_SIZE = 128; // comm-pid (tgid) [cpu] .... ts.tns: tracing_mark_write:
constexpr const int TRACE_TXT_HEADER_MAX = 1024;
constexpr const int CPU_CORE_NUM = 16;
constexpr const int DEFAULT_CACHE_SIZE = 32 * 1024;
constexpr const int MAX_FILE_SIZE = 500 * 1024 * 1024;
constexpr const int NS_TO_MS = 1000;
int g_tgid = -1;
uint64_t g_traceEventNum = 0;
int g_writeOffset = 0;
int g_fileSize = 0;
TraceFlag g_appFlag(FLAG_MAIN_THREAD);
std::atomic<uint64_t> g_fileLimitSize(0);
std::unique_ptr<char[]> g_traceBuffer;
std::recursive_mutex g_appTraceMutex;

static char g_markTypes[5] = {'B', 'E', 'S', 'F', 'C'};
enum MarkerType { MARKER_BEGIN, MARKER_END, MARKER_ASYNC_BEGIN, MARKER_ASYNC_END, MARKER_INT, MARKER_MAX };

const uint64_t VALID_TAGS = HITRACE_TAG_FFRT | HITRACE_TAG_COMMONLIBRARY | HITRACE_TAG_HDF | HITRACE_TAG_NET
    | HITRACE_TAG_NWEB | HITRACE_TAG_DISTRIBUTED_AUDIO | HITRACE_TAG_FILEMANAGEMENT | HITRACE_TAG_OHOS
    | HITRACE_TAG_ABILITY_MANAGER | HITRACE_TAG_ZCAMERA | HITRACE_TAG_ZMEDIA | HITRACE_TAG_ZIMAGE | HITRACE_TAG_ZAUDIO
    | HITRACE_TAG_DISTRIBUTEDDATA | HITRACE_TAG_GRAPHIC_AGP | HITRACE_TAG_ACE | HITRACE_TAG_NOTIFICATION
    | HITRACE_TAG_MISC | HITRACE_TAG_MULTIMODALINPUT | HITRACE_TAG_RPC | HITRACE_TAG_ARK | HITRACE_TAG_WINDOW_MANAGER
    | HITRACE_TAG_DISTRIBUTED_SCREEN | HITRACE_TAG_DISTRIBUTED_CAMERA | HITRACE_TAG_DISTRIBUTED_HARDWARE_FWK
    | HITRACE_TAG_GLOBAL_RESMGR | HITRACE_TAG_DEVICE_MANAGER | HITRACE_TAG_SAMGR | HITRACE_TAG_POWER
    | HITRACE_TAG_DISTRIBUTED_SCHEDULE | HITRACE_TAG_DISTRIBUTED_INPUT | HITRACE_TAG_BLUETOOTH | HITRACE_TAG_APP;

const std::string TRACE_TXT_HEADER_FORMAT = R"(# tracer: nop
#
# entries-in-buffer/entries-written: %-21s   #P:%-3s
#
#                                          _-----=> irqs-off
#                                         / _----=> need-resched
#                                        | / _---=> hardirq/softirq
#                                        || / _--=> preempt-depth
#                                        ||| /     delay
#           TASK-PID       TGID    CPU#  ||||   TIMESTAMP  FUNCTION
#              | |           |       |   ||||      |         |
)";

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "HitraceMeter"

inline void CreateCacheHandle()
{
    const char* devValue = "true";
    g_cachedHandle = CachedParameterCreate(KEY_TRACE_TAG.c_str(), devValue);
    g_appPidCachedHandle = CachedParameterCreate(KEY_APP_PID.c_str(), devValue);
}

inline void UpdateSysParamTags()
{
    // Get the system parameters of KEY_TRACE_TAG.
    int changed = 0;
    if (UNEXPECTANTLY(g_cachedHandle == nullptr || g_appPidCachedHandle == nullptr)) {
        CreateCacheHandle();
        return;
    }
    const char *paramValue = CachedParameterGetChanged(g_cachedHandle, &changed);
    if (UNEXPECTANTLY(changed == 1) && paramValue != nullptr) {
        uint64_t tags = 0;
        tags = strtoull(paramValue, nullptr, 0);
        g_tagsProperty = (tags | HITRACE_TAG_ALWAYS) & HITRACE_TAG_VALID_MASK;
    }
    int appPidChanged = 0;
    const char *paramPid = CachedParameterGetChanged(g_appPidCachedHandle, &changed);
    if (appPidChanged == 1 && paramPid != nullptr) {
        g_appTagMatchPid = strtoll(paramPid, nullptr, 0);
    }
}

bool IsAppspawnProcess()
{
    string procName;
    ifstream cmdline("/proc/self/cmdline");
    if (cmdline.is_open()) {
        getline(cmdline, procName, '\0');
        cmdline.close();
    }
    return procName == "appspawn";
}

void InitPid()
{
    std::string pidStr = std::to_string(getprocpid());
    int ret = strcpy_s(g_pid, PID_BUF_SIZE, pidStr.c_str());
    if (ret != 0) {
        HILOG_ERROR(LOG_CORE, "pid[%{public}s] strcpy_s fail ret: %{public}d.", pidStr.c_str(), ret);
        return;
    }

    if (!g_needReloadPid && IsAppspawnProcess()) {
        // appspawn restarted, all app need init pid again.
        g_needReloadPid = true;
    }

    HILOG_ERROR(LOG_CORE, "pid[%{public}s] first get g_tagsProperty: %{public}s", pidStr.c_str(),
        to_string(g_tagsProperty.load()).c_str());
}

void ReloadPid()
{
    std::string pidStr = std::to_string(getprocpid());
    int ret = strcpy_s(g_pid, PID_BUF_SIZE, pidStr.c_str());
    if (ret != 0) {
        HILOG_ERROR(LOG_CORE, "pid[%{public}s] strcpy_s fail ret: %{public}d.", pidStr.c_str(), ret);
        return;
    }
    if (!IsAppspawnProcess()) {
        // appspawn restarted, all app need reload pid again.
        g_pidHasReload = true;
    }
}

// open file "trace_marker".
void OpenTraceMarkerFile()
{
    const std::string debugFile = "/sys/kernel/debug/tracing/trace_marker";
    const std::string traceFile = "/sys/kernel/tracing/trace_marker";
    g_markerFd = open(debugFile.c_str(), O_WRONLY | O_CLOEXEC);
#ifdef HITRACE_UNITTEST
    SetMarkerFd(g_markerFd);
#endif
    if (g_markerFd == -1) {
        HILOG_ERROR(LOG_CORE, "open trace file %{public}s failed: %{public}d", debugFile.c_str(), errno);
        g_markerFd = open(traceFile.c_str(), O_WRONLY | O_CLOEXEC);
        if (g_markerFd == -1) {
            HILOG_ERROR(LOG_CORE, "open trace file %{public}s failed: %{public}d", traceFile.c_str(), errno);
            g_tagsProperty = 0;
            return;
        }
    }
    // get tags and pid
    g_tagsProperty = OHOS::system::GetUintParameter<uint64_t>(KEY_TRACE_TAG, 0);
    CreateCacheHandle();
    InitPid();

    g_isHitraceMeterInit = true;
}

void WriteFailedLog()
{
    HILOG_ERROR(LOG_CORE, "write trace_marker failed, %{public}d", errno);
}

void WriteToTraceMarker(const char* buf, const int count)
{
    if (UNEXPECTANTLY(count <= 0)) {
        return;
    }
    if (UNEXPECTANTLY(count >= BUFFER_LEN)) {
        return;
    }
    if (write(g_markerFd, buf, count) < 0) {
        std::call_once(g_onceWriteMarkerFailedFlag, WriteFailedLog);
    }
}

void AddTraceMarkerLarge(const std::string& name, MarkerType type, const int64_t value)
{
    std::string record;
    record += g_markTypes[type];
    record += "|";
    record += g_pid;
    record += "|H:";
    HiTraceId hiTraceId = HiTraceChain::GetId();
    if (hiTraceId.IsValid()) {
        char buf[HITRACEID_LEN] = {0};
        int bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "[%llx,%llx,%llx]#",
            hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId());
        if (EXPECTANTLY(bytes > 0)) {
            record += buf;
        }
    }
    record += name;
    record += " ";
    if (value != 0) {
        record += std::to_string(value);
    }
    WriteToTraceMarker(record.c_str(), record.size());
}

void WriteOnceLog(LogLevel loglevel, const std::string& logStr, bool& isWrite)
{
    if (!isWrite) {
        switch (loglevel) {
            case LOG_ERROR: {
                HILOG_ERROR(LOG_CORE, "%{public}s: %{public}d(%{public}s)", logStr.c_str(), errno, strerror(errno));
                break;
            }
            case LOG_INFO: {
                HILOG_INFO(LOG_CORE, "%{public}s", logStr.c_str());
                break;
            }
            default: {
                break;
            }
        }
        isWrite = true;
    }
}

bool GetProcData(const char* file, char* buffer, const size_t bufferSize)
{
    FILE* fp = fopen(file, "r");
    if (fp == nullptr) {
        static bool isWriteLog = false;
        std::string errLogStr = std::string(__func__) + ": open " + std::string(file) + " falied";
        WriteOnceLog(LOG_ERROR, errLogStr, isWriteLog);
        return false;
    }

    if (fgets(buffer, bufferSize, fp) == nullptr) {
        (void)fclose(fp);
        static bool isWriteLog = false;
        std::string errLogStr = std::string(__func__) + ": fgets " + std::string(file) + " falied";
        WriteOnceLog(LOG_ERROR, errLogStr, isWriteLog);
        return false;
    }

    if (fclose(fp) != 0) {
        static bool isWriteLog = false;
        std::string errLogStr = std::string(__func__) + ": fclose " + std::string(file) + " falied";
        WriteOnceLog(LOG_ERROR, errLogStr, isWriteLog);
        return false;
    }

    return true;
}

int CheckAppTraceArgs(TraceFlag flag, uint64_t tags, uint64_t limitSize)
{
    if (flag != FLAG_MAIN_THREAD && flag != FLAG_ALL_THREAD) {
        HILOG_ERROR(LOG_CORE, "flag(%{public}" PRId32 ") is invalid", flag);
        return RET_FAIL_INVALID_ARGS;
    }

    if (static_cast<int64_t>(tags) < 0 || !UNEXPECTANTLY(tags & VALID_TAGS)) {
        HILOG_ERROR(LOG_CORE, "tags(%{public}" PRId64 ") is invalid", tags);
        return RET_FAIL_INVALID_ARGS;
    }

    if (static_cast<int64_t>(limitSize) <= 0) {
        HILOG_ERROR(LOG_CORE, "limitSize(%{public}" PRId64 ") is invalid", limitSize);
        return RET_FAIL_INVALID_ARGS;
    }

    return RET_SUCC;
}

int SetAppFileName(std::string& destFileName, std::string& fileName)
{
    destFileName = SANDBOX_PATH + "trace/";
    mode_t permissions = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; // 0755
#ifdef HITRACE_UNITTEST
    destFileName = "/data/local/tmp/";
#endif
    if (access(destFileName.c_str(), F_OK) != 0 && mkdir(destFileName.c_str(), permissions) == -1) {
        HILOG_ERROR(LOG_CORE, "failed to create dir(%{public}s):%{public}d(%{public}s)", destFileName.c_str(),
            errno, strerror(errno));
        return RET_FAIL_MKDIR;
    }

    if (!GetProcData("/proc/self/cmdline", g_appName, NAME_NORMAL_LEN)) {
        HILOG_ERROR(LOG_CORE, "get app name failed, %{public}d", errno);
        return RET_FAILD;
    }

    time_t now = time(nullptr);
    if (now == static_cast<time_t>(-1)) {
        HILOG_ERROR(LOG_CORE, "get time failed, %{public}d", errno);
        return RET_FAILD;
    }

    struct tm tmTime;
    if (localtime_r(&now, &tmTime) == nullptr) {
        HILOG_ERROR(LOG_CORE, "localtime_r failed, %{public}d", errno);
        return RET_FAILD;
    }

    const int yearCount = 1900;
    const int size = sizeof(g_appName) + 32;
    char file[size] = {0};
    auto ret = snprintf_s(file, sizeof(file), sizeof(file) - 1, "%s_%04d%02d%02d_%02d%02d%02d.trace", g_appName,
        tmTime.tm_year + yearCount, tmTime.tm_mon + 1, tmTime.tm_mday, tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec);
    if (ret <= 0) {
        HILOG_ERROR(LOG_CORE, "Format file failed, %{public}d", errno);
        return RET_FAILD;
    }

    destFileName += std::string(file);
    fileName = PHYSICAL_PATH + std::string(g_appName) + "/trace/" + std::string(file);
    return RET_SUCC;
}

int InitTraceHead()
{
    // write reserved trace header
    std::vector<char> buffer(TRACE_TXT_HEADER_MAX, '\0');
    int used = snprintf_s(buffer.data(), buffer.size(), buffer.size() - 1, TRACE_TXT_HEADER_FORMAT.c_str(), "", "");
    if (used <= 0) {
        HILOG_ERROR(LOG_CORE, "format reserved trace header failed: %{public}d(%{public}s)", errno, strerror(errno));
        return RET_FAILD;
    }
    if (write(g_appFd, buffer.data(), used) != used) {
        HILOG_ERROR(LOG_CORE, "write reserved trace header failed: %{public}d(%{public}s)", errno, strerror(errno));
        return RET_FAILD;
    }

    g_writeOffset = 0;
    g_fileSize = used;
    g_traceEventNum = 0;

    lseek(g_appFd, used, SEEK_SET); // Reserve space to write the file header.
    return RET_SUCC;
}

bool WriteTraceToFile(char* buf, const int len)
{
    if (write(g_appFd, buf, len) != len) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_ERROR, "write app trace data failed", isWriteLog);
        return false;
    }

    g_fileSize += len;
    g_writeOffset = 0;
    return true;
}

char* GetTraceBuffer(int size)
{
    if (g_writeOffset + size > DEFAULT_CACHE_SIZE && g_writeOffset + size < MAX_FILE_SIZE) {
        // The remaining space is insufficient to cache the data. Write the data to the file.
        if (!WriteTraceToFile(g_traceBuffer.get(), g_writeOffset)) {
            return nullptr;
        }
    }

    return g_traceBuffer.get() + g_writeOffset;
}

void SetCommStr()
{
    int size = g_appTracePrefix.size();
    if (size >= COMM_STR_MAX) {
        g_appTracePrefix = g_appTracePrefix.substr(size - COMM_STR_MAX, size);
    } else {
        g_appTracePrefix = std::string(COMM_STR_MAX - size, ' ') + g_appTracePrefix;
    }
}

void SetMainThreadInfo()
{
    if (strlen(g_appName) == 0) {
        if (!GetProcData("/proc/self/cmdline", g_appName, NAME_NORMAL_LEN)) {
            HILOG_ERROR(LOG_CORE, "get app name failed, %{public}d", errno);
        }
    }

    g_appTracePrefix = std::string(g_appName);
    SetCommStr();
    std::string pidStr = std::string(g_pid);
    std::string pidFixStr = std::string(PID_STR_MAX - pidStr.length(), ' ');
    g_appTracePrefix +=  "-" + pidStr + pidFixStr + " (" + pidFixStr + pidStr + ")";
}

bool SetAllThreadInfo(const int& tid)
{
    std::string tidStr = std::to_string(tid);
    std::string file = "/proc/self/task/" + tidStr + "/comm";
    char comm[NAME_NORMAL_LEN + 1] = {0};
    if (!GetProcData(file.c_str(), comm, NAME_NORMAL_LEN)) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_ERROR, "get comm failed", isWriteLog);
        return false;
    }
    if (comm[strlen(comm) - 1] == '\n') {
        comm[strlen(comm) - 1] = '\0';
    }
    g_appTracePrefix = std::string(comm);
    SetCommStr();

    std::string pidStr = std::string(g_pid);
    std::string tidFixStr = std::string(PID_STR_MAX - tidStr.length(), ' ');
    std::string pidFixStr = std::string(PID_STR_MAX - pidStr.length(), ' ');
    g_appTracePrefix += "-" + tidStr + tidFixStr + " (" + pidFixStr + pidStr + ")";

    return true;
}

int SetAppTraceBuffer(char* buf, const int len, MarkerType type, const std::string& name, const int64_t value)
{
    struct timespec ts = { 0, 0 };
    clock_gettime(CLOCK_BOOTTIME, &ts);
    int cpu = sched_getcpu();
    if (cpu == -1) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_ERROR, "get cpu failed", isWriteLog);
        return -1;
    }

    int bytes = 0;
    if (type == MARKER_BEGIN) {
        bytes = snprintf_s(buf, len, len - 1, "  %s [%03d] .... %lu.%06lu: tracing_mark_write: B|%s|H:%s \n",
            g_appTracePrefix.c_str(), cpu, static_cast<long>(ts.tv_sec),
            static_cast<long>(ts.tv_nsec / NS_TO_MS), g_pid, name.c_str());
    } else if (type == MARKER_END) {
        bytes = snprintf_s(buf, len, len - 1, "  %s [%03d] .... %lu.%06lu: tracing_mark_write: E|%s|\n",
            g_appTracePrefix.c_str(), cpu, static_cast<long>(ts.tv_sec),
            static_cast<long>(ts.tv_nsec / NS_TO_MS), g_pid);
    } else {
        char marktypestr = g_markTypes[type];
        bytes = snprintf_s(buf, len, len - 1, "  %s [%03d] .... %lu.%06lu: tracing_mark_write: %c|%s|H:%s %lld\n",
            g_appTracePrefix.c_str(), cpu, static_cast<long>(ts.tv_sec),
            static_cast<long>(ts.tv_nsec / NS_TO_MS), marktypestr, g_pid, name.c_str(), value);
    }

    return bytes;
}

void SetAppTrace(const int len, MarkerType type, const std::string& name, const int64_t value)
{
    // get buffer
    char* buf = GetTraceBuffer(len);
    if (buf == nullptr) {
        return;
    }

    int bytes = SetAppTraceBuffer(buf, len, type, name, value);
    if (bytes > 0) {
        g_traceEventNum += 1;
        g_writeOffset += bytes;
    }
}

void WriteAppTraceLong(const int len, MarkerType type, const std::string& name, const int64_t value)
{
    // 1 write cache data.
    if (!WriteTraceToFile(g_traceBuffer.get(), g_writeOffset)) {
        return;
    }

    // 2 apply new memory and directly write file.
    auto buffer = std::make_unique<char[]>(len);
    if (buffer == nullptr) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_ERROR, "memory allocation failed", isWriteLog);
        return;
    }

    int bytes = SetAppTraceBuffer(buffer.get(), len, type, name, value);
    if (bytes > 0) {
        if (!WriteTraceToFile(buffer.get(), bytes)) {
            return;
        }
        g_traceEventNum += 1;
    }
}

bool CheckFileSize(int len)
{
    if (static_cast<uint64_t>(g_fileSize + g_writeOffset + len) > g_fileLimitSize.load()) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_INFO, "File size limit exceeded, stop capture trace.", isWriteLog);
        StopCaptureAppTrace();
        return false;
    }

    return true;
}

void WriteAppTrace(MarkerType type, const std::string& name, const int64_t value)
{
    int tid = getproctid();
    int len = PREFIX_MAX_SIZE + name.length();
    if (g_appFlag == FLAG_MAIN_THREAD && g_tgid == tid) {
        if (!CheckFileSize(len)) {
            return;
        }

        if (g_appTracePrefix.empty()) {
            SetMainThreadInfo();
        }

        if (len <= DEFAULT_CACHE_SIZE) {
            SetAppTrace(len, type, name, value);
        } else {
            WriteAppTraceLong(len, type, name, value);
        }
    } else if (g_appFlag == FLAG_ALL_THREAD) {
        std::unique_lock<std::recursive_mutex> lock(g_appTraceMutex);
        if (!CheckFileSize(len)) {
            return;
        }

        SetAllThreadInfo(tid);

        if (len <= DEFAULT_CACHE_SIZE) {
            SetAppTrace(len, type, name, value);
        } else {
            WriteAppTraceLong(len, type, name, value);
        }
    }
}

void AddHitraceMeterMarker(MarkerType type, uint64_t tag, const std::string& name, const int64_t value,
    const HiTraceIdStruct* hiTraceIdStruct = nullptr)
{
    if (UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    if (UNEXPECTANTLY(g_needReloadPid && !g_pidHasReload)) {
        ReloadPid();
    }
    if (UNEXPECTANTLY(!g_isHitraceMeterInit)) {
        struct timespec ts = { 0, 0 };
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1 || ts.tv_sec < 25) { // 25 : register after boot 25s
            return;
        }
        std::call_once(g_onceFlag, OpenTraceMarkerFile);
    }
    UpdateSysParamTags();
    if (UNEXPECTANTLY(g_tagsProperty & tag) && g_markerFd != -1) {
        if (tag == HITRACE_TAG_APP && g_appTagMatchPid > 0 && g_appTagMatchPid != std::stoi(g_pid)) {
            return;
        }
        // record fomart: "type|pid|name value".
        char buf[BUFFER_LEN] = {0};
        int len = name.length();
        if (UNEXPECTANTLY(len <= NAME_NORMAL_LEN)) {
            HiTraceId hiTraceId = (hiTraceIdStruct == nullptr) ? HiTraceChain::GetId() : HiTraceId(*hiTraceIdStruct);
            bool isHiTraceIdValid = hiTraceId.IsValid();
            int bytes = 0;
            if (type == MARKER_BEGIN) {
                bytes = isHiTraceIdValid ? snprintf_s(buf, sizeof(buf), sizeof(buf) - 1,
                    "B|%s|H:[%llx,%llx,%llx]#%s ", g_pid, hiTraceId.GetChainId(),
                    hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(), name.c_str())
                    : snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "B|%s|H:%s ", g_pid, name.c_str());
            } else if (type == MARKER_END) {
                bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "E|%s|", g_pid);
            } else {
                char marktypestr = g_markTypes[type];
                bytes = isHiTraceIdValid ? snprintf_s(buf, sizeof(buf), sizeof(buf) - 1,
                    "%c|%s|H:[%llx,%llx,%llx]#%s %lld", marktypestr, g_pid,
                    hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(), name.c_str(), value)
                    : snprintf_s(buf, sizeof(buf), sizeof(buf) - 1,
                    "%c|%s|H:%s %lld", marktypestr, g_pid, name.c_str(), value);
            }
            WriteToTraceMarker(buf, bytes);
        } else {
            AddTraceMarkerLarge(name, type, value);
        }
    }
    auto g_appTagload = g_appTag.load();
#ifdef HITRACE_UNITTEST
    g_appTagload = HITRACE_TAG_APP;
#endif
    if (UNEXPECTANTLY(g_appTagload != HITRACE_TAG_NOT_READY) && g_appFd != -1) {
        WriteAppTrace(type, name, value);
    }
}
}; // namespace

#ifdef HITRACE_UNITTEST
void SetReloadPid(bool isReloadPid)
{
    g_needReloadPid = isReloadPid;
}

void SetpidHasReload(bool ispidHasReload)
{
    g_pidHasReload = ispidHasReload;
}

void SetAppFd(int appFd)
{
    g_appFd = appFd;
}

void GetSetMainThreadInfo()
{
    SetMainThreadInfo();
}

void GetSetCommStr()
{
    SetCommStr();
}

void SetTraceBuffer(int size)
{
    GetTraceBuffer(size);
}

void SetFileLimitSize(uint64_t fileLimitSize)
{
    g_fileLimitSize = fileLimitSize;
}

void SetAddTraceMarkerLarge(const std::string& name, const int64_t value)
{
    AddTraceMarkerLarge(name, MARKER_BEGIN, value);
}

void SetAddHitraceMeterMarker(uint64_t label, const string& value)
{
    AddHitraceMeterMarker(MARKER_BEGIN, label, value, 0);
}

void SetWriteToTraceMarker(const char* buf, const int count)
{
    WriteToTraceMarker(buf, count);
}

void SetGetProcData(const char* file)
{
    GetProcData(file, g_appName, NAME_NORMAL_LEN);
}

void HitracePerfScoped::SetHitracePerfScoped(int fd1st, int fd2nd)
{
    if (fd1st == -1 && fd2nd == -1) {
        fd1st_ = fd1st;
        fd2nd_ = fd2nd;
    }
    GetInsCount();
    GetCycleCount();
}

void SetCachedHandleAndAppPidCachedHandle(CachedHandle cachedHandle, CachedHandle appPidCachedHandle)
{
    g_cachedHandle = cachedHandle;
    g_appPidCachedHandle = appPidCachedHandle;
    UpdateSysParamTags();
}

void SetMarkerFd(int markerFd)
{
    if (markerFd != -1) {
        g_markerFd = -1;
    }
}

void SetWriteAppTrace(TraceFlag appFlag, const std::string& name, const int64_t value, bool tid)
{
    if (tid) {
        g_tgid = getproctid();
    }
    g_appFlag = appFlag;
    WriteAppTrace(MARKER_BEGIN, name, value);
}
#endif
void UpdateTraceLabel(void)
{
    if (!g_isHitraceMeterInit) {
        return;
    }
    UpdateSysParamTags();
}

void SetTraceDisabled(bool disable)
{
    g_isHitraceMeterDisabled = disable;
}

void StartTraceWrapper(uint64_t label, const char *value)
{
    std::string traceValue = value;
    StartTrace(label, traceValue);
}

void StartTrace(uint64_t label, const string& value, float limit UNUSED_PARAM)
{
    AddHitraceMeterMarker(MARKER_BEGIN, label, value, 0);
}

void StartTraceDebug(bool isDebug, uint64_t label, const string& value, float limit UNUSED_PARAM)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, value, 0);
}

void StartTraceArgs(uint64_t label, const char *fmt, ...)
{
    UpdateSysParamTags();
    if (!(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, name, 0);
}

void StartTraceArgsDebug(bool isDebug, uint64_t label, const char *fmt, ...)
{
    UpdateSysParamTags();
    if (!isDebug || !(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, name, 0);
}

void FinishTrace(uint64_t label)
{
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
}

void FinishTraceDebug(bool isDebug, uint64_t label)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
}

void StartAsyncTrace(uint64_t label, const string& value, int32_t taskId, float limit UNUSED_PARAM)
{
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, value, taskId);
}

void StartAsyncTraceWrapper(uint64_t label, const char *value, int32_t taskId)
{
    std::string traceValue = value;
    StartAsyncTrace(label, traceValue, taskId);
}

void StartTraceChain(uint64_t label, const struct HiTraceIdStruct* hiTraceId, const char *value)
{
    AddHitraceMeterMarker(MARKER_BEGIN, label, value, 0, hiTraceId);
}

void StartAsyncTraceDebug(bool isDebug, uint64_t label, const string& value, int32_t taskId, float limit UNUSED_PARAM)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, value, taskId);
}

void StartAsyncTraceArgs(uint64_t label, int32_t taskId, const char *fmt, ...)
{
    UpdateSysParamTags();
    if (!(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, name, taskId);
}

void StartAsyncTraceArgsDebug(bool isDebug, uint64_t label, int32_t taskId, const char *fmt, ...)
{
    UpdateSysParamTags();
    if (!isDebug || !(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, name, taskId);
}

void FinishAsyncTrace(uint64_t label, const string& value, int32_t taskId)
{
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, value, taskId);
}

void FinishAsyncTraceWrapper(uint64_t label, const char *value, int32_t taskId)
{
    std::string traceValue = value;
    FinishAsyncTrace(label, traceValue, taskId);
}

void FinishAsyncTraceDebug(bool isDebug, uint64_t label, const string& value, int32_t taskId)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, value, taskId);
}

void FinishAsyncTraceArgs(uint64_t label, int32_t taskId, const char *fmt, ...)
{
    UpdateSysParamTags();
    if (!(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, name, taskId);
}

void FinishAsyncTraceArgsDebug(bool isDebug, uint64_t label, int32_t taskId, const char *fmt, ...)
{
    UpdateSysParamTags();
    if (!isDebug || !(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, name, taskId);
}

void MiddleTrace(uint64_t label, const string& beforeValue UNUSED_PARAM, const std::string& afterValue)
{
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
    AddHitraceMeterMarker(MARKER_BEGIN, label, afterValue, 0);
}

void MiddleTraceDebug(bool isDebug, uint64_t label, const string& beforeValue UNUSED_PARAM,
    const std::string& afterValue)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
    AddHitraceMeterMarker(MARKER_BEGIN, label, afterValue, 0);
}

void CountTrace(uint64_t label, const string& name, int64_t count)
{
    AddHitraceMeterMarker(MARKER_INT, label, name, count);
}

void CountTraceDebug(bool isDebug, uint64_t label, const string& name, int64_t count)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_INT, label, name, count);
}

void CountTraceWrapper(uint64_t label, const char *name, int64_t count)
{
    std::string traceName = name;
    CountTrace(label, traceName, count);
}

HitraceMeterFmtScoped::HitraceMeterFmtScoped(uint64_t label, const char *fmt, ...) : mTag(label)
{
    UpdateSysParamTags();
    if (!(label & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_ERROR(LOG_CORE, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, name, 0);
}

bool IsTagEnabled(uint64_t tag)
{
    UpdateSysParamTags();
    return ((tag & g_tagsProperty) == tag);
}

// For native process, the caller is responsible passing the full path of the fileName.
// For hap application, StartCaputreAppTrace() fill fileName
// as /data/app/el2/100/log/$(processname)/trace/$(processname)_$(date)_&(time).trace and return to caller.
int StartCaptureAppTrace(TraceFlag flag, uint64_t tags, uint64_t limitSize, std::string& fileName)
{
    auto ret = CheckAppTraceArgs(flag, tags, limitSize);
    if (ret != RET_SUCC) {
        return ret;
    }

    std::unique_lock<std::recursive_mutex> lock(g_appTraceMutex);
    if (g_appFd != -1) {
        HILOG_INFO(LOG_CORE, "CaptureAppTrace started, return");
        return RET_STARTED;
    }

    g_traceBuffer = std::make_unique<char[]>(DEFAULT_CACHE_SIZE);
    if (g_traceBuffer == nullptr) {
        HILOG_ERROR(LOG_CORE, "memory allocation failed: %{public}d(%{public}s)", errno, strerror(errno));
        return RET_FAILD;
    }

    g_appFlag = flag;
    g_appTag = tags;
    g_fileLimitSize = (limitSize > MAX_FILE_SIZE) ? MAX_FILE_SIZE : limitSize;
    g_tgid = getprocpid();
    g_appTracePrefix = "";

    std::string destFileName = fileName;
    if (destFileName.empty()) {
        auto retval = SetAppFileName(destFileName, fileName);
        if (retval != RET_SUCC) {
            HILOG_ERROR(LOG_CORE, "set appFileName failed: %{public}d(%{public}s)", errno, strerror(errno));
            return retval;
        }
    }

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 0644
    g_appFd = open(destFileName.c_str(), O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC, mode);
    if (g_appFd == -1) {
        HILOG_ERROR(LOG_CORE, "open %{public}s failed: %{public}d(%{public}s)", destFileName.c_str(),
            errno, strerror(errno));
        if (errno == ENOENT) {
            return RET_FAIL_ENOENT;
        } else if (errno == EACCES) {
            return RET_FAIL_EACCES;
        } else {
            return RET_FAILD;
        }
    }

    return InitTraceHead();
}

int StopCaptureAppTrace()
{
    std::unique_lock<std::recursive_mutex> lock(g_appTraceMutex);
    if (g_appFd == -1) {
        HILOG_INFO(LOG_CORE, "CaptureAppTrace stopped, return");
        return RET_STOPPED;
    }

    // Write cache data
    WriteTraceToFile(g_traceBuffer.get(), g_writeOffset);

    std::string eventNumStr = std::to_string(g_traceEventNum) + "/" + std::to_string(g_traceEventNum);
    std::vector<char> buffer(TRACE_TXT_HEADER_MAX, '\0');
    int used = snprintf_s(buffer.data(), buffer.size(), buffer.size() - 1, TRACE_TXT_HEADER_FORMAT.c_str(),
        eventNumStr.c_str(), std::to_string(CPU_CORE_NUM).c_str());
    if (used <= 0) {
        HILOG_ERROR(LOG_CORE, "format trace header failed: %{public}d(%{public}s)", errno, strerror(errno));
        return RET_FAILD;
    }

    lseek(g_appFd, 0, SEEK_SET); // Move the write pointer to populate the file header.
    if (write(g_appFd, buffer.data(), used) != used) {
        HILOG_ERROR(LOG_CORE, "write trace header failed: %{public}d(%{public}s)", errno, strerror(errno));
        return RET_FAILD;
    }

    g_fileSize = 0;
    g_writeOffset = 0;
    g_traceEventNum = 0;
    g_appTracePrefix = "";
    g_appTag = HITRACE_TAG_NOT_READY;

    close(g_appFd);
    g_appFd = -1;
    g_traceBuffer.reset();
    g_traceBuffer = nullptr;

    return RET_SUCC;
}

HitracePerfScoped::HitracePerfScoped(bool isDebug, uint64_t tag, const std::string &name) : mTag_(tag), mName_(name)
{
    if (!isDebug) {
        return;
    }
    struct perf_event_attr peIns;
    (void)memset_s(&peIns, sizeof(struct perf_event_attr), 0, sizeof(struct perf_event_attr));
    peIns.type = PERF_TYPE_HARDWARE;
    peIns.size = sizeof(struct perf_event_attr);
    peIns.config = PERF_COUNT_HW_INSTRUCTIONS;
    peIns.disabled = 1;
    peIns.exclude_kernel = 0;
    peIns.exclude_hv = 0;
    fd1st_ = syscall(__NR_perf_event_open, &peIns, 0, -1, -1, 0);
    if (fd1st_ == -1) {
        err_ = errno;
        return;
    }
    struct perf_event_attr peCycles;
    (void)memset_s(&peCycles, sizeof(struct perf_event_attr), 0, sizeof(struct perf_event_attr));
    peCycles.type = PERF_TYPE_HARDWARE;
    peCycles.size = sizeof(struct perf_event_attr);
    peCycles.config = PERF_COUNT_HW_CPU_CYCLES;
    peCycles.disabled = 1;
    peCycles.exclude_kernel = 0;
    peCycles.exclude_hv = 0;
    fd2nd_ = syscall(__NR_perf_event_open, &peCycles, 0, -1, -1, 0);
    if (fd2nd_ == -1) {
        err_ = errno;
        return;
    }
    ioctl(fd1st_, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd1st_, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd2nd_, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd2nd_, PERF_EVENT_IOC_ENABLE, 0);
}

HitracePerfScoped::~HitracePerfScoped()
{
    if (fd1st_ != -1) {
        ioctl(fd1st_, PERF_EVENT_IOC_DISABLE, 0);
        read(fd1st_, &countIns_, sizeof(long long));
        close(fd1st_);
        CountTrace(mTag_, mName_ + "-Ins", countIns_);
    }
    if (fd2nd_ != -1) {
        ioctl(fd2nd_, PERF_EVENT_IOC_DISABLE, 0);
        read(fd2nd_, &countCycles_, sizeof(long long));
        close(fd2nd_);
        CountTrace(mTag_, mName_ + "-Cycle", countCycles_);
    }
}

inline long long HitracePerfScoped::GetInsCount()
{
    if (fd1st_ == -1) {
        return err_;
    }
    read(fd1st_, &countIns_, sizeof(long long));
    return countIns_;
}

inline long long HitracePerfScoped::GetCycleCount()
{
    if (fd2nd_ == -1) {
        return err_;
    }
    read(fd2nd_, &countCycles_, sizeof(long long));
    return countCycles_;
}
