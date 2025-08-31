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

TraceDumpRet TraceDumpExecutor::DumpTrace(const TraceDumpParam& param)
{
    MarkClockSync(tracefsDir_);
    const std::string traceFile = GenerateTraceFileName(param.type);
    return DumpTraceInner(param, traceFile);
}

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
    int fileSizeThreshold = DEFAULT_FILE_SIZE * BYTE_PER_KB;
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
        .traceEndTime = param.traceEndTime
    };
    auto dumpRet = ExecuteDumpTrace(traceSourceFactory, request);
    HILOG_INFO(LOG_CORE, "DoDumpTraceLoop: ExecuteDumpTrace done, errorcode: %{public}d, tracefile: %{public}s",
        static_cast<uint8_t>(dumpRet.code), dumpRet.outputFile);
    if (dumpRet.code != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "DoDumpTraceLoop : Execute loop trace dump failed.");
        return false;
    }
    traceFile = traceSourceFactory->GetTraceFilePath();
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
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS