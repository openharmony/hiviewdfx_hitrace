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
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "file_ageing_utils.h"
#include "hilog/log.h"
#include "trace_dump_state.h"
#include "trace_file_utils.h"
#include "trace_strategy_factory.h"

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
constexpr int READ_TIMEOUT_MS = 200;
constexpr int MONITOR_EMPTY_LOOP_COUNT = 15;
#ifdef HITRACE_UNITTEST
constexpr int DEFAULT_CACHE_FILE_SIZE = 15 * 1024;
#else
constexpr int DEFAULT_CACHE_FILE_SIZE = 150 * 1024;
#endif
constexpr int64_t ASYNC_DUMP_FILE_SIZE_ADDITION = 1024 * 1024; // 1MB

static bool g_isRootVer = IsRootVersion();

std::vector<std::string> FilterLoopTraceResult(const std::vector<TraceFileInfo>& looptraceFiles)
{
    std::vector<std::string> outputFiles = {};
    for (const auto& output : looptraceFiles) {
        if (output.isNewFile) {
            outputFiles.emplace_back(output.filename);
        }
    }
    return outputFiles;
}
}

TraceDumpExecutor::TraceDumpExecutor()
{
    if (!IsTraceMounted(tracefsDir_)) {
        HILOG_ERROR(LOG_CORE, "TraceDumpExecutor: Trace is not mounted.");
    }
}

TraceDumpExecutor::~TraceDumpExecutor() {}

bool TraceDumpExecutor::PreCheckDumpTraceLoopStatus()
{
    // 通过状态机预先切换判断是否可以开始trace循环采集，避免采集线程重复切换状态
    return TraceDumpState::GetInstance().StartLoopDump();
}

bool TraceDumpExecutor::StartDumpTraceLoop(const TraceDumpParam& param)
{
    {
        std::lock_guard<std::mutex> lck(traceFileMutex_);
        loopTraceFiles_.clear();
        GetTraceFilesInDir(loopTraceFiles_, param.type);
        FileAgeingUtils::HandleAgeing(loopTraceFiles_, param.type);
    }

    if (param.fileSize == 0 && g_isRootVer) {
        std::string traceFile = param.outputFile.empty() ? GenerateTraceFileName(param.type) : param.outputFile;
        if (DoDumpTraceLoop(param, traceFile, false)) {
            std::lock_guard<std::mutex> lck(traceFileMutex_);
            loopTraceFiles_.emplace_back(traceFile);
        }
        TraceDumpState::GetInstance().EndLoopDumpSelf();
        return true;
    }

    while (TraceDumpState::GetInstance().IsLoopDumpRunning()) {
        {
            std::lock_guard<std::mutex> lck(traceFileMutex_);
            FileAgeingUtils::HandleAgeing(loopTraceFiles_, param.type);
        }
        std::string traceFile = GenerateTraceFileName(param.type);
        if (DoDumpTraceLoop(param, traceFile, true)) {
            std::lock_guard<std::mutex> lck(traceFileMutex_);
            loopTraceFiles_.emplace_back(traceFile);
        } else {
            break;
        }
    }
    TraceDumpState::GetInstance().EndLoopDumpSelf();
    return true;
}

std::vector<std::string> TraceDumpExecutor::StopDumpTraceLoop()
{
    TraceDumpState::GetInstance().EndLoopDump();
    std::lock_guard<std::mutex> lck(traceFileMutex_);
    return FilterLoopTraceResult(loopTraceFiles_);
}

bool TraceDumpExecutor::StartCacheTraceLoop(const TraceDumpParam& param)
{
    while (TraceDumpState::GetInstance().IsLoopDumpRunning()) {
        auto traceFile = GenerateTraceFileName(param.type);
        if (DoDumpTraceLoop(param, traceFile, true)) {
            std::lock_guard<std::mutex> cacheLock(traceFileMutex_);
            ClearCacheTraceFileBySize(cacheTraceFiles_, param.cacheTotalFileSizeLmt);
            HILOG_INFO(LOG_CORE, "ProcessCacheTask: save cache file.");
        } else {
            break;
        }
    }
    TraceDumpState::GetInstance().EndLoopDumpSelf();
    return true;
}

void TraceDumpExecutor::StopCacheTraceLoop()
{
    TraceDumpState::GetInstance().EndLoopDump();
}

TraceDumpRet TraceDumpExecutor::DumpTrace(const TraceDumpParam& param)
{
    MarkClockSync(tracefsDir_);
    const std::string traceFile = GenerateTraceFileName(param.type);
    return DumpTraceInner(param, traceFile);
}

std::vector<TraceFileInfo> TraceDumpExecutor::GetCacheTraceFiles()
{
    if (!TraceDumpState::GetInstance().InterruptCache()) {
        HILOG_WARN(LOG_CORE, "GetCacheTraceFiles: Cache trace loop is not running.");
    }
    std::vector<TraceFileInfo> cacheFiles;
    {
        std::lock_guard<std::mutex> lock(traceFileMutex_);
        cacheFiles = cacheTraceFiles_;
    }
    if (!TraceDumpState::GetInstance().ContinueCache()) {
        HILOG_WARN(LOG_CORE, "GetCacheTraceFiles: failed to continue cache trace loop.");
    }
    return cacheFiles;
}

void TraceDumpExecutor::ReadRawTraceLoop()
{
    const std::string threadName = "ReadRawTraceLoop";
    prctl(PR_SET_NAME, threadName.c_str());
    HILOG_INFO(LOG_CORE, "ReadRawTraceLoop start.");
    while (TraceDumpState::GetInstance().IsAsyncReadContinue()) {
        TraceDumpTask currentTask;
        bool hasTask = false;
        {
            std::unique_lock<std::mutex> lck(taskQueueMutex_);
            readCondVar_.wait(lck, [this]() {
                return !TraceDumpState::GetInstance().IsAsyncReadContinue() ||
                    std::any_of(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(), [](const TraceDumpTask& task) {
                        return task.status == TraceDumpStatus::START && task.code == TraceErrorCode::UNSET;
                    });
            });
            if (!TraceDumpState::GetInstance().IsAsyncReadContinue()) {
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
                std::lock_guard<std::mutex> lck(taskQueueMutex_);
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
    while (TraceDumpState::GetInstance().IsAsyncWriteContinue()) {
        TraceDumpTask currentTask;
        bool hasTask = false;
        {
            std::unique_lock<std::mutex> lck(taskQueueMutex_);
            writeCondVar_.wait(lck, [this]() {
                return !TraceDumpState::GetInstance().IsAsyncWriteContinue() ||
                    std::any_of(traceDumpTaskVec_.begin(), traceDumpTaskVec_.end(), [](const TraceDumpTask& task) {
                        return task.status == TraceDumpStatus::READ_DONE || task.status == TraceDumpStatus::WAIT_WRITE;
                    });
            });
            if (!TraceDumpState::GetInstance().IsAsyncWriteContinue()) {
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
    if (dumpPipe->ReadTraceTask(READ_TIMEOUT_MS, newTask)) {
        std::lock_guard<std::mutex> lck(taskQueueMutex_);
        traceDumpTaskVec_.push_back(newTask);
        readCondVar_.notify_one();
        sleepCnt = 0;
    }
}

void TraceDumpExecutor::DoProcessTraceDumpTask(std::shared_ptr<HitraceDumpPipe>& dumpPipe, TraceDumpTask& task,
    std::vector<TraceDumpTask>& completedTasks)
{
    if (task.status == TraceDumpStatus::WRITE_DONE) {
        if (task.hasSyncReturn && dumpPipe->WriteAsyncReturn(task)) {
            completedTasks.push_back(task);
        } else if (!task.hasSyncReturn && dumpPipe->WriteSyncReturn(task)) {
            task.hasSyncReturn = true;
        }
    } else if (task.status == TraceDumpStatus::READ_DONE) {
        if (task.code != TraceErrorCode::SUCCESS) { // read trace exception
            if (!task.hasSyncReturn && dumpPipe->WriteSyncReturn(task)) {
                task.hasSyncReturn = true;
            } else if (task.hasSyncReturn && dumpPipe->WriteAsyncReturn(task)) {
                completedTasks.push_back(task);
            }
        } else if (!task.hasSyncReturn) { // sync return
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
    int loopCnt = 0;
    do {
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
                loopCnt = 0;
            }
        }
        // Check for new tasks
        ProcessNewTask(dumpPipe, loopCnt);
        sleep(1); // sleep 1s to wait for new tasks
        loopCnt++;
    } while (loopCnt < MONITOR_EMPTY_LOOP_COUNT || !IsTraceDumpTaskEmpty());
    HILOG_INFO(LOG_CORE, "TraceDumpTaskMonitor : no task, dump process exit.");
    TraceDumpState::GetInstance().EndAsyncReadWrite();
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    readCondVar_.notify_all();
    writeCondVar_.notify_all();
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

void TraceDumpExecutor::ClearTraceDumpTask()
{
    std::lock_guard<std::mutex> lck(taskQueueMutex_);
    traceDumpTaskVec_.clear();
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
    cacheTraceFiles_.clear();
}
#endif

TraceDumpRet TraceDumpExecutor::ExecuteDumpTrace(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request)
{
    auto strategy = TraceStrategyFactory::GetInstance().Create(request.type);
    if (strategy == nullptr) {
        HILOG_ERROR(LOG_CORE, "ExecuteDumpTrace : Unknown trace dump type.");
        return { TraceErrorCode::UNKNOWN_TRACE_DUMP_TYPE, "", 0, 0, 0 };
    }
    return strategy->Execute(traceSourceFactory, request);
}

bool TraceDumpExecutor::DoDumpTraceLoop(const TraceDumpParam& param, std::string& traceFile, bool isLimited)
{
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace : Trace fs path is empty.");
        return false;
    }
    MarkClockSync(tracefsDir_);
    int fileSizeThreshold = param.type == TraceDumpType::TRACE_CACHE ?
        DEFAULT_CACHE_FILE_SIZE * BYTE_PER_KB : DEFAULT_FILE_SIZE * BYTE_PER_KB;
    if (param.fileSize != 0) {
        fileSizeThreshold = param.fileSize * BYTE_PER_KB;
    }
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(tracefsDir_, traceFile);
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(tracefsDir_, traceFile);
    }

    std::lock_guard<std::mutex> lck(traceFileMutex_);
    TraceDumpRequest request = {
        .type = param.type,
        .fileSize = fileSizeThreshold,
        .limitFileSz = isLimited,
        .traceStartTime = param.traceStartTime,
        .traceEndTime = param.traceEndTime,
        .cacheSliceDuration = param.cacheSliceDuration
    };
    auto dumpRet = ExecuteDumpTrace(traceSourceFactory, request);
    HILOG_INFO(LOG_CORE, "DoDumpTraceLoop: ExecuteDumpTrace done, errorcode: %{public}d, tracefile: %{public}s",
        static_cast<uint8_t>(dumpRet.code), dumpRet.outputFile);
    if (dumpRet.code != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Execute loop trace dump failed.");
        return false;
    }
    traceFile = traceSourceFactory->GetTraceFilePath();
    if (param.type == TraceDumpType::TRACE_CACHE) {
        TraceFileInfo traceFileInfo;
        if (!SetFileInfo(true, traceFile, dumpRet.traceStartTime, dumpRet.traceEndTime, traceFileInfo)) {
            RemoveFile(traceFile);
            return false;
        }
        traceFile = traceFileInfo.filename;
        cacheTraceFiles_.emplace_back(traceFileInfo);
    }
    if (access(traceFile.c_str(), F_OK) == -1) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Trace file (%{public}s) not found.", traceFile.c_str());
        return false;
    }
    return true;
}

TraceDumpRet TraceDumpExecutor::DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DumpTrace : Trace fs path is empty.");
        return { TraceErrorCode::TRACE_NOT_SUPPORTED, "", 0, 0, 0 };
    }
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(tracefsDir_, traceFile);
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(tracefsDir_, traceFile);
    }

    TraceDumpRequest request = {
        .type = param.type,
        .traceStartTime = param.traceStartTime,
        .traceEndTime = param.traceEndTime
    };
    return ExecuteDumpTrace(traceSourceFactory, request);
}

bool TraceDumpExecutor::DoReadRawTrace(TraceDumpTask& task)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DoReadRawTrace : Trace fs path is empty.");
        task.code = TraceErrorCode::TRACE_NOT_SUPPORTED;
        return false;
    }
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(tracefsDir_, "");
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(tracefsDir_, "");
    }

    TraceDumpRequest request = {
        .type = TraceDumpType::TRACE_ASYNC_READ,
        .traceStartTime = task.traceStartTime,
        .traceEndTime = task.traceEndTime,
        .taskId = task.time
    };
    auto ret = ExecuteDumpTrace(traceSourceFactory, request);
    task.code = ret.code;
    task.fileSize = ret.fileSize + ASYNC_DUMP_FILE_SIZE_ADDITION;
    if (strncpy_s(task.outputFile, TRACE_FILE_LEN, ret.outputFile, TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "DoReadRawTrace: strncpy_s failed.");
    }
    task.status = TraceDumpStatus::READ_DONE;
    if (task.code == TraceErrorCode::SUCCESS && task.fileSize > task.fileSizeLimit) {
        task.isFileSizeOverLimit = true;
    }
    if (!UpdateTraceDumpTask(task)) {
        HILOG_ERROR(LOG_CORE, "DoReadRawTrace: update trace dump task failed.");
        return false;
    }
    return task.code == TraceErrorCode::SUCCESS;
}

