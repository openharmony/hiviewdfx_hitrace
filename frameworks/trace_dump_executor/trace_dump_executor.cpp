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
constexpr uint64_t S_TO_NS = 1000000000;

std::vector<FileWithTime> g_looptraceFiles;
std::shared_ptr<ProductConfigJsonParser> g_ProductConfigParser
    = std::make_shared<ProductConfigJsonParser>("/sys_prod/etc/hiview/hitrace/hitrace_param.json");

static bool g_isRootVer = IsRootVersion();

// use for control status of trace dump loop.
std::atomic<bool> g_isDumpRunning(false);
std::atomic<bool> g_isDumpEnded(true);

uint64_t GetCurBootTime()
{
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    return static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
}

void FilterLoopTraceResult(const std::vector<FileWithTime>& looptraceFiles, std::vector<std::string>& outputFiles)
{
    outputFiles.clear();
    for (const auto& output : looptraceFiles) {
        if (output.isNewFile) {
            outputFiles.emplace_back(output.filename);
        }
    }
}
}

bool SingleTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request)
{
    // 初始化组件写入
    traceSource->GetTraceFileHeader()->WriteTraceContent();
    traceSource->GetTraceBaseInfo()->WriteTraceContent();
    traceSource->GetTraceEventFmt()->WriteTraceContent();

    // 核心数据写入
    auto traceCpuRaw = traceSource->GetTraceCpuRaw(request);
    if (!traceCpuRaw->WriteTraceContent()) {
        return false;
    }

    // 收尾组件写入
    traceSource->GetTraceCmdLines()->WriteTraceContent();
    traceSource->GetTraceTgids()->WriteTraceContent();
    traceSource->GetTraceHeaderPage()->WriteTraceContent();
    traceSource->GetTracePrintkFmt()->WriteTraceContent();

    return true;
}

bool LoopTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request)
{
    // 初始化组件写入
    traceSource->GetTraceFileHeader()->WriteTraceContent();
    traceSource->GetTraceBaseInfo()->WriteTraceContent();
    traceSource->GetTraceEventFmt()->WriteTraceContent();

    // 循环写入核心数据
    while (g_isDumpRunning.load()) {
        sleep(1);
        auto updatedRequest = request;
        updatedRequest.traceEndTime = GetCurBootTime();
        auto traceCpuRaw = traceSource->GetTraceCpuRaw(updatedRequest);
        if (!traceCpuRaw->WriteTraceContent()) {
            return false;
        }
    }

    // 收尾组件写入
    traceSource->GetTraceCmdLines()->WriteTraceContent();
    traceSource->GetTraceTgids()->WriteTraceContent();
    traceSource->GetTraceHeaderPage()->WriteTraceContent();
    traceSource->GetTracePrintkFmt()->WriteTraceContent();

    return true;
}

bool TraceDumpExecutor::StartDumpTraceLoop(const TraceDumpParam& param)
{
    if (!g_isDumpEnded.load()) {
        HILOG_WARN(LOG_CORE, "Trace dump loop is already running");
        return false; // Already stopped
    }

    g_isDumpEnded.store(false);
    g_isDumpRunning.store(true);
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
        if (!g_isRootVer) {
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
    FilterLoopTraceResult(g_looptraceFiles, traceFiles_);
    return traceFiles_;
}

std::string TraceDumpExecutor::DumpTrace(const TraceDumpParam& param)
{
    const std::string traceFile = GenerateTraceFileName(param.type);
    if (!DumpTraceInner(param, traceFile)) {
        return "";
    }
    return traceFile;
}

TraceDumpRet TraceDumpExecutor::DumpTraceAsync(const TraceDumpParam& param, std::function<void(bool)> callback,
    const int timeout)
{
    std::shared_ptr<AsyncTraceDumpContext> context = std::make_shared<AsyncTraceDumpContext>();
    const std::string traceFile = GenerateTraceFileName(param.type);
    std::thread worker([this, param, traceFile, context, callback] {
#ifdef HITRACE_UNITTEST
        sleep(3); // 3 : sleep 3 seconds for unittest
#endif
        bool ret = DumpTraceInner(param, traceFile);

        std::unique_lock<std::mutex> lck(context->mutex);
        if (context->timed_out) {
            lck.unlock();
            callback(ret);
        } else {
            context->completed = true;
            context->result = ret;
            context->cv.notify_one();
        }
    });
    {
        std::unique_lock<std::mutex> lck(context->mutex);
        if (!context->cv.wait_for(lck, std::chrono::seconds(timeout),
            [&context] { return context->completed; })) {
            context->timed_out = true;
            worker.detach();
            return {TraceErrorCode::ASYNC_DUMP, traceFile};
        } else {
            worker.join();
            return {TraceErrorCode::SUCCESS, traceFile};
        }
    }
}

void TraceDumpExecutor::SetTraceDumpStrategy(std::unique_ptr<ITraceDumpStrategy> strategy)
{
    dumpStrategy_ = std::move(strategy);
}

bool TraceDumpExecutor::ExecuteDumpTrace(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request)
{
    if (dumpStrategy_ == nullptr) {
        HILOG_ERROR(LOG_CORE, "ExecuteDumpTrace : No write strategy set!");
        return false;
    }
    return dumpStrategy_->Execute(traceSource, request);
}

// todo: tracefspath chose
bool TraceDumpExecutor::DoDumpTraceLoop(const TraceDumpParam& param, const std::string& traceFile, bool isLimited)
{
    int fileSizeThreshold = DEFAULT_FILE_SIZE * BYTE_PER_KB;
    if (param.fileSize != 0) {
        fileSizeThreshold = param.fileSize * BYTE_PER_KB;
    }
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, traceFile); // todo : tracefs path
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, traceFile);
    }

    TraceDumpRequest request = {
        param.type,
        fileSizeThreshold,
        isLimited,
        param.traceStartTime,
        param.traceEndTime
    };

    SetTraceDumpStrategy(std::make_unique<LoopTraceDumpStrategy>());
    if (!ExecuteDumpTrace(traceSource, request)) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Execute loop trace dump failed.");
        return false;
    }

    if (access(traceFile.c_str(), F_OK) == -1) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Trace file (%{public}s) not found.", traceFile.c_str());
        return false;
    }
    return true;
}

bool TraceDumpExecutor::DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile)
{
    std::shared_ptr<ITraceSource> traceSource = nullptr;
    if (IsHmKernel()) {
        traceSource = std::make_shared<TraceSourceHM>(TRACEFS_DIR, traceFile); // todo : tracefs path
    } else {
        traceSource = std::make_shared<TraceSourceLinux>(TRACEFS_DIR, traceFile);
    }

    TraceDumpRequest request = {
        param.type,
        0,
        false,
        param.traceStartTime,
        param.traceEndTime
    };

    SetTraceDumpStrategy(std::make_unique<SingleTraceDumpStrategy>());
    if (!ExecuteDumpTrace(traceSource, request)) {
        HILOG_ERROR(LOG_CORE, "DumpTrace : Execute single trace dump failed.");
        return false;
    }
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS