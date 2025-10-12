/*
 * Copyright (C) 2022-2024 Huawei Device Co., Ltd.
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

#include "hitrace_meter.h"

#include <algorithm>
#include <asm/unistd.h>
#include <atomic>
#include <cinttypes>
#include <climits>
#include <ctime>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <linux/perf_event.h>
#include <memory>
#include <mutex>
#include <queue>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include "common_define.h"
#include "common_utils.h"
#include "securec.h"
#include "hilog/log.h"
#include "param/sys_param.h"
#include "parameters.h"
#include "hitrace/tracechain.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceMeter"
#endif

using namespace OHOS::HiviewDFX;

namespace {
int g_markerFd = -1;
int g_appFd = -1;
std::once_flag g_onceFlag;
std::once_flag g_onceWriteMarkerFailedFlag;
std::atomic<CachedHandle> g_cachedHandle;
std::atomic<CachedHandle> g_appPidCachedHandle;
std::atomic<CachedHandle> g_levelThresholdCachedHandle;

std::atomic<bool> g_isHitraceMeterDisabled(false);
std::atomic<bool> g_isHitraceMeterInit(false);

std::atomic<uint64_t> g_tagsProperty(HITRACE_TAG_NOT_READY);
std::atomic<uint64_t> g_appTag(HITRACE_TAG_NOT_READY);
std::atomic<int64_t> g_appTagMatchPid(-1);
std::atomic<HiTraceOutputLevel> g_levelThreshold(HITRACE_LEVEL_MAX);

static const std::string SANDBOX_PATH = "/data/storage/el2/log/";
static const char* PHYSICAL_PATH = "/data/app/el2/100/log/";
const char* const EMPTY = "";

constexpr int VAR_NAME_MAX_SIZE = 400;
constexpr int NAME_NORMAL_LEN = 512;
constexpr int RECORD_SIZE_MAX = 1024;

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
std::mutex g_tagsChangeMutex;
std::atomic<bool> g_isCallbacksEmpty = true;

constexpr int32_t MAX_CALLBACKS = 10;
constexpr int32_t INVALID_PARAMETER = -2;
constexpr int32_t REACH_MAX_CALLBACKS = -1;
constexpr int32_t INVALID_INDEX = -2;
constexpr int32_t INDEX_NOT_REGISTERED = -1;
constexpr int32_t SUCCESS_UNREGISTER = 0;

static const char g_markTypes[5] = {'B', 'E', 'S', 'F', 'C'};
enum MarkerType { MARKER_BEGIN, MARKER_END, MARKER_ASYNC_BEGIN, MARKER_ASYNC_END, MARKER_INT };

static const char g_traceLevel[4] = {'D', 'I', 'C', 'M'};

const uint64_t VALID_TAGS = HITRACE_TAG_FFRT | HITRACE_TAG_COMMONLIBRARY | HITRACE_TAG_HDF | HITRACE_TAG_NET |
    HITRACE_TAG_NWEB | HITRACE_TAG_DISTRIBUTED_AUDIO | HITRACE_TAG_FILEMANAGEMENT | HITRACE_TAG_OHOS |
    HITRACE_TAG_ABILITY_MANAGER | HITRACE_TAG_ZCAMERA | HITRACE_TAG_ZMEDIA | HITRACE_TAG_ZIMAGE | HITRACE_TAG_ZAUDIO |
    HITRACE_TAG_DISTRIBUTEDDATA | HITRACE_TAG_GRAPHIC_AGP | HITRACE_TAG_ACE | HITRACE_TAG_NOTIFICATION |
    HITRACE_TAG_MISC | HITRACE_TAG_MULTIMODALINPUT | HITRACE_TAG_RPC | HITRACE_TAG_ARK | HITRACE_TAG_WINDOW_MANAGER |
    HITRACE_TAG_DISTRIBUTED_SCREEN | HITRACE_TAG_DISTRIBUTED_CAMERA | HITRACE_TAG_DISTRIBUTED_HARDWARE_FWK |
    HITRACE_TAG_GLOBAL_RESMGR | HITRACE_TAG_DEVICE_MANAGER | HITRACE_TAG_SAMGR | HITRACE_TAG_POWER |
    HITRACE_TAG_DISTRIBUTED_SCHEDULE | HITRACE_TAG_DISTRIBUTED_INPUT | HITRACE_TAG_BLUETOOTH | HITRACE_TAG_APP;

static const char* TRACE_TXT_HEADER_FORMAT = R"(# tracer: nop
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

struct TraceMarker {
    const MarkerType type;
    HiTraceOutputLevel level;
    uint64_t tag;
    const int64_t value;
    const char* name;
    const char* customCategory;
    const char* customArgs;
    const HiTraceIdStruct* hiTraceIdStruct = nullptr;
    int pid = -1;
};

class TaskQueue {
public:
    TaskQueue()
    {
        thread_ = std::thread(&TaskQueue::ProcessTasks, this);
    }

    ~TaskQueue()
    {
        stop_ = true;
        condition_.notify_one();
        thread_.join();
    }

    static TaskQueue& Instance()
    {
        static TaskQueue instance;
        return instance;
    }

    void Enqueue(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(task);
        }
        condition_.notify_one();
    }

private:
    void ProcessRemainingTasks()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            auto task = queue_.front();
            queue_.pop();
            lock.unlock();
            task();
            lock.lock();
        }
    }

    void ProcessTasks()
    {
        while (!stop_) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { return !queue_.empty() || stop_; });
                if (stop_) {
                    HILOG_INFO(LOG_CORE, "ProcessTasks stop");
                    break;
                }
                task = queue_.front();
                queue_.pop();
            }
            task();
        }
        ProcessRemainingTasks();
    }

    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread thread_;
    bool stop_ = false;
};

static void HandleAppTagChange(uint64_t oldTags, uint64_t newTags)
{
    bool oldAppTag = ((oldTags & HITRACE_TAG_APP) != 0);
    bool newAppTag = ((newTags & HITRACE_TAG_APP) != 0);

    if (newAppTag != oldAppTag && !g_isCallbacksEmpty) {
        HILOG_INFO(LOG_CORE, "HandleAppTagChange");
        TaskQueue::Instance().Enqueue([newAppTag] {
            CallbackRegistry::Instance().ExecuteAll(newAppTag);
        });
    }
}

inline void CreateCacheHandle()
{
    const char* devValue = "true";
    if (g_cachedHandle == nullptr) {
        g_cachedHandle = CachedParameterCreate(TRACE_TAG_ENABLE_FLAGS, devValue);
    }
    if (g_appPidCachedHandle == nullptr) {
        g_appPidCachedHandle = CachedParameterCreate(TRACE_KEY_APP_PID, devValue);
    }
    if (g_levelThresholdCachedHandle == nullptr) {
        g_levelThresholdCachedHandle = CachedParameterCreate(TRACE_LEVEL_THRESHOLD, devValue);
    }
}

static void UpdateSysParamTags()
{
    // Get the system parameters of TRACE_TAG_ENABLE_FLAGS.
    if (UNEXPECTANTLY(g_cachedHandle == nullptr || g_appPidCachedHandle == nullptr ||
        g_levelThresholdCachedHandle == nullptr)) {
        CreateCacheHandle();
        return;
    }

    {
        std::unique_lock<std::mutex> lock(g_tagsChangeMutex);
        int changed = 0;
        const char* paramValue = CachedParameterGetChanged(g_cachedHandle, &changed);
        if (UNEXPECTANTLY(changed == 1) && paramValue != nullptr) {
            uint64_t tags = 0;
            if (!OHOS::HiviewDFX::Hitrace::StringToUint64(paramValue, tags)) {
                return;
            }

            uint64_t oldTags = g_tagsProperty.load() | g_appTag.load();
            g_tagsProperty.store((tags | HITRACE_TAG_ALWAYS) & HITRACE_TAG_VALID_MASK);
            uint64_t newTags = g_tagsProperty.load() | g_appTag.load();
            HandleAppTagChange(oldTags, newTags);
        }
    }
    int appPidChanged = 0;
    const char* paramPid = CachedParameterGetChanged(g_appPidCachedHandle, &appPidChanged);
    if (UNEXPECTANTLY(appPidChanged == 1) && paramPid != nullptr) {
        int64_t appTagMatchPid = -1;
        if (!OHOS::HiviewDFX::Hitrace::StringToInt64(paramPid, appTagMatchPid)) {
            return;
        }
        g_appTagMatchPid = appTagMatchPid;
    }
    int levelThresholdChanged = 0;
    const char* paramLevel = CachedParameterGetChanged(g_levelThresholdCachedHandle, &levelThresholdChanged);
    if (UNEXPECTANTLY(levelThresholdChanged == 1) && paramLevel != nullptr) {
        int64_t levelThreshold = 0;
        if (!OHOS::HiviewDFX::Hitrace::StringToInt64(paramLevel, levelThreshold)) {
            return;
        }
        g_levelThreshold = static_cast<HiTraceOutputLevel>(levelThreshold);
    }
}

// open file "trace_marker".
void OpenTraceMarkerFile()
{
    const std::string debugFile = DEBUGFS_TRACING_DIR + TRACE_MARKER_NODE;
    const std::string traceFile = TRACEFS_DIR + TRACE_MARKER_NODE;
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
    // get tags, level threshold and pid
    g_tagsProperty = OHOS::system::GetUintParameter<uint64_t>(TRACE_TAG_ENABLE_FLAGS, 0);
    g_levelThreshold = static_cast<HiTraceOutputLevel>(OHOS::system::GetIntParameter<int>(TRACE_LEVEL_THRESHOLD,
        HITRACE_LEVEL_MAX, HITRACE_LEVEL_DEBUG, HITRACE_LEVEL_COMMERCIAL));
    CreateCacheHandle();

    g_isHitraceMeterInit = true;
}

__attribute__((destructor)) static void LibraryUnload()
{
    if (g_markerFd != -1) {
        close(g_markerFd);
        g_markerFd = -1;
    }
    if (g_appFd != -1) {
        close(g_appFd);
        g_appFd = -1;
    }

    CachedParameterDestroy(g_cachedHandle);
    g_cachedHandle = nullptr;

    CachedParameterDestroy(g_appPidCachedHandle);
    g_appPidCachedHandle = nullptr;

    CachedParameterDestroy(g_levelThresholdCachedHandle);
    g_levelThresholdCachedHandle = nullptr;
}

__attribute__((always_inline)) bool PrepareTraceMarker()
{
    if (UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return false;
    }
    if (UNEXPECTANTLY(!g_isHitraceMeterInit)) {
        struct timespec ts = { 0, 0 };
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1 || ts.tv_sec < 25) { // 25 : register after boot 25s
            return false;
        }
        std::call_once(g_onceFlag, OpenTraceMarkerFile);
    }
    UpdateSysParamTags();
    return true;
}

void WriteFailedLog()
{
    HILOG_ERROR(LOG_CORE, "write trace_marker failed, %{public}d", errno);
}

void WriteToTraceMarker(const char* buf, int bytes)
{
    if (write(g_markerFd, buf, bytes) < 0) {
        std::call_once(g_onceWriteMarkerFailedFlag, WriteFailedLog);
    }
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
        std::string errLogStr = std::string(__func__) + ": open " + std::string(file) + " failed";
        WriteOnceLog(LOG_ERROR, errLogStr, isWriteLog);
        return false;
    }

    if (fgets(buffer, bufferSize, fp) == nullptr) {
        (void)fclose(fp);
        static bool isWriteLog = false;
        std::string errLogStr = std::string(__func__) + ": fgets " + std::string(file) + " failed";
        WriteOnceLog(LOG_ERROR, errLogStr, isWriteLog);
        return false;
    }

    if (fclose(fp) != 0) {
        static bool isWriteLog = false;
        std::string errLogStr = std::string(__func__) + ": fclose " + std::string(file) + " failed";
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
    int used = snprintf_s(buffer.data(), buffer.size(), buffer.size() - 1, TRACE_TXT_HEADER_FORMAT, "", "");
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
    if ((g_writeOffset + size) > DEFAULT_CACHE_SIZE && (g_writeOffset + size) < MAX_FILE_SIZE) {
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

void SetMainThreadInfo(const int pid)
{
    if (strlen(g_appName) == 0) {
        if (!GetProcData("/proc/self/cmdline", g_appName, NAME_NORMAL_LEN)) {
            HILOG_ERROR(LOG_CORE, "get app name failed, %{public}d", errno);
        }
    }

    g_appTracePrefix = std::string(g_appName);
    SetCommStr();
    std::string pidStr = std::to_string(pid);
    std::string pidFixStr = std::string(PID_STR_MAX - pidStr.length(), ' ');
    g_appTracePrefix +=  "-" + pidStr + pidFixStr + " (" + pidFixStr + pidStr + ")";
}

bool SetAllThreadInfo(const int pid, const int tid)
{
    std::string tidStr = std::to_string(tid);
    std::string file = "/proc/self/task/" + tidStr + "/comm";
    char comm[NAME_NORMAL_LEN + 1] = {0};
    if (!GetProcData(file.c_str(), comm, NAME_NORMAL_LEN) || strlen(comm) <= 0) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_ERROR, "get comm failed", isWriteLog);
        return false;
    }
    if (comm[strlen(comm) - 1] == '\n') {
        comm[strlen(comm) - 1] = '\0';
    }
    g_appTracePrefix = std::string(comm);
    SetCommStr();

    std::string pidStr = std::to_string(pid);
    std::string tidFixStr = std::string(PID_STR_MAX - tidStr.length(), ' ');
    std::string pidFixStr = std::string(PID_STR_MAX - pidStr.length(), ' ');
    g_appTracePrefix += "-" + tidStr + tidFixStr + " (" + pidFixStr + pidStr + ")";

    return true;
}

int SetAppTraceBuffer(char* buf, const int len, const TraceMarker& traceMarker)
{
    struct timespec ts = { 0, 0 };
    clock_gettime(CLOCK_BOOTTIME, &ts);
    int cpu = sched_getcpu();
    if (cpu == -1) {
        static bool isWriteLog = false;
        WriteOnceLog(LOG_ERROR, "get cpu failed", isWriteLog);
        return -1;
    }
    constexpr int bitStrSize = 7;
    char bitStr[bitStrSize] = {0};
    ParseTagBits(traceMarker.tag, bitStr, bitStrSize);
    std::string additionalParams = "";
    int bytes = 0;
    if (traceMarker.type == MARKER_BEGIN) {
        if (*(traceMarker.customArgs) != '\0') {
            additionalParams = std::string("|") + std::string(traceMarker.customArgs);
        }
        bytes = snprintf_s(buf, len, len - 1, "  %s [%03d] .... %lu.%06lu: tracing_mark_write: B|%d|H:%s|%c%s%s\n",
            g_appTracePrefix.c_str(), cpu, static_cast<long>(ts.tv_sec), static_cast<long>(ts.tv_nsec / NS_TO_MS),
            traceMarker.pid, traceMarker.name, g_traceLevel[traceMarker.level], bitStr, additionalParams.c_str());
    } else if (traceMarker.type == MARKER_END) {
        bytes = snprintf_s(buf, len, len - 1, "  %s [%03d] .... %lu.%06lu: tracing_mark_write: E|%d|%c%s\n",
            g_appTracePrefix.c_str(), cpu, static_cast<long>(ts.tv_sec), static_cast<long>(ts.tv_nsec / NS_TO_MS),
            traceMarker.pid, g_traceLevel[traceMarker.level], bitStr);
    } else if (traceMarker.type == MARKER_ASYNC_BEGIN) {
        if (*(traceMarker.customArgs) != '\0') {
            additionalParams = std::string("|") + std::string(traceMarker.customCategory) + std::string("|") +
                               std::string(traceMarker.customArgs);
        } else if (*(traceMarker.customCategory) != '\0') {
            additionalParams = std::string("|") + std::string(traceMarker.customCategory);
        }
        bytes = snprintf_s(buf, len, len - 1,
            "  %s [%03d] .... %lu.%06lu: tracing_mark_write: S|%d|H:%s|%lld|%c%s%s\n", g_appTracePrefix.c_str(),
            cpu, static_cast<long>(ts.tv_sec), static_cast<long>(ts.tv_nsec / NS_TO_MS), traceMarker.pid,
            traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level], bitStr, additionalParams.c_str());
    } else {
        bytes = snprintf_s(buf, len, len - 1, "  %s [%03d] .... %lu.%06lu: tracing_mark_write: %c|%d|H:%s|%lld|%c%s\n",
            g_appTracePrefix.c_str(), cpu, static_cast<long>(ts.tv_sec), static_cast<long>(ts.tv_nsec / NS_TO_MS),
            g_markTypes[traceMarker.type], traceMarker.pid, traceMarker.name, traceMarker.value,
            g_traceLevel[traceMarker.level], bitStr);
    }
    return bytes;
}

void SetAppTrace(const int len, const TraceMarker& traceMarker)
{
    // get buffer
    char* buf = GetTraceBuffer(len);
    if (buf == nullptr) {
        return;
    }

    int bytes = SetAppTraceBuffer(buf, len, traceMarker);
    if (bytes > 0) {
        g_traceEventNum += 1;
        g_writeOffset += bytes;
    }
}

void WriteAppTraceLong(const int len, const TraceMarker& traceMarker)
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

    int bytes = SetAppTraceBuffer(buffer.get(), len, traceMarker);
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

void WriteAppTrace(const TraceMarker& traceMarker)
{
    int tid = getproctid();
    int len = PREFIX_MAX_SIZE + strlen(traceMarker.name) + strlen(traceMarker.customArgs) +
              strlen(traceMarker.customCategory);
    if (g_appFlag == FLAG_MAIN_THREAD && g_tgid == tid) {
        if (!CheckFileSize(len)) {
            return;
        }

        if (g_appTracePrefix.empty()) {
            SetMainThreadInfo(traceMarker.pid);
        }

        if (len <= DEFAULT_CACHE_SIZE) {
            SetAppTrace(len, traceMarker);
        } else {
            WriteAppTraceLong(len, traceMarker);
        }
    } else if (g_appFlag == FLAG_ALL_THREAD) {
        std::unique_lock<std::recursive_mutex> lock(g_appTraceMutex);
        if (!CheckFileSize(len)) {
            return;
        }

        SetAllThreadInfo(traceMarker.pid, tid);

        if (len <= DEFAULT_CACHE_SIZE) {
            SetAppTrace(len, traceMarker);
        } else {
            WriteAppTraceLong(len, traceMarker);
        }
    }
}

static int FmtSyncBeginRecord(TraceMarker& traceMarker, const char* bitStr,
    char* record, int size)
{
    int bytes = 0;
    HiTraceId hiTraceId = (traceMarker.hiTraceIdStruct == nullptr) ?
                          HiTraceChain::GetId() :
                          HiTraceId(*traceMarker.hiTraceIdStruct);
    if (hiTraceId.IsValid()) {
        if (*(traceMarker.customArgs) == '\0') {
            bytes = snprintf_s(record, size, size - 1, "B|%d|H:[%llx,%llx,%llx]#%s|%c%s", traceMarker.pid,
                hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(),
                traceMarker.name, g_traceLevel[traceMarker.level], bitStr);
        } else {
            bytes = snprintf_s(record, size, size - 1, "B|%d|H:[%llx,%llx,%llx]#%s|%c%s|%s", traceMarker.pid,
                hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(),
                traceMarker.name, g_traceLevel[traceMarker.level], bitStr, traceMarker.customArgs);
        }
    } else {
        if (*(traceMarker.customArgs) == '\0') {
            bytes = snprintf_s(record, size, size - 1, "B|%d|H:%s|%c%s", traceMarker.pid,
                traceMarker.name, g_traceLevel[traceMarker.level], bitStr);
        } else {
            bytes = snprintf_s(record, size, size - 1, "B|%d|H:%s|%c%s|%s", traceMarker.pid,
                traceMarker.name, g_traceLevel[traceMarker.level], bitStr, traceMarker.customArgs);
        }
    }
    if (UNEXPECTANTLY(bytes == -1)) {
        bytes = RECORD_SIZE_MAX;
        HILOG_DEBUG(LOG_CORE, "Trace record buffer may be truncated");
    }
    return bytes;
}

static int FmtAsyncBeginRecord(TraceMarker& traceMarker, const char* bitStr,
    char* record, int size)
{
    int bytes = 0;
    HiTraceId hiTraceId = (traceMarker.hiTraceIdStruct == nullptr) ?
                          HiTraceChain::GetId() :
                          HiTraceId(*traceMarker.hiTraceIdStruct);
    if (hiTraceId.IsValid()) {
        if (*(traceMarker.customCategory) == '\0' && *(traceMarker.customArgs) == '\0') {
            bytes = snprintf_s(record, size, size - 1, "S|%d|H:[%llx,%llx,%llx]#%s|%lld|%c%s", traceMarker.pid,
                hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(),
                traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level], bitStr);
        } else if (*(traceMarker.customArgs) == '\0') {
            bytes = snprintf_s(record, size, size - 1, "S|%d|H:[%llx,%llx,%llx]#%s|%lld|%c%s|%s", traceMarker.pid,
                hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(),
                traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level],
                bitStr, traceMarker.customCategory);
        } else {
            bytes = snprintf_s(record, size, size - 1, "S|%d|H:[%llx,%llx,%llx]#%s|%lld|%c%s|%s|%s", traceMarker.pid,
                hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId(),
                traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level],
                bitStr, traceMarker.customCategory, traceMarker.customArgs);
        }
    } else {
        if (*(traceMarker.customCategory) == '\0' && *(traceMarker.customArgs) == '\0') {
            bytes = snprintf_s(record, size, size - 1, "S|%d|H:%s|%lld|%c%s", traceMarker.pid,
                traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level], bitStr);
        } else if (*(traceMarker.customArgs) == '\0') {
            bytes = snprintf_s(record, size, size - 1, "S|%d|H:%s|%lld|%c%s|%s", traceMarker.pid,
                traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level],
                bitStr, traceMarker.customCategory);
        } else {
            bytes = snprintf_s(record, size, size - 1, "S|%d|H:%s|%lld|%c%s|%s|%s", traceMarker.pid,
                traceMarker.name, traceMarker.value, g_traceLevel[traceMarker.level],
                bitStr, traceMarker.customCategory, traceMarker.customArgs);
        }
    }
    if (bytes == -1) {
        bytes = RECORD_SIZE_MAX;
        HILOG_DEBUG(LOG_CORE, "Trace record buffer may be truncated");
    }
    return bytes;
}

static int FmtOtherTypeRecord(TraceMarker& traceMarker, const char* bitStr,
    char* record, int size)
{
    int bytes = 0;
    HiTraceId hiTraceId = (traceMarker.hiTraceIdStruct == nullptr) ?
                          HiTraceChain::GetId() :
                          HiTraceId(*traceMarker.hiTraceIdStruct);
    if (hiTraceId.IsValid()) {
        bytes = snprintf_s(record, size, size - 1, "%c|%d|H:[%llx,%llx,%llx]#%s|%lld|%c%s",
            g_markTypes[traceMarker.type], traceMarker.pid, hiTraceId.GetChainId(), hiTraceId.GetSpanId(),
            hiTraceId.GetParentSpanId(), traceMarker.name, traceMarker.value,
            g_traceLevel[traceMarker.level], bitStr);
    } else {
        bytes = snprintf_s(record, size, size - 1, "%c|%d|H:%s|%lld|%c%s",
            g_markTypes[traceMarker.type], traceMarker.pid, traceMarker.name,
            traceMarker.value, g_traceLevel[traceMarker.level], bitStr);
    }
    if (bytes == -1) {
        bytes = RECORD_SIZE_MAX;
        HILOG_DEBUG(LOG_CORE, "Trace record buffer may be truncated");
    }
    return bytes;
}

void SetNullptrToEmpty(TraceMarker& traceMarker)
{
    if (traceMarker.name == nullptr) {
        traceMarker.name = EMPTY;
    }
    if (traceMarker.customCategory == nullptr) {
        traceMarker.customCategory = EMPTY;
    }
    if (traceMarker.customArgs == nullptr) {
        traceMarker.customArgs = EMPTY;
    }
}

void AddHitraceMeterMarker(TraceMarker& traceMarker)
{
    if (traceMarker.level < HITRACE_LEVEL_DEBUG || traceMarker.level > HITRACE_LEVEL_MAX || !PrepareTraceMarker()) {
        return;
    }
    SetNullptrToEmpty(traceMarker);
    if (UNEXPECTANTLY(g_tagsProperty & traceMarker.tag) && g_markerFd != -1) {
        if (traceMarker.tag & HITRACE_TAG_COMMERCIAL) {
            traceMarker.level = HITRACE_LEVEL_COMMERCIAL;
        }
        traceMarker.pid = getprocpid();
        if ((traceMarker.level < g_levelThreshold) ||
            (traceMarker.tag == HITRACE_TAG_APP && g_appTagMatchPid > 0 && g_appTagMatchPid != traceMarker.pid)) {
            return;
        }
        char record[RECORD_SIZE_MAX + 1] = {0};
        int bytes = 0;
        constexpr int bitStrSize = 7;
        char bitStr[bitStrSize] = {0};
        ParseTagBits(traceMarker.tag, bitStr, bitStrSize);
        if (traceMarker.type == MARKER_BEGIN) {
            bytes = FmtSyncBeginRecord(traceMarker, bitStr, record, sizeof(record));
        } else if (traceMarker.type == MARKER_END) {
            bytes = snprintf_s(record, sizeof(record), sizeof(record) - 1, "E|%d|%c%s",
                traceMarker.pid, g_traceLevel[traceMarker.level], bitStr);
        } else if (traceMarker.type == MARKER_ASYNC_BEGIN) {
            bytes = FmtAsyncBeginRecord(traceMarker, bitStr, record, sizeof(record));
        } else {
            bytes = FmtOtherTypeRecord(traceMarker, bitStr, record, sizeof(record));
        }
        WriteToTraceMarker(record, bytes);
    }
    auto appTagload = g_appTag.load();
#ifdef HITRACE_UNITTEST
    appTagload = HITRACE_TAG_APP;
#endif
    if (UNEXPECTANTLY(appTagload != HITRACE_TAG_NOT_READY) && g_appFd != -1 && (traceMarker.tag & g_appTag) != 0) {
        if (traceMarker.pid == -1) {
            traceMarker.pid = getprocpid();
        }
        WriteAppTrace(traceMarker);
    }
}
}; // namespace

#ifdef HITRACE_UNITTEST
void SetCachedHandle(const char* name, CachedHandle cachedHandle)
{
    if (strcmp(name, "g_cachedHandle") == 0) {
        CachedParameterDestroy(g_cachedHandle);
        g_cachedHandle = cachedHandle;
    } else if (strcmp(name, "g_appPidCachedHandle") == 0) {
        CachedParameterDestroy(g_appPidCachedHandle);
        g_appPidCachedHandle = cachedHandle;
    } else if (strcmp(name, "g_levelThresholdCachedHandle") == 0) {
        CachedParameterDestroy(g_levelThresholdCachedHandle);
        g_levelThresholdCachedHandle = cachedHandle;
    }
    UpdateSysParamTags();
}

void SetMarkerFd(int markerFd)
{
    g_markerFd = -1;
}

void SetWriteOnceLog(LogLevel loglevel, const std::string& logStr, bool& isWrite)
{
    WriteOnceLog(loglevel, logStr, isWrite);
}
#endif

void ParseTagBits(const uint64_t tag, char* bitStr, const int bitStrSize)
{
    constexpr uint64_t tagOptionMask = HITRACE_TAG_ALWAYS | HITRACE_TAG_COMMERCIAL;
    uint64_t tagOption = tag & tagOptionMask;
    uint64_t tagWithoutOption = tag & ~tagOptionMask;
    int32_t writeIndex = 0;
    switch (tagOption) {
        case HITRACE_TAG_ALWAYS:
            bitStr[0] = '0';
            bitStr[1] = '0';
            writeIndex = 2; // 2 : after two written digits
            if (tagWithoutOption == 0) {
                bitStr[writeIndex] = '\0';
                return;
            }
            break;
        case HITRACE_TAG_COMMERCIAL:
            bitStr[0] = '0';
            bitStr[1] = '5';
            writeIndex = 2; // 2 : after two written digits
            break;
        default:
            break;
    }
    if (EXPECTANTLY((tagWithoutOption & (tagWithoutOption - 1)) == 0 && tagWithoutOption != 0)) {
        int tagIndex = __builtin_ctzll(tagWithoutOption);
        bitStr[writeIndex] = '0' + tagIndex / 10; // 10 : decimal, first digit
        bitStr[writeIndex + 1] = '0' + tagIndex % 10; // 10 : decimal, second digit
        bitStr[writeIndex + 2] = '\0'; // 2 : after two written digits
        return;
    }

    writeIndex = 0;
    uint32_t offsetBit = 1;
    uint64_t curTag = tag >> offsetBit;
    while (curTag != 0) {
        if ((curTag & 1) != 0 && writeIndex < (bitStrSize - 3)) { // 3 : two digits and '\0'
            bitStr[writeIndex] = '0' + offsetBit / 10; // 10 : decimal, first digit
            bitStr[writeIndex + 1] = '0' + offsetBit % 10; // 10 : decimal, second digit
            writeIndex += 2; // 2 : after two written digits
        }
        curTag >>= 1;
        offsetBit++;
    }
    bitStr[writeIndex] = '\0';
    return;
}

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

void StartTraceWrapper(uint64_t tag, const char* name)
{
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartTrace(uint64_t tag, const std::string& name, float limit UNUSED_PARAM)
{
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartTraceEx(HiTraceOutputLevel level, uint64_t tag, const char* name, const char* customArgs)
{
    TraceMarker traceMarker = {MARKER_BEGIN, level, tag, 0, name, EMPTY, customArgs};
    AddHitraceMeterMarker(traceMarker);
}

void StartTraceDebug(bool isDebug, uint64_t tag, const std::string& name, float limit UNUSED_PARAM)
{
    if (!isDebug) {
        return;
    }
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartTraceArgs(uint64_t tag, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartTraceArgsEx(HiTraceOutputLevel level, uint64_t tag, const char* customArgs, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_BEGIN, level, tag, 0, name, EMPTY, customArgs};
    AddHitraceMeterMarker(traceMarker);
}

void StartTraceArgsDebug(bool isDebug, uint64_t tag, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!isDebug) {
        return;
    }
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishTrace(uint64_t tag)
{
    TraceMarker traceMarker = {MARKER_END, HITRACE_LEVEL_INFO, tag, 0, EMPTY, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishTraceEx(HiTraceOutputLevel level, uint64_t tag)
{
    TraceMarker traceMarker = {MARKER_END, level, tag, 0, EMPTY, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishTraceDebug(bool isDebug, uint64_t tag)
{
    if (!isDebug) {
        return;
    }
    TraceMarker traceMarker = {MARKER_END, HITRACE_LEVEL_INFO, tag, 0, EMPTY, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTrace(uint64_t tag, const std::string& name, int32_t taskId, float limit UNUSED_PARAM)
{
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, HITRACE_LEVEL_INFO, tag, taskId, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTraceEx(HiTraceOutputLevel level, uint64_t tag, const char* name, int32_t taskId,
    const char* customCategory, const char* customArgs)
{
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, level, tag, taskId, name, customCategory, customArgs};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTraceWrapper(uint64_t tag, const char* name, int32_t taskId)
{
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, HITRACE_LEVEL_INFO, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartTraceChain(uint64_t tag, const struct HiTraceIdStruct* hiTraceId, const char* name)
{
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name, EMPTY, EMPTY, hiTraceId};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTraceDebug(bool isDebug, uint64_t tag, const std::string& name, int32_t taskId,
    float limit UNUSED_PARAM)
{
    if (!isDebug) {
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, HITRACE_LEVEL_INFO, tag, taskId, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTraceArgs(uint64_t tag, int32_t taskId, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, HITRACE_LEVEL_INFO, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTraceArgsEx(HiTraceOutputLevel level, uint64_t tag, int32_t taskId,
    const char* customCategory, const char* customArgs, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, level, tag, taskId, name, customCategory, customArgs};
    AddHitraceMeterMarker(traceMarker);
}

void StartAsyncTraceArgsDebug(bool isDebug, uint64_t tag, int32_t taskId, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!isDebug) {
        return;
    }
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_BEGIN, HITRACE_LEVEL_INFO, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTrace(uint64_t tag, const std::string& name, int32_t taskId)
{
    TraceMarker traceMarker = {MARKER_ASYNC_END, HITRACE_LEVEL_INFO, tag, taskId, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTraceEx(HiTraceOutputLevel level, uint64_t tag, const char* name, int32_t taskId)
{
    TraceMarker traceMarker = {MARKER_ASYNC_END, level, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTraceWrapper(uint64_t tag, const char* name, int32_t taskId)
{
    TraceMarker traceMarker = {MARKER_ASYNC_END, HITRACE_LEVEL_INFO, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTraceDebug(bool isDebug, uint64_t tag, const std::string& name, int32_t taskId)
{
    if (!isDebug) {
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_END, HITRACE_LEVEL_INFO, tag, taskId, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTraceArgs(uint64_t tag, int32_t taskId, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_END, HITRACE_LEVEL_INFO, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTraceArgsEx(HiTraceOutputLevel level, uint64_t tag, int32_t taskId, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_END, level, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void FinishAsyncTraceArgsDebug(bool isDebug, uint64_t tag, int32_t taskId, const char* fmt, ...)
{
    UpdateSysParamTags();
    if (!isDebug) {
        return;
    }
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_ASYNC_END, HITRACE_LEVEL_INFO, tag, taskId, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void MiddleTrace(uint64_t tag, const std::string& beforeValue UNUSED_PARAM, const std::string& afterValue)
{
    TraceMarker traceMarkerEnd = {MARKER_END, HITRACE_LEVEL_INFO, tag, 0, EMPTY, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarkerEnd);
    TraceMarker traceMarkerBegin = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, afterValue.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarkerBegin);
}

void MiddleTraceDebug(bool isDebug, uint64_t tag, const std::string& beforeValue UNUSED_PARAM,
    const std::string& afterValue)
{
    if (!isDebug) {
        return;
    }
    TraceMarker traceMarkerEnd = {MARKER_END, HITRACE_LEVEL_INFO, tag, 0, EMPTY, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarkerEnd);
    TraceMarker traceMarkerBegin = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, afterValue.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarkerBegin);
}

void CountTrace(uint64_t tag, const std::string& name, int64_t count)
{
    TraceMarker traceMarker = {MARKER_INT, HITRACE_LEVEL_INFO, tag, count, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void CountTraceEx(HiTraceOutputLevel level, uint64_t tag, const char* name, int64_t count)
{
    TraceMarker traceMarker = {MARKER_INT, level, tag, count, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void CountTraceDebug(bool isDebug, uint64_t tag, const std::string& name, int64_t count)
{
    if (!isDebug) {
        return;
    }
    TraceMarker traceMarker = {MARKER_INT, HITRACE_LEVEL_INFO, tag, count, name.c_str(), EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

void CountTraceWrapper(uint64_t tag, const char* name, int64_t count)
{
    TraceMarker traceMarker = {MARKER_INT, HITRACE_LEVEL_INFO, tag, count, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

HitraceMeterFmtScoped::HitraceMeterFmtScoped(uint64_t tag, const char* fmt, ...) : mTag(tag)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_BEGIN, HITRACE_LEVEL_INFO, tag, 0, name, EMPTY, EMPTY};
    AddHitraceMeterMarker(traceMarker);
}

HitraceMeterFmtScopedEx::HitraceMeterFmtScopedEx(HiTraceOutputLevel level, uint64_t tag,
    const char* customArgs, const char* fmt, ...) : tag_(tag), level_(level)
{
    UpdateSysParamTags();
    if (!(tag & g_tagsProperty) || UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        if ((g_appFd == -1) || ((tag & g_appTag.load()) == 0)) {
            return;
        }
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HILOG_DEBUG(LOG_CORE, "vsnprintf_s failed: %{public}d, name: %{public}s", errno, fmt);
        return;
    }
    TraceMarker traceMarker = {MARKER_BEGIN, level, tag, 0, name, EMPTY, customArgs};
    AddHitraceMeterMarker(traceMarker);
}

bool IsTagEnabled(uint64_t tag)
{
    UpdateSysParamTags();
    return ((tag & g_tagsProperty) == tag);
}

static void ResetGlobalStatus()
{
    close(g_appFd);
    g_appFd = -1;
    g_fileSize = 0;
    g_writeOffset = 0;
    g_traceEventNum = 0;
    g_appTracePrefix = "";
    g_appTag = HITRACE_TAG_NOT_READY;
    g_traceBuffer.reset();
    g_traceBuffer = nullptr;
}

static int CheckFd(int fd)
{
    if (fd == -1) {
        if (errno == ENOENT) {
            return RET_FAIL_ENOENT;
        } else if (errno == EACCES) {
            return RET_FAIL_EACCES;
        } else {
            return RET_FAILD;
        }
    }
    return RET_SUCC;
}

// For native process, the caller is responsible passing the full path of the fileName.
// For hap application, StartCaputreAppTrace() fill fileName
// as /data/app/el2/100/log/$(processname)/trace/$(processname)_$(date)_&(time).trace and return to caller.
int StartCaptureAppTrace(TraceFlag flag, uint64_t tags, uint64_t limitSize, std::string& fileName)
{
    int ret = CheckAppTraceArgs(flag, tags, limitSize);
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
            ResetGlobalStatus();
            return retval;
        }
    }

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 0644
    g_appFd = open(destFileName.c_str(), O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC, mode);
    ret = CheckFd(g_appFd);
    if (ret != RET_SUCC) {
        HILOG_ERROR(LOG_CORE, "open destFileName failed: %{public}d(%{public}s)", errno, strerror(errno));
        ResetGlobalStatus();
        return ret;
    }

    ret = InitTraceHead();
    if (ret == RET_SUCC) {
        std::unique_lock<std::mutex> lock(g_tagsChangeMutex);
        uint64_t oldTags = g_tagsProperty.load();
        uint64_t newTags = g_tagsProperty.load() | g_appTag.load();
        HandleAppTagChange(oldTags, newTags);
    } else {
        ResetGlobalStatus();
    }

    return ret;
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
    int used = snprintf_s(buffer.data(), buffer.size(), buffer.size() - 1, TRACE_TXT_HEADER_FORMAT,
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

    {
        std::unique_lock<std::mutex> lock(g_tagsChangeMutex);
        uint64_t oldTags = g_tagsProperty.load() | g_appTag.load();
        uint64_t newTags = g_tagsProperty.load();
        HandleAppTagChange(oldTags, newTags);
    }
    ResetGlobalStatus();

    return RET_SUCC;
}

HitracePerfScoped::HitracePerfScoped(bool isDebug, uint64_t tag, const std::string& name) : mTag_(tag), mName_(name)
{
    if (!isDebug) {
        return;
    }
    struct perf_event_attr peIns;
    if (memset_s(&peIns, sizeof(struct perf_event_attr), 0, sizeof(struct perf_event_attr)) != EOK) {
        err_ = errno;
        return;
    }
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
    if (memset_s(&peCycles, sizeof(struct perf_event_attr), 0, sizeof(struct perf_event_attr)) != EOK) {
        err_ = errno;
        return;
    }
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

int32_t CallbackRegistry::Register(void* callback, CallbackType type)
{
    if (!callback) {
        return INVALID_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (int32_t index = 0; index < MAX_CALLBACKS; ++index) {
        auto it = callbacks_.find(index);
        if (it != callbacks_.end()) {
            continue;
        }
        callbacks_[index] = {callback, type};
        g_isCallbacksEmpty = false;
        HILOG_INFO(LOG_CORE, "RegisterTraceListener success, index: %{public}d", index);
        bool appTagEnable = (((g_tagsProperty.load() | g_appTag.load()) & HITRACE_TAG_APP) != 0);
        TaskQueue::Instance().Enqueue([index, appTagEnable] {
            CallbackRegistry::Instance().ExecuteOne(index, appTagEnable);
        });
        return index;
    }

    return REACH_MAX_CALLBACKS;
}

int32_t CallbackRegistry::Unregister(int32_t index)
{
    if (index < 0 || index >= MAX_CALLBACKS) {
        return INVALID_INDEX;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = callbacks_.find(index);
    if (it == callbacks_.end()) {
        return INDEX_NOT_REGISTERED;
    }

    switch (it->second.type) {
        case CallbackType::Napi:
            if (deleteCallbackNapi_) {
                deleteCallbackNapi_(it->second.callback);
            }
            break;
        case CallbackType::Ani:
            if (deleteCallbackAni_) {
                deleteCallbackAni_(it->second.callback);
            }
            break;
        case CallbackType::Native:
            break;
    }

    callbacks_.erase(it);
    if (callbacks_.empty()) {
        g_isCallbacksEmpty = true;
    }
    HILOG_INFO(LOG_CORE, "UnregisterTraceListener success, index: %{public}d", index);
    return SUCCESS_UNREGISTER;
}

void CallbackRegistry::ExecuteAll(bool appTagEnable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, handle] : callbacks_) {
        switch (handle.type) {
            case CallbackType::Native: {
                HILOG_INFO(LOG_CORE, "Execute Native");
                auto cb = reinterpret_cast<void(*)(bool)>(handle.callback);
                cb(appTagEnable);
                break;
            }
            case CallbackType::Napi: {
                HILOG_INFO(LOG_CORE, "Execute Napi");
                if (executeCallbackNapi_) {
                    executeCallbackNapi_(handle.callback, appTagEnable);
                }
                break;
            }
            case CallbackType::Ani: {
                HILOG_INFO(LOG_CORE, "Execute Ani");
                if (executeCallbackAni_) {
                    executeCallbackAni_(handle.callback, appTagEnable);
                }
                break;
            }
        }
    }
}

void CallbackRegistry::ExecuteOne(int32_t index, bool appTagEnable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = callbacks_.find(index);
    if (it == callbacks_.end()) {
        HILOG_WARN(LOG_CORE, "callback not exist, index: %{public}d", index);
        return;
    }
    switch (it->second.type) {
        case CallbackType::Native: {
            HILOG_INFO(LOG_CORE, "Execute Native");
            auto cb = reinterpret_cast<void(*)(bool)>(it->second.callback);
            cb(appTagEnable);
            break;
        }
        case CallbackType::Napi: {
            HILOG_INFO(LOG_CORE, "Execute Napi");
            if (executeCallbackNapi_) {
                executeCallbackNapi_(it->second.callback, appTagEnable);
            }
            break;
        }
        case CallbackType::Ani: {
            HILOG_INFO(LOG_CORE, "Execute Ani");
            if (executeCallbackAni_) {
                executeCallbackAni_(it->second.callback, appTagEnable);
            }
            break;
        }
    }
}

void CallbackRegistry::SetCallbacksNapi(ExecuteCallbackNapi executeCallbackNapi, DeleteCallbackNapi deleteCallbackNapi)
{
    executeCallbackNapi_ = executeCallbackNapi;
    deleteCallbackNapi_ = deleteCallbackNapi;
}

void CallbackRegistry::SetCallbacksAni(ExecuteCallbackAni executeCallbackAni, DeleteCallbackAni deleteCallbackAni)
{
    executeCallbackAni_ = executeCallbackAni;
    deleteCallbackAni_ = deleteCallbackAni;
}

void SetCallbacksNapi(ExecuteCallbackNapi executeCallbackNapi, DeleteCallbackNapi deleteCallbackNapi)
{
    CallbackRegistry::Instance().SetCallbacksNapi(executeCallbackNapi, deleteCallbackNapi);
}

void SetCallbacksAni(ExecuteCallbackAni executeCallbackAni, DeleteCallbackAni deleteCallbackAni)
{
    CallbackRegistry::Instance().SetCallbacksAni(executeCallbackAni, deleteCallbackAni);
}

int32_t RegisterTraceListener(TraceEventListener callback)
{
    return CallbackRegistry::Instance().Register(reinterpret_cast<void *>(callback));
}

int32_t RegisterTraceListenerNapi(void* callback)
{
    return CallbackRegistry::Instance().Register(callback, CallbackType::Napi);
}

int32_t RegisterTraceListenerAni(void* callback)
{
    return CallbackRegistry::Instance().Register(callback, CallbackType::Ani);
}

int32_t UnregisterTraceListener(int32_t index)
{
    return CallbackRegistry::Instance().Unregister(index);
}

int32_t UnregisterTraceListenerNapi(int32_t index)
{
    return CallbackRegistry::Instance().Unregister(index);
}

int32_t UnregisterTraceListenerAni(int32_t index)
{
    return CallbackRegistry::Instance().Unregister(index);
}