bool TraceDumpExecutor::DoWriteRawTrace(TraceDumpTask& task)
{
    std::shared_ptr<ITraceSourceFactory> traceSourceFactory = nullptr;
    if (tracefsDir_.empty()) {
        HILOG_ERROR(LOG_CORE, "DoWriteRawTrace : Trace fs path is empty.");
        task.code = TraceErrorCode::TRACE_NOT_SUPPORTED;
        return false;
    }
    if (IsHmKernel()) {
        traceSourceFactory = std::make_shared<TraceSourceHMFactory>(tracefsDir_, task.outputFile);
    } else {
        traceSourceFactory = std::make_shared<TraceSourceLinuxFactory>(tracefsDir_, task.outputFile);
    }

    TraceDumpRequest request = {
        .type = TraceDumpType::TRACE_ASYNC_WRITE,
        .traceStartTime = task.traceStartTime,
        .traceEndTime = task.traceEndTime,
        .taskId = task.time
    };
    auto ret = ExecuteDumpTrace(traceSourceFactory, request);
    task.code = ret.code;
    task.fileSize = static_cast<int64_t>(GetFileSize(std::string(task.outputFile)));
    task.status = TraceDumpStatus::WRITE_DONE;
    if (task.code == TraceErrorCode::SUCCESS && task.fileSize > task.fileSizeLimit) {
        task.isFileSizeOverLimit = true;
    }
    if (!UpdateTraceDumpTask(task)) {
        HILOG_ERROR(LOG_CORE, "DoWriteRawTrace: update trace dump task failed.");
        return false;
    }
    return task.code == TraceErrorCode::SUCCESS;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS