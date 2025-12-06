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

static bool UpdateFileFd(const std::string& traceFile, int& fd)
{
    std::string path = CanonicalizeSpecPath(traceFile.c_str());
    int newFd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644 : -rw-r--r--
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

// 模板实现
template <typename Strategy>
TraceSource<Strategy>::TraceSource(const std::string& tracefsPath, const std::string& traceFilePath)
    : tracefsPath_(tracefsPath), traceFilePath_(traceFilePath)
{
    if (traceFilePath.empty()) {
        return;
    }
    std::string path = CanonicalizeSpecPath(traceFilePath.c_str());
    traceFileFd_ = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644); // 0644 : -rw-r--r--
    if (traceFileFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceSource: open %{public}s failed, errno(%{public}d)", traceFilePath.c_str(), errno);
    }
}

template <typename Strategy>
TraceSource<Strategy>::~TraceSource()
{
    if (traceFileFd_ != -1) {
        close(traceFileFd_);
    }
}

template <typename Strategy>
std::shared_ptr<ITraceFileHdrContent> TraceSource<Strategy>::GetTraceFileHeader()
{
    return Strategy::CreateFileHdr(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<TraceBaseInfoContent> TraceSource<Strategy>::GetTraceBaseInfo()
{
    return Strategy::CreateBaseInfo(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<ITraceCpuRawContent> TraceSource<Strategy>::GetTraceCpuRaw(const TraceDumpRequest& request)
{
    return Strategy::CreateCpuRaw(traceFileFd_, tracefsPath_, traceFilePath_, request);
}

template <typename Strategy>
std::shared_ptr<ITraceHeaderPageContent> TraceSource<Strategy>::GetTraceHeaderPage()
{
    return Strategy::CreateHeaderPage(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<ITracePrintkFmtContent> TraceSource<Strategy>::GetTracePrintkFmt()
{
    return Strategy::CreatePrintkFmt(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<TraceEventFmtContent> TraceSource<Strategy>::GetTraceEventFmt()
{
    return Strategy::CreateEventFmt(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<TraceCmdLinesContent> TraceSource<Strategy>::GetTraceCmdLines()
{
    return Strategy::CreateCmdLines(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<TraceTgidsContent> TraceSource<Strategy>::GetTraceTgids()
{
    return Strategy::CreateTgids(traceFileFd_, tracefsPath_, traceFilePath_);
}

template <typename Strategy>
std::shared_ptr<ITraceCpuRawRead> TraceSource<Strategy>::GetTraceCpuRawRead(const TraceDumpRequest& request)
{
    return Strategy::CreateCpuRawRead(tracefsPath_, request);
}

template <typename Strategy>
std::shared_ptr<ITraceCpuRawWrite> TraceSource<Strategy>::GetTraceCpuRawWrite(const uint64_t taskId)
{
    return Strategy::CreateCpuRawWrite(traceFileFd_, traceFilePath_, taskId);
}

template <typename Strategy>
std::string TraceSource<Strategy>::GetTraceFilePath()
{
    return traceFilePath_;
}

template <typename Strategy>
bool TraceSource<Strategy>::UpdateTraceFile(const std::string& traceFilePath)
{
    if (!UpdateFileFd(traceFilePath, traceFileFd_)) {
        return false;
    }
    traceFilePath_ = traceFilePath;
    return true;
}

// 显式实例化
template class TraceSource<LinuxTraceSourceStrategy>;
template class TraceSource<HMTraceSourceStrategy>;
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS