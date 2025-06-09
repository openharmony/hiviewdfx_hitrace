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

#include "trace_dump_executor.h"

#include <securec.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "hilog/log.h"
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
#define LOG_TAG "HitraceDumpExecutor"
#endif
namespace {
constexpr int BYTE_PER_KB = 1024;
constexpr int DEFAULT_FILE_SIZE = 100 * 1024;
constexpr int MAX_NEW_TRACE_FILE_LIMIT = 5;
#ifdef HITRACE_UNITTEST
constexpr int DEFAULT_CACHE_FILE_SIZE = 15 * 1024;
#else
constexpr int DEFAULT_CACHE_FILE_SIZE = 150 * 1024;
#endif
constexpr uint64_t SYNC_RETURN_TIMEOUT_NS = 5000000000; // 5s
constexpr int64_t ASYNC_DUMP_FILE_SIZE_ADDITION = 1024 * 1024; // 1MB

std::vector<FileWithTime> g_looptraceFiles;
std::shared_ptr<ProductConfigJsonParser> g_ProductConfigParser
    = std::make_shared<ProductConfigJsonParser>("/sys_prod/etc/hiview/hitrace/hitrace_param.json");
uint64_t g_sliceMaxDuration;

static bool g_isRootVer = IsRootVersion();

// use for control status of trace dump loop.
std::atomic<bool> g_isDumpRunning(false);
std::atomic<bool> g_isDumpEnded(true);
std::atomic<bool> g_interruptCache(false);
std::atomic<bool> g_readFlag(true);
std::atomic<bool> g_writeFlag(true);

std::vector<std::string> FilterLoopTraceResult(const std::vector<FileWithTime>& looptraceFiles)
{
    std::vector<std::string> outputFiles = {};
    for (const auto& output : looptraceFiles) {
        if (output.isNewFile) {
            outputFiles.emplace_back(output.filename);
        }
    }
    return outputFiles;
}

bool IsGenerateNewFile(std::shared_ptr<ITraceSource> traceSource, const TRACE_TYPE traceType, int& count)
{
    if (access(traceSource->GetTraceFilePath().c_str(), F_OK) == 0) {
        return false;
    }
    if (count > MAX_NEW_TRACE_FILE_LIMIT) {
        HILOG_ERROR(LOG_CORE, "create new trace file limited.");
        return false;
    }
    count++;
    auto newFileName = GenerateTraceFileName(traceType);
    traceSource->UpdateTraceFile(newFileName);
    HILOG_INFO(LOG_CORE, "IsGenerateNewFile: update tracesource filename : %{public}s", newFileName.c_str());
    return true;
}

void PreTraceContentDump(const TraceContentPtr& traceContentPtr)
{
    traceContentPtr.fileHdr->ResetCurrentFileSize();
    traceContentPtr.fileHdr->WriteTraceContent();
    traceContentPtr.baseInfo->WriteTraceContent();
    traceContentPtr.eventFmt->WriteTraceContent();
}

void AfterTraceContentDump(const TraceContentPtr& traceContentPtr)
{
    traceContentPtr.cmdLines->WriteTraceContent();
    traceContentPtr.tgids->WriteTraceContent();
    traceContentPtr.headerPage->WriteTraceContent();
    traceContentPtr.printkFmt->WriteTraceContent();
}
}

