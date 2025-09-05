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

#include "trace_source_factory.h"

#include <fcntl.h>
#include <hilog/log.h>
#include <string>
#include <unistd.h>

#include "common_utils.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceSource"
#endif

static bool UpdateFileFd(const std::string& traceFile, UniqueFd& fd)
{
    std::string path = CanonicalizeSpecPath(traceFile.c_str());
    UniqueFd newFd(open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644)); // 0644 : -rw-r--r--
    if (newFd < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSource: open %{public}s failed.", traceFile.c_str());
        return false;
    }
    fd.Release();
    fd = std::move(newFd);
    return true;
}
}

TraceSourceLinuxFactory::TraceSourceLinuxFactory(const std::string& tracefsPath, const std::string& traceFilePath)
    : tracefsPath_(tracefsPath), traceFilePath_(traceFilePath)
{
    if (traceFilePath.empty()) {
        return;
    }
    std::string path = CanonicalizeSpecPath(traceFilePath.c_str());
    traceFileFd_ = UniqueFd(open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644)); // 0644 : -rw-r--r--
    if (traceFileFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSourceLinux: open %{public}s failed.", traceFilePath.c_str());
    }
}

std::unique_ptr<ITraceFileHdrContent> TraceSourceLinuxFactory::GetTraceFileHeader()
{
    return std::make_unique<TraceFileHdrLinux>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::unique_ptr<ITraceCpuRawContent> TraceSourceLinuxFactory::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return std::make_unique<TraceCpuRawLinux>(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

std::unique_ptr<ITraceHeaderPageContent> TraceSourceLinuxFactory::GetTraceHeaderPage()
{
    return std::make_unique<TraceHeaderPageLinux>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::unique_ptr<ITracePrintkFmtContent> TraceSourceLinuxFactory::GetTracePrintkFmt()
{
    return std::make_unique<TracePrintkFmtLinux>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::unique_ptr<TraceEventFmtContent> TraceSourceLinuxFactory::GetTraceEventFmt()
{
    return std::make_unique<TraceEventFmtContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::unique_ptr<TraceCmdLinesContent> TraceSourceLinuxFactory::GetTraceCmdLines()
{
    return std::make_unique<TraceCmdLinesContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::unique_ptr<TraceTgidsContent> TraceSourceLinuxFactory::GetTraceTgids()
{
    return std::make_unique<TraceTgidsContent>(traceFileFd_, tracefsPath_, traceFilePath_, false);
}

std::string TraceSourceLinuxFactory::GetTraceFilePath()
{
    return traceFilePath_;
}

bool TraceSourceLinuxFactory::UpdateTraceFile(const std::string& traceFilePath)
{
    if (!UpdateFileFd(traceFilePath, traceFileFd_)) {
        return false;
    }
    traceFilePath_ = traceFilePath;
    return true;
}

TraceSourceHMFactory::TraceSourceHMFactory(const std::string& tracefsPath, const std::string& traceFilePath)
    : tracefsPath_(tracefsPath), traceFilePath_(traceFilePath)
{
    if (traceFilePath.empty()) {
        return;
    }
    std::string path = CanonicalizeSpecPath(traceFilePath.c_str());
    traceFileFd_ = UniqueFd(open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644)); // 0644 : -rw-r--r--
    if (traceFileFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSourceHM: open %{public}s failed.", traceFilePath.c_str());
    }
}

std::unique_ptr<ITraceFileHdrContent> TraceSourceHMFactory::GetTraceFileHeader()
{
    return std::make_unique<TraceFileHdrHM>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::unique_ptr<ITraceCpuRawContent> TraceSourceHMFactory::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return std::make_unique<TraceCpuRawHM>(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

std::unique_ptr<ITraceHeaderPageContent> TraceSourceHMFactory::GetTraceHeaderPage()
{
    return std::make_unique<TraceHeaderPageHM>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::unique_ptr<ITracePrintkFmtContent> TraceSourceHMFactory::GetTracePrintkFmt()
{
    return std::make_unique<TracePrintkFmtHM>(traceFileFd_, tracefsPath_, traceFilePath_);
}

std::unique_ptr<TraceEventFmtContent> TraceSourceHMFactory::GetTraceEventFmt()
{
    return std::make_unique<TraceEventFmtContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::unique_ptr<TraceCmdLinesContent> TraceSourceHMFactory::GetTraceCmdLines()
{
    return std::make_unique<TraceCmdLinesContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::unique_ptr<TraceTgidsContent> TraceSourceHMFactory::GetTraceTgids()
{
    return std::make_unique<TraceTgidsContent>(traceFileFd_, tracefsPath_, traceFilePath_, true);
}

std::string TraceSourceHMFactory::GetTraceFilePath()
{
    return traceFilePath_;
}

bool TraceSourceHMFactory::UpdateTraceFile(const std::string& traceFilePath)
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