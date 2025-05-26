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
constexpr int MAX_NEW_TRACE_FILE_LIMIT = 5;
#ifdef HITRACE_UNITTEST
constexpr int DEFAULT_CACHE_FILE_SIZE = 15 * 1024;
#else
constexpr int DEFAULT_CACHE_FILE_SIZE = 150 * 1024;
#endif

std::vector<FileWithTime> g_looptraceFiles;
std::shared_ptr<ProductConfigJsonParser> g_ProductConfigParser
    = std::make_shared<ProductConfigJsonParser>("/sys_prod/etc/hiview/hitrace/hitrace_param.json");
uint64_t g_sliceMaxDuration;

static bool g_isRootVer = IsRootVersion();

// use for control status of trace dump loop.
std::atomic<bool> g_isDumpRunning(false);
std::atomic<bool> g_isDumpEnded(true);
std::atomic<bool> g_interruptCache(false);

uint64_t GetCurBootTime()
{
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    return static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
}

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
    traceSource->UpdateTraceFile(GenerateTraceFileName(traceType));
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

TraceDumpRet RecordTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    struct TraceContentPtr traceContentPtr;
    if (!CreateTraceContentPtr(traceSource, request, traceContentPtr)) {
        HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: CreateTraceContentPtr failed.");
        return {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0};
    }
    int newFileCount = 1;
    TraceErrorCode code;
    std::string traceFile;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
    do {
        PreTraceContentDump(traceContentPtr);
        while (g_isDumpRunning.load()) {
            sleep(1);
            auto updatedRequest = request;
            updatedRequest.traceEndTime = GetCurBootTime();
            traceContentPtr.cpuRaw = traceSource->GetTraceCpuRaw(updatedRequest);
            if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
                HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: write raw trace content failed.");
                break;
            }
            if (traceContentPtr.cpuRaw->IsOverFlow()) {
                HILOG_INFO(LOG_CORE, "RecordTraceDumpStrategy: write trace content overflow.");
                break;
            }
            code = traceContentPtr.cpuRaw->GetDumpStatus();
            traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
            traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
            traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
        }
        AfterTraceContentDump(traceContentPtr);
    } while (IsGenerateNewFile(traceSource, TRACE_TYPE::TRACE_RECORDING, newFileCount));

    TraceDumpRet ret = {
        .code = code,
        .traceStartTime = traceStartTime,
        .traceEndTime = traceEndTime,
    };
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: strncpy_s failed.");
    }
    return ret;
}

TraceDumpRet CacheTraceDumpStrategy::Execute(std::shared_ptr<ITraceSource> traceSource,
    const TraceDumpRequest& request)
{
    struct TraceContentPtr traceContentPtr;
    if (!CreateTraceContentPtr(traceSource, request, traceContentPtr)) {
        return {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0};
    }
    int newFileCount = 1;
    TraceErrorCode code;
    std::string traceFile;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
    uint64_t sliceDuration = 0;
    do {
        PreTraceContentDump(traceContentPtr);
        while (g_isDumpRunning.load()) {
            struct timespec bts = {0, 0};
            clock_gettime(CLOCK_BOOTTIME, &bts);
            uint64_t startTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
            sleep(1);
            if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
                HILOG_ERROR(LOG_CORE, "CacheTraceDumpStrategy: write raw trace content failed.");
                break;
            }
            clock_gettime(CLOCK_BOOTTIME, &bts);
            uint64_t endTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
            uint64_t timeDiff = (endTime - startTime) / S_TO_NS;
            code = traceContentPtr.cpuRaw->GetDumpStatus();
            traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
            traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
            traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
            sliceDuration += timeDiff;
            if (sliceDuration >= g_sliceMaxDuration || g_interruptCache.load()) {
                sliceDuration = 0;
                break;
            }
        }
        AfterTraceContentDump(traceContentPtr);
    } while (IsGenerateNewFile(traceSource, TRACE_TYPE::TRACE_CACHE, newFileCount));
    TraceDumpRet ret = {
        .code = code,
        .traceStartTime = traceStartTime,
        .traceEndTime = traceEndTime,
    };
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "CacheTraceDumpStrategy: strncpy_s failed.");
    }
    return ret;
}

TraceDumpExecutor::TraceDumpExecutor()
{
    if (!IsTraceMounted(tracefsDir_)) {
        HILOG_ERROR(LOG_CORE, "TraceDumpExecutor: Trace is not mounted.");
    }
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

    g_isDumpEnded.store(false);
    g_isDumpRunning.store(true);
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

#ifdef HITRACE_UNITTEST
void TraceDumpExecutor::ClearCacheTraceFiles()
{
    std::lock_guard<std::mutex> lock(traceFileMutex_);
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

bool TraceDumpExecutor::DoDumpTraceLoop(const TraceDumpParam& param, const std::string& traceFile, bool isLimited)
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
    if (dumpRet.code != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Execute loop trace dump failed.");
        return false;
    }

    auto tmpfilename = traceFile;
    if (param.type == TRACE_TYPE::TRACE_CACHE) {
        TraceFileInfo traceFileInfo;
        if (!SetFileInfo(tmpfilename, dumpRet.traceStartTime, dumpRet.traceEndTime, traceFileInfo)) {
            RemoveFile(traceFile);
            return false;
        }
        tmpfilename = traceFileInfo.filename;
        traceFiles_.emplace_back(traceFileInfo);
    }
    if (access(tmpfilename.c_str(), F_OK) == -1) {
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
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS