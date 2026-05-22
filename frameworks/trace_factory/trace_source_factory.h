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

#ifndef TRACE_SOURCE_FACTORY_H
#define TRACE_SOURCE_FACTORY_H

#include <memory>
#include <string>

#include "trace_content.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
/**
 * @brief ITraceSourceFactory is the interface for trace source factory.
 * @note The trace source factory is responsible for providing the trace data to the trace dump executor.
 */
class ITraceSourceFactory {
public:
    explicit ITraceSourceFactory(const std::string& traceFilePath);
    virtual ~ITraceSourceFactory() = default;
    virtual std::unique_ptr<ITraceFileHdrContent> GetTraceFileHeader() = 0;
    virtual std::unique_ptr<TraceBaseInfoContent> GetTraceBaseInfo() = 0;
    virtual std::unique_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) = 0;
    virtual std::unique_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() = 0;
    virtual std::unique_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() = 0;
    virtual std::unique_ptr<TraceEventFmtContent> GetTraceEventFmt() = 0;
    virtual std::unique_ptr<TraceCmdLinesContent> GetTraceCmdLines() = 0;
    virtual std::unique_ptr<TraceTgidsContent> GetTraceTgids() = 0;
    virtual std::unique_ptr<ITraceCpuRawRead> GetTraceCpuRawRead(const TraceDumpRequest& request) = 0;
    virtual std::unique_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(const uint64_t taskId) = 0;
    virtual const std::string& GetTraceFilePath();
    virtual bool UpdateTraceFile(const std::string& traceFilePath);
protected:
    SmartFd traceFileFd_;
    std::string traceFilePath_;
};

class TraceSourceLinuxFactory : public ITraceSourceFactory {
public:
    TraceSourceLinuxFactory() = delete;
    explicit TraceSourceLinuxFactory(const std::string& traceFilePath);
    ~TraceSourceLinuxFactory() override = default;

    std::unique_ptr<ITraceFileHdrContent> GetTraceFileHeader() override;
    std::unique_ptr<TraceBaseInfoContent> GetTraceBaseInfo() override;
    std::unique_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) override;
    std::unique_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() override;
    std::unique_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() override;
    std::unique_ptr<TraceEventFmtContent> GetTraceEventFmt() override;
    std::unique_ptr<TraceCmdLinesContent> GetTraceCmdLines() override;
    std::unique_ptr<TraceTgidsContent> GetTraceTgids() override;
    std::unique_ptr<ITraceCpuRawRead> GetTraceCpuRawRead(const TraceDumpRequest& request) override;
    std::unique_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(const uint64_t taskId) override;
};

class TraceSourceHMFactory : public ITraceSourceFactory {
public:
    TraceSourceHMFactory() = delete;
    explicit TraceSourceHMFactory(const std::string& traceFilePath);
    ~TraceSourceHMFactory() override = default;

    std::unique_ptr<ITraceFileHdrContent> GetTraceFileHeader() override;
    std::unique_ptr<TraceBaseInfoContent> GetTraceBaseInfo() override;
    std::unique_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) override;
    std::unique_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() override;
    std::unique_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() override;
    std::unique_ptr<TraceEventFmtContent> GetTraceEventFmt() override;
    std::unique_ptr<TraceCmdLinesContent> GetTraceCmdLines() override;
    std::unique_ptr<TraceTgidsContent> GetTraceTgids() override;
    std::unique_ptr<ITraceCpuRawRead> GetTraceCpuRawRead(const TraceDumpRequest& request) override;
    std::unique_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(const uint64_t taskId) override;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_SOURCE_FACTORY_H