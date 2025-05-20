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

std::shared_ptr<ITraceFileHdrContent> TraceSourceLinux::GetTraceFileHeader()
{
    return std::make_shared<TraceFileHdrLinux>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::shared_ptr<TraceBaseInfoContent> TraceSourceLinux::GetTraceBaseInfo()
{
    return std::make_shared<TraceBaseInfoContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::shared_ptr<ITraceCpuRawContent> TraceSourceLinux::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return std::make_shared<TraceCpuRawLinux>(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

std::shared_ptr<ITraceHeaderPageContent> TraceSourceLinux::GetTraceHeaderPage()
{
    return std::make_shared<TraceHeaderPageLinux>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::shared_ptr<ITracePrintkFmtContent> TraceSourceLinux::GetTracePrintkFmt()
{
    return std::make_shared<TracePrintkFmtLinux>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::shared_ptr<TraceEventFmtContent> TraceSourceLinux::GetTraceEventFmt()
{
    return std::make_shared<TraceEventFmtContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::shared_ptr<TraceCmdLinesContent> TraceSourceLinux::GetTraceCmdLines()
{
    return std::make_shared<TraceCmdLinesContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::shared_ptr<TraceTgidsContent> TraceSourceLinux::GetTraceTgids()
{
    return std::make_shared<TraceTgidsContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
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

std::shared_ptr<ITraceFileHdrContent> TraceSourceHM::GetTraceFileHeader()
{
    return std::make_shared<TraceFileHdrHM>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::shared_ptr<TraceBaseInfoContent> TraceSourceHM::GetTraceBaseInfo()
{
    return std::make_shared<TraceBaseInfoContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::shared_ptr<ITraceCpuRawContent> TraceSourceHM::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return std::make_shared<TraceCpuRawHM>(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

std::shared_ptr<ITraceHeaderPageContent> TraceSourceHM::GetTraceHeaderPage()
{
    return std::make_shared<TraceHeaderPageHM>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::shared_ptr<ITracePrintkFmtContent> TraceSourceHM::GetTracePrintkFmt()
{
    return std::make_shared<TracePrintkFmtHM>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::shared_ptr<TraceEventFmtContent> TraceSourceHM::GetTraceEventFmt()
{
    return std::make_shared<TraceEventFmtContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::shared_ptr<TraceCmdLinesContent> TraceSourceHM::GetTraceCmdLines()
{
    return std::make_shared<TraceCmdLinesContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::shared_ptr<TraceTgidsContent> TraceSourceHM::GetTraceTgids()
{
    return std::make_shared<TraceTgidsContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
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