bool ITraceDumpStrategy::CreateTraceContentPtr(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request, TraceContentPtr& contentPtr)
{
    contentPtr.fileHdr = traceSource->GetTraceFileHeader();
    if (contentPtr.fileHdr == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceFileHeader failed.");
        return false;
    }
    contentPtr.baseInfo = traceSource->GetTraceBaseInfo();
    if (contentPtr.baseInfo == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceBaseInfo failed.");
        return false;
    }
    contentPtr.eventFmt = traceSource->GetTraceEventFmt();
    if (contentPtr.eventFmt == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceEventFmt failed.");
        return false;
    }
    contentPtr.cpuRaw = traceSource->GetTraceCpuRaw(request);
    if (contentPtr.cpuRaw == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceCpuRaw failed.");
        return false;
    }
    contentPtr.cmdLines = traceSource->GetTraceCmdLines();
    if (contentPtr.cmdLines == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceCmdLines failed.");
        return false;
    }
    contentPtr.tgids = traceSource->GetTraceTgids();
    if (contentPtr.tgids == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceTgids failed.");
        return false;
    }
    contentPtr.headerPage = traceSource->GetTraceHeaderPage();
    if (contentPtr.headerPage == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTraceHeaderPage failed.");
        return false;
    }
    contentPtr.printkFmt = traceSource->GetTracePrintkFmt();
    if (contentPtr.printkFmt == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: GetTracePrintkFmt failed.");
        return false;
    }
    return true;
}

TraceDumpRet SnapshotTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    struct TraceContentPtr traceContentPtr;
    if (!CreateTraceContentPtr(traceSource, request, traceContentPtr)) {
        HILOG_ERROR(LOG_CORE, "SnapshotTraceDumpStrategy: CreateTraceContentPtr failed.");
        return {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0};
    }
    PreTraceContentDump(traceContentPtr);
    if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
        return {traceContentPtr.cpuRaw->GetDumpStatus(), "", 0, 0};
    }
    AfterTraceContentDump(traceContentPtr);

    auto traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
    TraceDumpRet ret = {
        .code = traceContentPtr.cpuRaw->GetDumpStatus(),
        .traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp(),
        .traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp(),
    };
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "SnapshotTraceDumpStrategy: strncpy_s failed.");
    }

    return ret;
}

bool RecordTraceDumpStrategy::ProcessTraceContent(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request, TraceContentPtr& traceContentPtr, TraceProcessResult& result)
{
    while (g_isDumpRunning.load()) {
        sleep(1);
        auto updatedRequest = request;
        updatedRequest.traceEndTime = GetCurBootTime();
        traceContentPtr.cpuRaw = traceSource->GetTraceCpuRaw(updatedRequest);
        if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
            return false;
        }
        if (traceContentPtr.cpuRaw->IsOverFlow()) {
            HILOG_INFO(LOG_CORE, "RecordTraceDumpStrategy: write trace content overflow.");
            break;
        }
        result.code = traceContentPtr.cpuRaw->GetDumpStatus();
        result.traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
        result.traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
        result.traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
    }
    return true;
}

TraceDumpRet RecordTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    int newFileCount = 1;
    TraceProcessResult result = {};
    do {
        struct TraceContentPtr traceContentPtr;
        if (!CreateTraceContentPtr(traceSource, request, traceContentPtr)) {
            HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: CreateTraceContentPtr failed.");
            return {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0};
        }
        PreTraceContentDump(traceContentPtr);
        if (!ProcessTraceContent(traceSource, request, traceContentPtr, result)) {
            if (!IsGenerateNewFile(traceSource, TRACE_TYPE::TRACE_RECORDING, newFileCount)) {
                HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: write raw trace content failed.");
                break;
            }
            continue;
        }
        AfterTraceContentDump(traceContentPtr);
    } while (IsGenerateNewFile(traceSource, TRACE_TYPE::TRACE_RECORDING, newFileCount));

    TraceDumpRet ret = {
        .code = result.code,
        .traceStartTime = result.traceStartTime,
        .traceEndTime = result.traceEndTime,
    };
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, result.traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: strncpy_s failed.");
    }
    return ret;
}

bool CacheTraceDumpStrategy::ProcessTraceContent(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request, TraceContentPtr& traceContentPtr, TraceProcessResult& result,
    uint64_t& sliceDuration)
{
    while (g_isDumpRunning.load()) {
        struct timespec bts = {0, 0};
        clock_gettime(CLOCK_BOOTTIME, &bts);
        uint64_t startTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
        sleep(1);
        if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
            return false;
        }
        clock_gettime(CLOCK_BOOTTIME, &bts);
        uint64_t endTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
        uint64_t timeDiff = (endTime - startTime) / S_TO_NS;
        result.code = traceContentPtr.cpuRaw->GetDumpStatus();
        result.traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
        result.traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
        result.traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
        sliceDuration += timeDiff;
        if (sliceDuration >= g_sliceMaxDuration || g_interruptCache.load()) {
            sliceDuration = 0;
            break;
        }
    }
    return true;
}

TraceDumpRet CacheTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    int newFileCount = 1;
    TraceProcessResult result = {};
    uint64_t sliceDuration = 0;
    do {
        struct TraceContentPtr traceContentPtr;
        if (!CreateTraceContentPtr(traceSource, request, traceContentPtr)) {
            return {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0};
        }
        PreTraceContentDump(traceContentPtr);
        if (!ProcessTraceContent(traceSource, request, traceContentPtr, result, sliceDuration)) {
            if (!IsGenerateNewFile(traceSource, TRACE_TYPE::TRACE_CACHE, newFileCount)) {
                HILOG_ERROR(LOG_CORE, "CacheTraceDumpStrategy: write raw trace content failed.");
                break;
            }
            continue;
        }
        AfterTraceContentDump(traceContentPtr);
    } while (IsGenerateNewFile(traceSource, TRACE_TYPE::TRACE_CACHE, newFileCount));

    TraceDumpRet ret = {
        .code = result.code,
        .traceStartTime = result.traceStartTime,
        .traceEndTime = result.traceEndTime,
    };
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, result.traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "CacheTraceDumpStrategy: strncpy_s failed.");
    }
    return ret;
}

TraceDumpRet AsyncTraceReadStrategy::Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request)
{
    auto cpuRawRead = traceSource->GetTraceCpuRawRead(request);
    if (!cpuRawRead->WriteTraceContent()) {
        return {cpuRawRead->GetDumpStatus(), "", 0, 0};
    }

    TraceDumpRet ret = {
        .code = cpuRawRead->GetDumpStatus(),
        .fileSize = static_cast<int64_t>(TraceBufferManager::GetInstance().GetTaskTotalUsedBytes(request.taskId)),
        .traceStartTime = cpuRawRead->GetFirstPageTimeStamp(),
        .traceEndTime = cpuRawRead->GetLastPageTimeStamp(),
    };
    auto tracefile = GenerateTraceFileNameByTraceTime(request.type, ret.traceStartTime, ret.traceEndTime);
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, tracefile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "AsyncTraceReadStrategy: strncpy_s failed.");
    }
    HILOG_INFO(LOG_CORE, "AsyncTraceReadStrategy: trace file : %{public}s, file size : %{public}" PRId64,
        ret.outputFile, ret.fileSize);
    return ret;
}

TraceDumpRet AsyncTraceWriteStrategy::Execute(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    struct TraceContentPtr traceContentPtr;
    if (!CreateTraceContentPtr(traceSource, request, traceContentPtr)) {
        HILOG_ERROR(LOG_CORE, "AsyncTraceWriteStrategy: CreateTraceContentPtr failed.");
        return {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0};
    }
    PreTraceContentDump(traceContentPtr);
    auto cpuRawWrite = traceSource->GetTraceCpuRawWrite(request.taskId);
    if (!cpuRawWrite->WriteTraceContent()) {
        return {TraceErrorCode::FILE_ERROR, "", 0, 0};
    }
    AfterTraceContentDump(traceContentPtr);
    TraceDumpRet ret = {
        .code = TraceErrorCode::SUCCESS
    };
    return ret;
}

TraceDumpExecutor::TraceDumpExecutor()
{
    if (!IsTraceMounted(tracefsDir_)) {
        HILOG_ERROR(LOG_CORE, "TraceDumpExecutor: Trace is not mounted.");
    }
}

bool TraceDumpExecutor::PreCheckDumpTraceLoopStatus()
{
    if (!g_isDumpEnded.load()) {
        return false;
    }
    g_isDumpRunning.store(true);
    g_isDumpEnded.store(false);
    return true;
}

bool TraceDumpExecutor::StartDumpTraceLoop(const TraceDumpParam& param)
{
    {
        std::lock_guard<std::mutex> lck(traceFileMutex_);
        g_looptraceFiles.clear();
        GetTraceFilesInDir(g_looptraceFiles, TRACE_TYPE::TRACE_RECORDING);
        if (param.type == TRACE_TYPE::TRACE_RECORDING &&
            (!g_isRootVer || g_ProductConfigParser->GetRootAgeingStatus() == ConfigStatus::ENABLE)) {
            DelOldRecordTraceFile(g_looptraceFiles, param.fileLimit, g_ProductConfigParser->GetRecordFileSizeKb());
        }
    }

    if (param.fileSize == 0 && g_isRootVer) {
        std::string traceFile = param.outputFile.empty() ? GenerateTraceFileName(param.type) : param.outputFile;
        if (DoDumpTraceLoop(param, traceFile, false)) {
            std::lock_guard<std::mutex> lck(traceFileMutex_);
            g_looptraceFiles.emplace_back(traceFile);
        }
        g_isDumpEnded.store(true);
        g_isDumpRunning.store(false);
        return true;
    }

    while (g_isDumpRunning.load()) {
        if (!g_isRootVer || g_ProductConfigParser->GetRootAgeingStatus() == ConfigStatus::ENABLE) {
            std::lock_guard<std::mutex> lck(traceFileMutex_);
            ClearOldTraceFile(g_looptraceFiles, param.fileLimit, g_ProductConfigParser->GetRecordFileSizeKb());
        }
        std::string traceFile = GenerateTraceFileName(param.type);
        if (DoDumpTraceLoop(param, traceFile, true)) {
            std::lock_guard<std::mutex> lck(traceFileMutex_);
            g_looptraceFiles.emplace_back(traceFile);
        } else {
            break;
        }
    }
    g_isDumpEnded.store(true);
    g_isDumpRunning.store(false);
    return true;
}

std::vector<std::string> TraceDumpExecutor::StopDumpTraceLoop()
{
    g_isDumpRunning.store(false);
    while (!g_isDumpEnded.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100 : 100ms
        g_isDumpRunning.store(false);
    }

    std::lock_guard<std::mutex> lck(traceFileMutex_);
    return FilterLoopTraceResult(g_looptraceFiles);
}

bool TraceDumpExecutor::StartCacheTraceLoop(const TraceDumpParam& param,
    uint64_t totalFileSize, uint64_t sliceMaxDuration)
{
    g_sliceMaxDuration = sliceMaxDuration;
    while (g_isDumpRunning.load()) {
        auto traceFile = GenerateTraceFileName(param.type);
        if (DoDumpTraceLoop(param, traceFile, true)) {
            std::lock_guard<std::mutex> cacheLock(traceFileMutex_);
            ClearCacheTraceFileBySize(traceFiles_, totalFileSize);
            HILOG_INFO(LOG_CORE, "ProcessCacheTask: save cache file.");
        } else {
            break;
        }
    }
    g_isDumpEnded.store(true);
    return true;
}

void TraceDumpExecutor::StopCacheTraceLoop()
{
    g_isDumpRunning.store(false);
    while (!g_isDumpEnded.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100 : 100ms
        g_isDumpRunning.store(false);
    }
}

TraceDumpRet TraceDumpExecutor::DumpTrace(const TraceDumpParam& param)
{
    MarkClockSync(tracefsDir_);
    constexpr int waitTime = 10000; // 10ms
    usleep(waitTime);
    const std::string traceFile = GenerateTraceFileName(param.type);
    return DumpTraceInner(param, traceFile);
}

std::vector<TraceFileInfo> TraceDumpExecutor::GetCacheTraceFiles()
{
    g_interruptCache.store(true);
    std::vector<TraceFileInfo> cacheFiles;
    {
        std::lock_guard<std::mutex> lock(traceFileMutex_);
        cacheFiles = traceFiles_;
        g_interruptCache.store(false);
    }
    return cacheFiles;
}

void TraceDumpExecutor::ReadRawTraceLoop()
{
    const std::string threadName = "ReadRawTraceLoop";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "ReadRawTraceLoop start.");
    while (g_readFlag.load()) {
        TraceDumpTask currentTask;
        bool hasTask = false;
        {
            std::unique_lock<std::mutex> lck(taskQueueMutex_);
            readCondVar_.wait(lck, [this]() {
                return !g_readFlag.load() ||
                    std::any_of(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(), [](const TraceDumpTask& task) {
                        return task.status == TraceDumpStatus::START && task.code == TraceErrorCode::UNSET;
                    });
            });
            if (!g_readFlag.load()) {
                break;
            }
            auto it = std::find_if(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(), [](const TraceDumpTask& task) {
                return task.status == TraceDumpStatus::START && task.code == TraceErrorCode::UNSET;
            });
            if (it != traceDumpTaskVec_.end()) {
                currentTask = *it;
                hasTask = true;
            }
        }
        if (hasTask) {
            HILOG_INFO(LOG_CORE, "ReadRawTraceLoop : start read trace of taskid[%{public}" PRIu64 "]",
                currentTask.time);
            if (!DoReadRawTrace(currentTask)) {
                HILOG_WARN(LOG_CORE, "ReadRawTraceLoop : do read raw trace failed, taskid[%{public}" PRIu64 "]",
                    currentTask.time);
            } else {
                HILOG_INFO(LOG_CORE, "ReadRawTraceLoop : read raw trace done, taskid[%{public}" PRIu64 "]",
                    currentTask.time);
                writeCondVar_.notify_one();
            }
        }
    }
    HILOG_INFO(LOG_CORE, "ReadRawTraceLoop end.");
}

void TraceDumpExecutor::WriteTraceLoop()
{
    const std::string threadName = "WriteTraceLoop";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "WriteTraceLoop start.");
    while (g_writeFlag.load()) {
        TraceDumpTask currentTask;
        bool hasTask = false;
        {
            std::unique_lock<std::mutex> lck(taskQueueMutex_);
            writeCondVar_.wait(lck, [this]() {
                return !g_writeFlag.load() ||
                    std::any_of(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(), [](const TraceDumpTask& task) {
                        return task.status == TraceDumpStatus::READ_DONE || task.status == TraceDumpStatus::WAIT_WRITE;
                    });
            });
            if (!g_writeFlag.load()) {
                break;
            }
            auto it = std::find_if(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(), [](const TraceDumpTask& task) {
                return task.status == TraceDumpStatus::READ_DONE || task.status == TraceDumpStatus::WAIT_WRITE;
            });
            if (it != traceDumpTaskVec_.end()) {
                currentTask = *it;
                hasTask = true;
            }
        }
        if (hasTask) {
            HILOG_INFO(LOG_CORE, "WriteTraceLoop : start write trace of taskid[%{public}" PRIu64 "]", currentTask.time);
            if (!DoWriteRawTrace(currentTask)) {
                HILOG_WARN(LOG_CORE, "WriteTraceLoop : do write raw trace failed, taskid[%{public}" PRIu64 "]",
                    currentTask.time);
            } else {
                HILOG_INFO(LOG_CORE, "WriteTraceLoop : write raw trace done, taskid[%{public}" PRIu64 "]",
                    currentTask.time);
            }
        }
    }
    HILOG_INFO(LOG_CORE, "WriteTraceLoop end.");
}

void TraceDumpExecutor::ProcessNewTask(std::shared_ptr<HitraceDumpPipe>& dumpPipe, int& sleepCnt)
{
    TraceDumpTask newTask;
    if (dumpPipe->ReadTraceTask(200, newTask)) { // 200 : 200ms
        std::lock_guard<std::mutex> lck(taskQueueMutex_);
        traceDumpTaskVec_.push_back(newTask);
        readCondVar_.notify_one();
        sleepCnt = 0;
    }
}

void TraceDumpExecutor::DoProcessTraceDumpTask(std::shared_ptr<HitraceDumpPipe>& dumpPipe, TraceDumpTask& task,
    std::vector<TraceDumpTask>& completedTasks)
{
    uint64_t curBootTime = GetCurBootTime();
    if (task.status == TraceDumpStatus::WRITE_DONE) {
        if (task.hasSyncReturn && dumpPipe->WriteAsyncReturn(task)) { // Async return
            completedTasks.push_back(task);
        } else if (!task.hasSyncReturn && dumpPipe->WriteSyncReturn(task)) { // Sync return
            task.hasSyncReturn = true;
            if (curBootTime - task.time <= SYNC_RETURN_TIMEOUT_NS) {
                completedTasks.push_back(task);
            }
        }
    } else if (task.status == TraceDumpStatus::READ_DONE) {
        if (task.code != TraceErrorCode::SUCCESS && task.code != TraceErrorCode::SIZE_EXCEED_LIMIT) {
            auto writeRet = false;
            if (!task.hasSyncReturn) {
                writeRet = dumpPipe->WriteSyncReturn(task);
            } else {
                writeRet = dumpPipe->WriteAsyncReturn(task);
            }
            if (writeRet) {
                completedTasks.push_back(task);
            }
        } else if (!task.hasSyncReturn && curBootTime - task.time > SYNC_RETURN_TIMEOUT_NS) { // write trace timeout
            if (dumpPipe->WriteSyncReturn(task)) {
                task.hasSyncReturn = true;
                task.status = TraceDumpStatus::WAIT_WRITE;
                writeCondVar_.notify_one();
            }
        }
    }
}

void TraceDumpExecutor::TraceDumpTaskMonitor()
{
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
    int sleepCnt = 0;
    while (true) {
        std::vector<TraceDumpTask> completedTasks;
        {
            std::lock_guard<std::mutex> lck(taskQueueMutex_);
            for (auto& task : traceDumpTaskVec_) {
                DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
            }
            // Remove completed tasks
            for (const auto& task : completedTasks) {
                auto it = std::remove_if(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(),
                    [&task](const TraceDumpTask& t) { return t.time == task.time; });
                traceDumpTaskVec_.erase(it, traceDumpTaskVec_.end());
            }
            if (!traceDumpTaskVec_.empty()) {
                sleepCnt = 0;
            }
        }
        // Check for new tasks
        ProcessNewTask(dumpPipe, sleepCnt);
        sleep(1);
        sleepCnt++;
        if (sleepCnt >= 15 && IsTraceDumpTaskEmpty()) { // 15 : similar to 15 seconds
            HILOG_INFO(LOG_CORE, "TraceDumpTaskMonitor : no task, dump process exit.");
            g_readFlag.store(false);
            g_writeFlag.store(false);
            readCondVar_.notify_all();
            writeCondVar_.notify_all();
            break;
        }
    }
}

void TraceDumpExecutor::RemoveTraceDumpTask(const uint64_t time)
{
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    auto it = std::remove_if(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(),
        [time](const TraceDumpTask& task) { return task.time == time; });
    if (it != traceDumpTaskVec_.end()) {
        traceDumpTaskVec_.erase(it, traceDumpTaskVec_.end());
        HILOG_INFO(LOG_CORE, "EraseTraceDumpTask: task removed from task list.");
    } else {
        HILOG_WARN(LOG_CORE, "EraseTraceDumpTask: task not found in task list.");
    }
}

bool TraceDumpExecutor::UpdateTraceDumpTask(const TraceDumpTask& task)
{
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    for (auto& dumpTask : traceDumpTaskVec_) {
        if (dumpTask.time == task.time) {
            // attention: avoid updating hasSyncReturn field, it is only used in monitor thread.
            dumpTask.code = task.code;
            dumpTask.status = task.status;
            dumpTask.fileSize = task.fileSize;
            dumpTask.traceStartTime = task.traceStartTime;
            dumpTask.traceEndTime = task.traceEndTime;
            if (strcpy_s(dumpTask.outputFile, sizeof(dumpTask.outputFile), task.outputFile) != 0) {
                HILOG_ERROR(LOG_CORE, "UpdateTraceDumpTask: strcpy_s failed.");
            }
            HILOG_INFO(LOG_CORE, "UpdateTraceDumpTask: task id: %{public}" PRIu64 ", status: %{public}hhu, "
                "file: %{public}s, filesize: %{public}" PRId64 ".",
                task.time, task.status, task.outputFile, task.fileSize);
            return true;
        }
    }
    HILOG_WARN(LOG_CORE, "UpdateTraceDumpTask: task[%{public}" PRIu64 "] not found in lists.", task.time);
    return false;
}

void TraceDumpExecutor::AddTraceDumpTask(const TraceDumpTask& task)
{
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    traceDumpTaskVec_.emplace_back(task);
    readCondVar_.notify_one();
    HILOG_INFO(LOG_CORE, "AddTraceDumpTask: task added to the list.");
}

bool TraceDumpExecutor::IsTraceDumpTaskEmpty()
{
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    return traceDumpTaskVec_.empty();
}

size_t TraceDumpExecutor::GetTraceDumpTaskCount()
{
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    return traceDumpTaskVec_.size();
}

#ifdef HITRACE_UNITTEST
void TraceDumpExecutor::ClearCacheTraceFiles()
{
    std::lock_guard<std::mutex> lck(traceFileMutex_);
    traceFiles_.clear();
}
#endif

void TraceDumpExecutor::SetTraceDumpStrategy(std::unique_ptr<ITraceDumpStrategy> strategy)
{
    dumpStrategy_ = std::move(strategy);
}

TraceDumpRet TraceDumpExecutor::ExecuteDumpTrace(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    if (dumpStrategy_ == nullptr) {
        HILOG_ERROR(LOG_CORE, "ExecuteDumpTrace : No write strategy set!");
        return { TraceErrorCode::FILE_ERROR, "", 0, 0 }; // todo
    }
    return dumpStrategy_->Execute(traceSource, request);
}

bool TraceDumpExecutor::DoDumpTraceLoop(const TraceDumpParam& param, std::string& traceFile, bool isLimited)
{
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace : Trace fs path is empty.");
        return false;
    }
    MarkClockSync(tracefsDir_);
    int fileSizeThreshold = param.type == TRACE_TYPE::TRACE_CACHE ?
        DEFAULT_CACHE_FILE_SIZE * BYTE_PER_KB : DEFAULT_FILE_SIZE * BYTE_PER_KB;
    if (param.fileSize != 0) {
        fileSizeThreshold = param.fileSize * BYTE_PER_KB;
    }
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(tracefsDir_, traceFile);
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(tracefsDir_, traceFile);
    }
    if (param.type == TRACE_TYPE::TRACE_RECORDING) {
        SetTraceDumpStrategy(std::make_unique<RecordTraceDumpStrategy>());
    } else {
        SetTraceDumpStrategy(std::make_unique<CacheTraceDumpStrategy>());
    }

    TraceDumpRequest request = { param.type, fileSizeThreshold, isLimited, param.traceStartTime, param.traceEndTime };

    std::lock_guard<std::mutex> lck(traceFileMutex_);
    auto dumpRet = ExecuteDumpTrace(traceSource, request);
    HILOG_INFO(LOG_CORE, "DoDumpTraceLoop: ExecuteDumpTrace done, errorcode: %{public}d, tracefile: %{public}s",
        static_cast<uint8_t>(dumpRet.code), dumpRet.outputFile);
    if (dumpRet.code != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Execute loop trace dump failed.");
        return false;
    }
    traceFile = traceSource->GetTraceFilePath();
    if (param.type == TRACE_TYPE::TRACE_CACHE) {
        TraceFileInfo traceFileInfo;
        if (!SetFileInfo(true, traceFile, dumpRet.traceStartTime, dumpRet.traceEndTime, traceFileInfo)) {
            RemoveFile(traceFile);
            return false;
        }
        traceFile = traceFileInfo.filename;
        traceFiles_.emplace_back(traceFileInfo);
    }
    if (access(traceFile.c_str(), F_OK) == -1) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Trace file (%{public}s) not found.", traceFile.c_str());
        return false;
    }
    return true;
}

TraceDumpRet TraceDumpExecutor::DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile)
{
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace : Trace fs path is empty.");
        return { TraceErrorCode::TRACE_NOT_SUPPORTED, "", 0, 0 }; // todo
    }
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(tracefsDir_, traceFile);
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(tracefsDir_, traceFile);
    }

    TraceDumpRequest request = {
        param.type,
        0,
        false,
        param.traceStartTime,
        param.traceEndTime
    };

    SetTraceDumpStrategy(std::make_unique<SnapshotTraceDumpStrategy>());
    return ExecuteDumpTrace(traceSource, request);
}

bool TraceDumpExecutor::DoReadRawTrace(TraceDumpTask& task)
{
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DoReadRawTrace : Trace fs path is empty.");
        task.code = TraceErrorCode::TRACE_NOT_SUPPORTED;
        return false;
    }
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(tracefsDir_, "");
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(tracefsDir_, "");
    }

    TraceDumpRequest request = {
        TRACE_TYPE::TRACE_SNAPSHOT,
        0,
        false,
        task.traceStartTime,
        task.traceEndTime,
        task.time
    };
    SetTraceDumpStrategy(std::make_unique<AsyncTraceReadStrategy>());
    auto ret = ExecuteDumpTrace(traceSource, request);
    task.code = ret.code;
    task.fileSize = ret.fileSize + ASYNC_DUMP_FILE_SIZE_ADDITION;
    if (strncpy_s(task.outputFile, TRACE_FILE_LEN, ret.outputFile, TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "DoReadRawTrace: strncpy_s failed.");
    }
    task.status = TraceDumpStatus::READ_DONE;
    if (task.code == TraceErrorCode::SUCCESS && task.fileSize > task.fileSizeLimit) {
        task.code = TraceErrorCode::SIZE_EXCEED_LIMIT;
    }
    if (!UpdateTraceDumpTask(task)) {
        HILOG_ERROR(LOG_CORE, "DoReadRawTrace: update trace dump task failed.");
        return false;
    }
    return task.code == TraceErrorCode::SUCCESS || task.code == TraceErrorCode::SIZE_EXCEED_LIMIT;
}

bool TraceDumpExecutor::DoWriteRawTrace(TraceDumpTask& task)
{
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DoWriteRawTrace : Trace fs path is empty.");
        task.code = TraceErrorCode::TRACE_NOT_SUPPORTED;
        return false;
    }
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(tracefsDir_, task.outputFile);
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(tracefsDir_, task.outputFile);
    }

    TraceDumpRequest request = {
        TRACE_TYPE::TRACE_SNAPSHOT,
        0,
        false,
        task.traceStartTime,
        task.traceEndTime,
        task.time
    };
    SetTraceDumpStrategy(std::make_unique<AsyncTraceWriteStrategy>());
    auto ret = ExecuteDumpTrace(traceSource, request);
    task.code = ret.code;
    task.fileSize = static_cast<int64_t>(GetFileSize(std::string(task.outputFile)));
    task.status = TraceDumpStatus::WRITE_DONE;
    if (task.code == TraceErrorCode::SUCCESS && task.fileSize > task.fileSizeLimit) {
        task.code = TraceErrorCode::SIZE_EXCEED_LIMIT;
    }
#ifdef HITRACE_UNITTEST
    sleep(10); // 10 : sleep 10 seconds to construct a timeout task
#endif
    if (!UpdateTraceDumpTask(task)) {
        HILOG_ERROR(LOG_CORE, "DoWriteRawTrace: update trace dump task failed.");
        return false;
    }
    return task.code == TraceErrorCode::SUCCESS || task.code == TraceErrorCode::SIZE_EXCEED_LIMIT;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS