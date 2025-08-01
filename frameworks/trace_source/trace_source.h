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
#include <string>

#include "trace_content.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
/**
 * @brief ITraceSource is the interface for trace source.
 * @note The trace source is responsible for providing the trace data to the trace dump executor.
 */
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
    virtual std::shared_ptr<ITraceCpuRawRead> GetTraceCpuRawRead(const TraceDumpRequest& request) = 0;
    virtual std::shared_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(const uint64_t taskId) = 0;
    virtual std::string GetTraceFilePath() = 0;
    virtual bool UpdateTraceFile(const std::string& traceFilePath) = 0;
};

struct LinuxTraceSourceStrategy {
    static std::shared_ptr<ITraceFileHdrContent> CreateFileHdr(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceFileHdrLinux>(fd, fs, file);
    }

    static std::shared_ptr<TraceBaseInfoContent> CreateBaseInfo(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceBaseInfoContent>(fd, fs, file, false);
    }

    static std::shared_ptr<ITraceCpuRawContent> CreateCpuRaw(int fd, const std::string& fs, const std::string& file,
        const TraceDumpRequest& req)
    {
        return std::make_shared<TraceCpuRawLinux>(fd, fs, file, req);
    }

    static std::shared_ptr<ITraceHeaderPageContent> CreateHeaderPage(int fd, const std::string& fs,
        const std::string& file)
    {
        return std::make_shared<TraceHeaderPageLinux>(fd, fs, file);
    }

    static std::shared_ptr<ITracePrintkFmtContent> CreatePrintkFmt(int fd, const std::string& fs,
        const std::string& file)
    {
        return std::make_shared<TracePrintkFmtLinux>(fd, fs, file);
    }

    static std::shared_ptr<TraceEventFmtContent> CreateEventFmt(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceEventFmtContent>(fd, fs, file, false);
    }

    static std::shared_ptr<TraceCmdLinesContent> CreateCmdLines(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceCmdLinesContent>(fd, fs, file, false);
    }

    static std::shared_ptr<TraceTgidsContent> CreateTgids(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceTgidsContent>(fd, fs, file, false);
    }

    static std::shared_ptr<ITraceCpuRawRead> CreateCpuRawRead(const std::string& fs, const TraceDumpRequest& req)
    {
        return std::make_shared<TraceCpuRawReadLinux>(fs, req);
    }

    static std::shared_ptr<ITraceCpuRawWrite> CreateCpuRawWrite(int fd, const std::string& file, uint64_t taskId)
    {
        return std::make_shared<TraceCpuRawWriteLinux>(fd, file, taskId);
    }
};

struct HMTraceSourceStrategy {
    static std::shared_ptr<ITraceFileHdrContent> CreateFileHdr(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceFileHdrHM>(fd, fs, file);
    }

    static std::shared_ptr<TraceBaseInfoContent> CreateBaseInfo(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceBaseInfoContent>(fd, fs, file, true);
    }

    static std::shared_ptr<ITraceCpuRawContent> CreateCpuRaw(int fd, const std::string& fs, const std::string& file,
        const TraceDumpRequest& req)
    {
        return std::make_shared<TraceCpuRawHM>(fd, fs, file, req);
    }

    static std::shared_ptr<ITraceHeaderPageContent> CreateHeaderPage(int fd, const std::string& fs,
        const std::string& file)
    {
        return std::make_shared<TraceHeaderPageHM>(fd, fs, file);
    }

    static std::shared_ptr<ITracePrintkFmtContent> CreatePrintkFmt(int fd, const std::string& fs,
        const std::string& file)
    {
        return std::make_shared<TracePrintkFmtHM>(fd, fs, file);
    }

    static std::shared_ptr<TraceEventFmtContent> CreateEventFmt(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceEventFmtContent>(fd, fs, file, true);
    }

    static std::shared_ptr<TraceCmdLinesContent> CreateCmdLines(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceCmdLinesContent>(fd, fs, file, true);
    }

    static std::shared_ptr<TraceTgidsContent> CreateTgids(int fd, const std::string& fs, const std::string& file)
    {
        return std::make_shared<TraceTgidsContent>(fd, fs, file, true);
    }

    static std::shared_ptr<ITraceCpuRawRead> CreateCpuRawRead(const std::string& fs, const TraceDumpRequest& req)
    {
        return std::make_shared<TraceCpuRawReadHM>(fs, req);
    }

    static std::shared_ptr<ITraceCpuRawWrite> CreateCpuRawWrite(int fd, const std::string& file, uint64_t taskId)
    {
        return std::make_shared<TraceCpuRawWriteHM>(fd, file, taskId);
    }
};

template <typename Strategy>
class TraceSource : public ITraceSource {
public:
    TraceSource() = delete;
    TraceSource(const std::string& tracefsPath, const std::string& traceFilePath);
    ~TraceSource() override;

    std::shared_ptr<ITraceFileHdrContent> GetTraceFileHeader() override;
    std::shared_ptr<TraceBaseInfoContent> GetTraceBaseInfo() override;
    std::shared_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) override;
    std::shared_ptr<ITraceHeaderPageContent> GetTraceHeaderPage() override;
    std::shared_ptr<ITracePrintkFmtContent> GetTracePrintkFmt() override;
    std::shared_ptr<TraceEventFmtContent> GetTraceEventFmt() override;
    std::shared_ptr<TraceCmdLinesContent> GetTraceCmdLines() override;
    std::shared_ptr<TraceTgidsContent> GetTraceTgids() override;
    std::shared_ptr<ITraceCpuRawRead> GetTraceCpuRawRead(const TraceDumpRequest& request) override;
    std::shared_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(const uint64_t taskId) override;
    std::string GetTraceFilePath() override;
    bool UpdateTraceFile(const std::string& traceFilePath) override;

private:
    int traceFileFd_ = -1;
    std::string tracefsPath_;
    std::string traceFilePath_;
};

using TraceSourceLinux = TraceSource<LinuxTraceSourceStrategy>;
using TraceSourceHM = TraceSource<HMTraceSourceStrategy>;
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_SOURCE_H