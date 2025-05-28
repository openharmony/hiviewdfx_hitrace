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
#include <condition_variable>

#include "hitrace_define.h"
#include "trace_file_utils.h"
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
    char outputFile[TRACE_FILE_LEN] = { 0 };
    size_t fileSize = 0;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
};

struct TraceContentPtr {
    std::shared_ptr<ITraceFileHdrContent> fileHdr;
    std::shared_ptr<TraceBaseInfoContent> baseInfo;
    std::shared_ptr<TraceEventFmtContent> eventFmt;
    std::shared_ptr<ITraceCpuRawContent> cpuRaw;
    std::shared_ptr<TraceCmdLinesContent> cmdLines;
    std::shared_ptr<TraceTgidsContent> tgids;
    std::shared_ptr<ITraceHeaderPageContent> headerPage;
    std::shared_ptr<ITracePrintkFmtContent> printkFmt;
};

class ITraceDumpStrategy {
public:
    virtual ~ITraceDumpStrategy() = default;
    virtual TraceDumpRet Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) = 0;
    bool CreateTraceContentPtr(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request,
        TraceContentPtr& contentPtr);
};

class SnapshotTraceDumpStrategy : public ITraceDumpStrategy {
public:
    TraceDumpRet Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class RecordTraceDumpStrategy : public ITraceDumpStrategy {
public:
    TraceDumpRet Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class CacheTraceDumpStrategy : public ITraceDumpStrategy {
public:
    TraceDumpRet Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class AsyncTraceReadStrategy : public ITraceDumpStrategy {
public:
    TraceDumpRet Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class AsyncTraceWriteStrategy : public ITraceDumpStrategy {
public:
    TraceDumpRet Execute(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request) override;
};

class TraceDumpExecutor final {
public:
    TraceDumpExecutor();
    ~TraceDumpExecutor() = default;

    static TraceDumpExecutor& GetInstance()
    {
        static TraceDumpExecutor instance;
        return instance;
    }

    bool PreCheckDumpTraceLoopStatus();
    // attention: StartXXXTraceLoop and StopXXXTraceLoop should be called in different threads.
    bool StartDumpTraceLoop(const TraceDumpParam& param);
    std::vector<std::string> StopDumpTraceLoop();
    bool StartCacheTraceLoop(const TraceDumpParam& param, uint64_t totalFileSize, uint64_t sliceMaxDuration);
    void StopCacheTraceLoop();
    TraceDumpRet DumpTrace(const TraceDumpParam& param);

    std::vector<TraceFileInfo> GetCacheTraceFiles();

    // attention: ReadRawTraceLoop, WriteTraceLoop and TraceDumpTaskMonitor should be called in different threads.
    void ReadRawTraceLoop();
    void WriteTraceLoop();
    void TraceDumpTaskMonitor();

    // bool GetTraceDumpTaskByTime(const uint64_t time, TraceDumpTask& task);
    void RemoveTraceDumpTask(const uint64_t time);
    bool UpdateTraceDumpTask(const TraceDumpTask& task);
    void AddTraceDumpTask(const TraceDumpTask& task);
    bool IsTraceDumpTaskEmpty();

#ifdef HITRACE_UNITTEST
    void ClearCacheTraceFiles();
#endif

    TraceDumpExecutor(const TraceDumpExecutor&) = delete;
    TraceDumpExecutor(TraceDumpExecutor&&) = delete;
    TraceDumpExecutor& operator=(const TraceDumpExecutor&) = delete;
    TraceDumpExecutor& operator=(TraceDumpExecutor&&) = delete;

private:
    void SetTraceDumpStrategy(std::unique_ptr<ITraceDumpStrategy> strategy);
    TraceDumpRet ExecuteDumpTrace(std::shared_ptr<ITraceSource> traceSource, const TraceDumpRequest& request);
    bool DoDumpTraceLoop(const TraceDumpParam& param, std::string& traceFile, bool isLimited);
    TraceDumpRet DumpTraceInner(const TraceDumpParam& param, const std::string& traceFile);
    bool DoReadRawTrace(TraceDumpTask& task);
    bool DoWriteRawTrace(TraceDumpTask& task);

    std::string tracefsDir_ = "";
    std::vector<TraceFileInfo> traceFiles_; // for cache
    std::vector<TraceDumpTask> traceDumpTaskVec_;
    std::mutex traceFileMutex_;
    std::mutex taskQueueMutex_;
    std::condition_variable readCondVar_;
    std::condition_variable writeCondVar_;
    std::unique_ptr<ITraceDumpStrategy> dumpStrategy_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif