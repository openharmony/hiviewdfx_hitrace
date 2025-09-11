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

#ifndef TRACE_DUMP_STRATEGY_H
#define TRACE_DUMP_STRATEGY_H

#include <memory>

#include "hitrace_define.h"
#include "trace_source_factory.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
struct TraceDumpRet {
    TraceErrorCode code = TraceErrorCode::UNSET;
    char outputFile[TRACE_FILE_LEN] = { 0 };
    int64_t fileSize = 0;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
};

struct TraceContentPtr {
    std::unique_ptr<ITraceFileHdrContent> fileHdr;
    std::unique_ptr<TraceBaseInfoContent> baseInfo;
    std::unique_ptr<TraceEventFmtContent> eventFmt;
    std::unique_ptr<ITraceCpuRawContent> cpuRaw;
    std::unique_ptr<TraceCmdLinesContent> cmdLines;
    std::unique_ptr<TraceTgidsContent> tgids;
    std::unique_ptr<ITraceHeaderPageContent> headerPage;
    std::unique_ptr<ITracePrintkFmtContent> printkFmt;
};

class ITraceDumpStrategy {
public:
    virtual ~ITraceDumpStrategy() = default;
    // Template Method: unified dump flow. Subclasses implement DoCore and may override hooks.
    TraceDumpRet Execute(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request);
    bool CreateTraceContentPtr(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request,
        TraceContentPtr& contentPtr);

protected:
    // Core step implemented by concrete strategies.
    virtual bool DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request,
        const TraceContentPtr& contentPtr, TraceDumpRet& ret) = 0;
    // Hooks: default to common pre/post dump steps. Subclasses may override if needed.
    virtual void OnPre(const TraceContentPtr& contentPtr);
    virtual void OnPost(const TraceContentPtr& contentPtr);
    // Some strategies (e.g., async read) do not need common content preparation.
    virtual bool NeedDoPreAndPost() const { return true; }
    virtual bool NeedCreateTraceContentPtr() const { return true; }
    virtual bool NeedCheckFileExist() const { return false; }

private:
    bool ProcessTraceDumpIteration(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
        const TraceDumpRequest& request, TraceDumpRet& ret, int& newFileCount);
    bool InitializeTraceContent(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
        const TraceDumpRequest& request, TraceContentPtr& traceContentPtr);
    void ExecutePreProcessing(const TraceContentPtr& traceContentPtr);
    void ExecutePostProcessing(const TraceContentPtr& traceContentPtr);
    bool HandleCoreFailure(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
        const TraceDumpRequest& request, TraceDumpRet& ret, int& newFileCount);
    bool ShouldContinueWithNewFile(std::shared_ptr<ITraceSourceFactory> traceSourceFactory,
        const TraceDumpRequest& request, int& newFileCount);
};

class SnapshotTraceDumpStrategy : public ITraceDumpStrategy {
public:
    bool DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request,
        const TraceContentPtr& contentPtr, TraceDumpRet& ret) override;
};

class RecordTraceDumpStrategy : public ITraceDumpStrategy {
public:
    bool DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request,
        const TraceContentPtr& contentPtr, TraceDumpRet& ret) override;
    bool NeedCheckFileExist() const override { return true; }
};

class CacheTraceDumpStrategy : public ITraceDumpStrategy {
public:
    bool DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request,
        const TraceContentPtr& contentPtr, TraceDumpRet& ret) override;
    bool NeedCheckFileExist() const override { return true; }
};


class AsyncTraceReadStrategy : public ITraceDumpStrategy {
public:
    bool DoCore(std::shared_ptr<ITraceSourceFactory> tracetraceSourceFactoryource, const TraceDumpRequest& request,
        const TraceContentPtr& contentPtr, TraceDumpRet& ret) override;
    bool NeedCreateTraceContentPtr() const override { return false; }
    bool NeedDoPreAndPost() const override { return false; }
};

class AsyncTraceWriteStrategy : public ITraceDumpStrategy {
public:
    bool DoCore(std::shared_ptr<ITraceSourceFactory> traceSourceFactory, const TraceDumpRequest& request,
        const TraceContentPtr& contentPtr, TraceDumpRet& ret) override;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_DUMP_STRATEGY_H