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

#include "trace_source.h"

#include <fcntl.h>
#include <hilog/log.h>
#include <string>
#include <unistd.h>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace{
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceSource"
#endif

static bool UpdateFileFd(const std::string& traceFile, int& fd)
{
    int newFd = open(traceFile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644 : -rw-r--r--
    if (newFd < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSource: open %{public}s failed.", traceFile.c_str());
        return false;
    }
    if (fd != -1) {
        close(fd);
    }
    fd = newFd;
    return true;
}
}

TraceSourceLinux::TraceSourceLinux(const std::string& tracefsPath, const std::string& traceFilePath) :
    tracefsPath_(tracefsPath), traceFilePath_(traceFilePath)
{
    traceFileFd_ = open(traceFilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644 : -rw-r--r--
    if (traceFileFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSourceLinux: open %{public}s failed.", traceFilePath.c_str());
    }
}

TraceSourceLinux::~TraceSourceLinux()
{
    if (traceFileFd_ != -1) {
        close(traceFileFd_);
    }
}

ITraceFileHdrContent* TraceSourceLinux::GetTraceFileHeader()
{
    return new TraceFileHdrLinux(traceFileFd_, tracefsPath_, traceFilePath_);
}

ITraceCpuRawContent* TraceSourceLinux::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return new TraceCpuRawLinux(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

ITraceHeaderPageContent* TraceSourceLinux::GetTraceHeaderPage()
{
    return new TraceHeaderPageLinux(traceFileFd_, tracefsPath_, traceFilePath_);
}

ITracePrintkFmtContent* TraceSourceLinux::GetTracePrintkFmt()
{
    return new TracePrintkFmtLinux(traceFileFd_, tracefsPath_, traceFilePath_);
}

TraceEventFmtContent* TraceSourceLinux::GetTraceEventFmt()
{
    return new TraceEventFmtContent(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

TraceCmdLinesContent* TraceSourceLinux::GetTraceCmdLines()
{
    return new TraceCmdLinesContent(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

TraceTgidsContent* TraceSourceLinux::GetTraceTgids()
{
    return new TraceTgidsContent(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::string TraceSourceLinux::GetTraceFilePath()
{
    return traceFilePath_;
}

bool TraceSourceLinux::UpdateTraceFile(const std::string& traceFilePath)
{
    if (!UpdateFileFd(traceFilePath, traceFileFd_)) {
        return false;
    }
    traceFilePath_ = traceFilePath;
    return true;
}

TraceSourceHM::TraceSourceHM(const std::string& tracefsPath, const std::string& traceFilePath) :
    tracefsPath_(tracefsPath), traceFilePath_(traceFilePath)
{
    traceFileFd_ = open(traceFilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644 : -rw-r--r--
    if (traceFileFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSourceHM: open %{public}s failed.", traceFilePath.c_str());
    }
}

TraceSourceHM::~TraceSourceHM()
{
    if (traceFileFd_ != -1) {
        close(traceFileFd_);
    }
}

ITraceFileHdrContent* TraceSourceHM::GetTraceFileHeader()
{
    return new TraceFileHdrHM(traceFileFd_, tracefsPath_, traceFilePath_);
}

ITraceCpuRawContent* TraceSourceHM::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return new TraceCpuRawHM(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

ITraceHeaderPageContent* TraceSourceHM::GetTraceHeaderPage()
{
    return new TraceHeaderPageHM(traceFileFd_, tracefsPath_, traceFilePath_);
}

ITracePrintkFmtContent* TraceSourceHM::GetTracePrintkFmt()
{
    return new TracePrintkFmtHM(traceFileFd_, tracefsPath_, traceFilePath_);
}

TraceEventFmtContent* TraceSourceHM::GetTraceEventFmt()
{
    return new TraceEventFmtContent(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

TraceCmdLinesContent* TraceSourceHM::GetTraceCmdLines()
{
    return new TraceCmdLinesContent(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

TraceTgidsContent* TraceSourceHM::GetTraceTgids()
{
    return new TraceTgidsContent(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::string TraceSourceHM::GetTraceFilePath()
{
    return traceFilePath_;
}

bool TraceSourceHM::UpdateTraceFile(const std::string& traceFilePath)
{
    if (!UpdateFileFd(traceFilePath, traceFileFd_)) {
        return false;
    }
    traceFilePath_ = traceFilePath;
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS