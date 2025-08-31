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

#ifndef TRACE_DUMP_EXECUTOR_H
#define TRACE_DUMP_EXECUTOR_H

#include <mutex>
#include <string>
#include <vector>

#include "hitrace_define.h"
#include "singleton.h"
#include "trace_dump_strategy.h"
#include "trace_file_utils.h"
#include "trace_source_factory.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
struct TraceDumpParam {
    TraceDumpType type = TraceDumpType::TRACE_SNAPSHOT;
    std::string outputFile = "";
    int fileLimit = 0;
    int fileSize = 0;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = std::numeric_limits<uint64_t>::max();
};

class TraceDumpExecutor : public DelayedRefSingleton<TraceDumpExecutor> {
    DECLARE_DELAYED_REF_SINGLETON(TraceDumpExecutor);

public:
    bool PreCheckDumpTraceLoopStatus();
    bool StartDumpTraceLoop(const TraceDumpParam& param);
    std::vector<std::string> StopDumpTraceLoop();
    TraceDumpRet DumpTrace(const TraceDumpParam& param);

private:
    TraceDumpRet ExecuteDumpTrace(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
        const TraceDumpRequest& request);
    bool DoDumpTraceLoop(const TraceDumpParam& param, std::string& traceFile, bool isLimited);
    TraceDumpRet DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile);

    std::string tracefsDir_ = "";
    std::vector<TraceFileInfo> loopTraceFiles_ = {};
    std::mutex traceFileMutex_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif