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

#include "trace_dump_strategy.h"

#include <securec.h>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
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
#define LOG_TAG "HitraceDumpStrategy"
#endif

namespace {
constexpr int MAX_NEW_TRACE_FILE_LIMIT = 5;

bool IsGenerateNewFile(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpType traceType, int& count)
{
    if (access(traceSourceFactory->GetTraceFilePath().c_str(), F_OK) == 0) {
        return false;
    }
    if (count > MAX_NEW_TRACE_FILE_LIMIT) {
        HILOG_ERROR(LOG_CORE, "create new trace file limited.");
        return false;
    }
    count++;
    auto newFileName = GenerateTraceFileName(traceType);
    traceSourceFactory->UpdateTraceFile(newFileName);
    HILOG_INFO(LOG_CORE, "IsGenerateNewFile: update tracesource filename : %{public}s", newFileName.c_str());
    return true;
}

template<typename T>
void SafeWriteTraceContent(const std::unique_ptr<T>& component, const std::string& componentName)
{
    if (!component->WriteTraceContent()) {
        HILOG_INFO(LOG_CORE, "%s WriteTraceContent failed.", componentName.c_str());
    }
}

template<typename T, typename F>
bool SafeGetTraceContent(std::unique_ptr<T>& target, F&& getter, const std::string& componentName)
{
    target = std::forward<F>(getter)(); // perfect forwarding
    if (target == nullptr) {
        HILOG_ERROR(LOG_CORE, "CreateTraceContentPtr: %s failed.", componentName.c_str());
        return false;
    }
    return true;
}
} // namespace

TraceDumpRet ITraceDumpStrategy::Execute(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request)
{
    int newFileCount = 1;
    TraceDumpRet ret;
    do {
        if (!ProcessTraceDumpIteration(traceSourceFactory, request, ret, newFileCount)) {
            break;
        }
    } while (ShouldContinueWithNewFile(traceSourceFactory, request, newFileCount));

    return ret;
}

bool ITraceDumpStrategy::ProcessTraceDumpIteration(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, TraceDumpRet& ret, int& newFileCount)
{
    TraceContentPtr traceContentPtr;

    if (!InitializeTraceContent(traceSourceFactory, request, traceContentPtr)) {
        ret = {TraceErrorCode::WRITE_TRACE_INFO_ERROR, "", 0, 0, 0};
        return false;
    }

    ExecutePreProcessing(traceContentPtr);

    if (!DoCore(traceSourceFactory, request, traceContentPtr, ret)) {
        return HandleCoreFailure(traceSourceFactory, request, ret, newFileCount);
    }

    ExecutePostProcessing(traceContentPtr);
    return true;
}

bool ITraceDumpStrategy::InitializeTraceContent(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, TraceContentPtr& traceContentPtr)
{
    if (!NeedCreateTraceContentPtr()) {
        return true;
    }

    if (!CreateTraceContentPtr(traceSourceFactory, request, traceContentPtr)) {
        HILOG_ERROR(LOG_CORE, "ITraceDumpStrategy: CreateTraceContentPtr failed.");
        return false;
    }

    return true;
}

void ITraceDumpStrategy::ExecutePreProcessing(const TraceContentPtr& traceContentPtr)
{
    if (NeedDoPreAndPost()) {
        OnPre(traceContentPtr);
    }
}

void ITraceDumpStrategy::ExecutePostProcessing(const TraceContentPtr& traceContentPtr)
{
    if (NeedDoPreAndPost()) {
        OnPost(traceContentPtr);
    }
}

bool ITraceDumpStrategy::HandleCoreFailure(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, TraceDumpRet& ret, int& newFileCount)
{
    if (!NeedCheckFileExist()) {
        return false;
    }

    if (IsGenerateNewFile(traceSourceFactory, request.type, newFileCount)) {
        return true;
    }

    return false;
}

bool ITraceDumpStrategy::ShouldContinueWithNewFile(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, int& newFileCount)
{
    return NeedCheckFileExist() && IsGenerateNewFile(traceSourceFactory, request.type, newFileCount);
}

void ITraceDumpStrategy::OnPre(const TraceContentPtr& traceContentPtr)
{
    traceContentPtr.fileHdr->ResetCurrentFileSize();
    SafeWriteTraceContent(traceContentPtr.fileHdr, "fileHdr");
    SafeWriteTraceContent(traceContentPtr.baseInfo, "baseInfo");
    SafeWriteTraceContent(traceContentPtr.eventFmt, "eventFmt");
}

void ITraceDumpStrategy::OnPost(const TraceContentPtr& traceContentPtr)
{
    SafeWriteTraceContent(traceContentPtr.cmdLines, "cmdLines");
    SafeWriteTraceContent(traceContentPtr.tgids, "tgids");
    SafeWriteTraceContent(traceContentPtr.headerPage, "headerPage");
    SafeWriteTraceContent(traceContentPtr.printkFmt, "printkFmt");
}

bool ITraceDumpStrategy::CreateTraceContentPtr(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, TraceContentPtr& contentPtr)
{
    if (!SafeGetTraceContent(contentPtr.fileHdr,
        [&]() { return traceSourceFactory->GetTraceFileHeader(); }, "GetTraceFileHeader")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.baseInfo,
        [&]() { return traceSourceFactory->GetTraceBaseInfo(); }, "GetTraceBaseInfo")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.eventFmt,
        [&]() { return traceSourceFactory->GetTraceEventFmt(); }, "GetTraceEventFmt")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.cpuRaw,
        [&]() { return traceSourceFactory->GetTraceCpuRaw(request); }, "GetTraceCpuRaw")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.cmdLines,
        [&]() { return traceSourceFactory->GetTraceCmdLines(); }, "GetTraceCmdLines")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.tgids,
        [&]() { return traceSourceFactory->GetTraceTgids(); }, "GetTraceTgids")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.headerPage,
        [&]() { return traceSourceFactory->GetTraceHeaderPage(); }, "GetTraceHeaderPage")) {
        return false;
    }
    if (!SafeGetTraceContent(contentPtr.printkFmt,
        [&]() { return traceSourceFactory->GetTracePrintkFmt(); }, "GetTracePrintkFmt")) {
        return false;
    }
    return true;
}

bool SnapshotTraceDumpStrategy::DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, const TraceContentPtr& traceContentPtr, TraceDumpRet& ret)
{
    if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
        ret.code = traceContentPtr.cpuRaw->GetDumpStatus();
        return false;
    }
    auto traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
    ret.code = traceContentPtr.cpuRaw->GetDumpStatus();
    ret.traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
    ret.traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
    if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
        HILOG_ERROR(LOG_CORE, "SnapshotTraceDumpStrategy: strncpy_s failed.");
    }
    return true;
}

bool RecordTraceDumpStrategy::DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, const TraceContentPtr& traceContentPtr, TraceDumpRet& ret)
{
    while (TraceDumpState::GetInstance().IsLoopDumpRunning()) {
        sleep(1); // sleep 1s to wait for trace data.
        auto updatedRequest = request;
        updatedRequest.traceEndTime = GetCurBootTime();
        if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
            ret.code = traceContentPtr.cpuRaw->GetDumpStatus();
            return false;
        }
        if (traceContentPtr.cpuRaw->IsOverFlow()) {
            HILOG_INFO(LOG_CORE, "RecordTraceDumpStrategy: write trace content overflow.");
            break;
        }
        auto traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
        ret.code = traceContentPtr.cpuRaw->GetDumpStatus();
        ret.traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
        ret.traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
        if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
            HILOG_ERROR(LOG_CORE, "RecordTraceDumpStrategy: strncpy_s failed.");
            return false;
        }
    }
    return true;
}

bool CacheTraceDumpStrategy::DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
    const TraceDumpRequest& request, const TraceContentPtr& traceContentPtr, TraceDumpRet& ret)
{
    uint64_t sliceDuration = 0;
    while (TraceDumpState::GetInstance().IsLoopDumpRunning()) {
        struct timespec bts = {0, 0};
        clock_gettime(CLOCK_BOOTTIME, &bts);
        uint64_t startTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
        sleep(1); // sleep 1s to wait for trace data.
        if (!traceContentPtr.cpuRaw->WriteTraceContent()) {
            return false;
        }
        clock_gettime(CLOCK_BOOTTIME, &bts);
        uint64_t endTime = static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
        uint64_t timeDiff = (endTime - startTime) / S_TO_NS;
        auto traceFile = traceContentPtr.cpuRaw->GetTraceFilePath();
        ret.code = traceContentPtr.cpuRaw->GetDumpStatus();
        ret.traceStartTime = traceContentPtr.cpuRaw->GetFirstPageTimeStamp();
        ret.traceEndTime = traceContentPtr.cpuRaw->GetLastPageTimeStamp();
        if (strncpy_s(ret.outputFile, TRACE_FILE_LEN, traceFile.c_str(), TRACE_FILE_LEN - 1) != 0) {
            HILOG_ERROR(LOG_CORE, "CacheTraceDumpStrategy: strncpy_s failed.");
            return false;
        }
        sliceDuration += timeDiff;
        if (sliceDuration >= request.cacheSliceDuration || TraceDumpState::GetInstance().IsInterruptCache()) {
            sliceDuration = 0;
            break;
        }
    }
    return true;
}

// Register strategies
REGISTER_TRACE_STRATEGY(TRACE_SNAPSHOT, SnapshotTraceDumpStrategy);
REGISTER_TRACE_STRATEGY(TRACE_RECORDING, RecordTraceDumpStrategy);
REGISTER_TRACE_STRATEGY(TRACE_CACHE, CacheTraceDumpStrategy);
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS