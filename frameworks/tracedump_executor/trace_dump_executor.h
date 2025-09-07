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
#include "trace_dump_pipe.h"
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
    uint64_t cacheTotalFileSizeLmt = 0;
    uint64_t cacheSliceDuration = 30; // 30 : 30 seconds as default cache trace slice duration
};

class TraceDumpExecutor : public DelayedRefSingleton<TraceDumpExecutor> {
    DECLARE_DELAYED_REF_SINGLETON(TraceDumpExecutor);

public:
    bool PreCheckDumpTraceLoopStatus();
    bool StartDumpTraceLoop(const TraceDumpParam& param);
    std::vector<std::string> StopDumpTraceLoop();
    bool StartCacheTraceLoop(const TraceDumpParam& param);
    void StopCacheTraceLoop();
    TraceDumpRet DumpTrace(const TraceDumpParam& param);

    std::vector<TraceFileInfo> GetCacheTraceFiles();
    void ReadRawTraceLoop();
    void WriteTraceLoop();
    void TraceDumpTaskMonitor();

    void RemoveTraceDumpTask(const uint64_t time);
    bool UpdateTraceDumpTask(const TraceDumpTask& task);
    void AddTraceDumpTask(const TraceDumpTask& task);
    void ClearTraceDumpTask();
    bool IsTraceDumpTaskEmpty();
    size_t GetTraceDumpTaskCount();

#ifdef HITRACE_UNITTEST
    void ClearCacheTraceFiles();
#endif

private:
    TraceDumpRet ExecuteDumpTrace(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
        const TraceDumpRequest& request);
    bool DoDumpTraceLoop(const TraceDumpParam& param, std::string& traceFile, bool isLimited);
    TraceDumpRet DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile);
    bool DoReadRawTrace(TraceDumpTask& task);
    bool DoWriteRawTrace(TraceDumpTask& task);
    void DoProcessTraceDumpTask(std::shared_ptr<HitraceDumpPipe>& dumpPipe, TraceDumpTask& task,
        std::vector<TraceDumpTask>& completedTasks);
    void ProcessNewTask(std::shared_ptr<HitraceDumpPipe>& dumpPipe, int& sleepCnt);

    std::string tracefsDir_ = "";
    std::vector<TraceFileInfo> loopTraceFiles_ = {};
    std::vector<TraceFileInfo> cacheTraceFiles_ = {};
    std::vector<TraceDumpTask> traceDumpTaskVec_ = {};
    std::mutex traceFileMutex_;
    std::mutex taskQueueMutex_;
    std::condition_variable readCondVar_;
    std::condition_variable writeCondVar_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif