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

#ifndef TRACE_SOURCE_H
#define TRACE_SOURCE_H

#include <memory>

#include "trace_content.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
class ITraceSource {
public:
    ITraceSource() {}
    virtual ~ITraceSource() {}

    virtual std::shared_ptr<ITraceFileHdrContent> GetTraceFileHeader() = 0;
    virtual std::shared_ptr<TraceBaseInfoContent> GetTraceBaseInfo() = 0;
    virtual std::shared_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) = 0;
    virtual std::shared_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() = 0;
    virtual std::shared_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() = 0;
    virtual std::shared_ptr<TraceEventFmtContent> GetTraceEventFmt() = 0;
    virtual std::shared_ptr<TraceCmdLinesContent> GetTraceCmdLines() = 0;
    virtual std::shared_ptr<TraceTgidsContent> GetTraceTgids() = 0;
    virtual std::string GetTraceFilePath() = 0;
    virtual bool UpdateTraceFile(const std::string& traceFilePath) = 0;
};

class TraceSourceLinux : public ITraceSource {
public:
    TraceSourceLinux() = delete;
    TraceSourceLinux(const std::string& tracefsPath, const std::string& traceFilePath);
    ~TraceSourceLinux() override;

    std::shared_ptr<ITraceFileHdrContent> GetTraceFileHeader() override;
    std::shared_ptr<TraceBaseInfoContent> GetTraceBaseInfo() override;
    std::shared_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) override;
    std::shared_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() override;
    std::shared_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() override;
    std::shared_ptr<TraceEventFmtContent> GetTraceEventFmt() override;
    std::shared_ptr<TraceCmdLinesContent> GetTraceCmdLines() override;
    std::shared_ptr<TraceTgidsContent> GetTraceTgids() override;
    std::string GetTraceFilePath() override;
    bool UpdateTraceFile(const std::string& traceFilePath) override;

private:
    int traceFileFd_ = -1;
    std::string tracefsPath_;
    std::string traceFilePath_;
};

class TraceSourceHM : public ITraceSource {
public:
    TraceSourceHM() = delete;
    TraceSourceHM(const std::string& tracefsPath, const std::string& traceFilePath);
    ~TraceSourceHM() override;

    std::shared_ptr<ITraceFileHdrContent> GetTraceFileHeader() override;
    std::shared_ptr<TraceBaseInfoContent> GetTraceBaseInfo() override;
    std::shared_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) override;
    std::shared_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() override;
    std::shared_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() override;
    std::shared_ptr<TraceEventFmtContent> GetTraceEventFmt() override;
    std::shared_ptr<TraceCmdLinesContent> GetTraceCmdLines() override;
    std::shared_ptr<TraceTgidsContent> GetTraceTgids() override;
    std::string GetTraceFilePath() override;
    bool UpdateTraceFile(const std::string& traceFilePath) override;

private:
    int traceFileFd_ = -1;
    std::string tracefsPath_;
    std::string traceFilePath_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_SOURCE_H