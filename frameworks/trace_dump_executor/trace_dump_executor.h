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

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "trace_source.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
struct TraceDumpParam {
    TRACE_TYPE type;
    std::string outputFile;
    int fileLimit;
    int fileSize;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
};

struct TraceDumpRet {
    TraceErrorCode code = TraceErrorCode::UNSET;
    std::string outputFile = "";
};

enum class TraceDumpStatus {
    START = 0,
    READ_DONE,
    WRITE_DONE,
    FINISH
};

struct TraceDumpTask {
    uint64_t time = 0;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
    int bufferIdx = -1;
    std::string outputFile = "";
    TraceDumpStatus status = TraceDumpStatus::START;
};

struct AsyncTraceDumpContext {
    std::mutex mutex;
    std::condition_variable cv;
    bool completed = false;
    bool timed_out = false;
    bool result = false;
};

class ITraceDumpStrategy {
public:
    virtual ~ITraceDumpStrategy() = default;
    virtual bool Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) = 0;
};

class SingleTraceDumpStrategy : public ITraceDumpStrategy {
public:
    bool Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class LoopTraceDumpStrategy : public ITraceDumpStrategy {
public:
    bool Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class TraceDumpExecutor final {
public:
    TraceDumpExecutor() = default;
    ~TraceDumpExecutor() = default;

    static TraceDumpExecutor& GetInstance()
    {
        static TraceDumpExecutor instance;
        return instance;
    }

    // attention: StartDumpTraceLoop and StopDumpTraceLoop should be called in different threads.
    bool StartDumpTraceLoop(const TraceDumpParam& param);
    std::vector<std::string> StopDumpTraceLoop();

    std::string DumpTrace(const TraceDumpParam& param);
    TraceDumpRet DumpTraceAsync(const TraceDumpParam& param, std::function<void(bool)> callback,
        const int timeout = 5); // 5 : 5 seconds


    void ReadRawTraceLoop() {}
    void WriteTraceLoop() {}

    TraceDumpExecutor(const TraceDumpExecutor&) = delete;
    TraceDumpExecutor(TraceDumpExecutor&&) = delete;
    TraceDumpExecutor& operator=(const TraceDumpExecutor&) = delete;
    TraceDumpExecutor& operator=(TraceDumpExecutor&&) = delete;

private:
    void SetTraceDumpStrategy(std::unique_ptr<ITraceDumpStrategy> strategy);
    bool ExecuteDumpTrace(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request);
    bool DoDumpTraceLoop(const TraceDumpParam& param, const std::string& traceFile, bool isLimited);
    bool DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile);

    std::vector<std::string> traceFiles_;
    std::queue<TraceDumpTask> traceDumpTasks_;
    std::mutex traceFileMutex_;
    std::mutex traceDumpTaskMutex_;
    std::unique_ptr<ITraceDumpStrategy> dumpStrategy_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif