/*
* Copyright (C) 2026 Huawei Device Co., Ltd.
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

#include "trace_context.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <set>
#include <sstream>

#include "hilog/log.h"
#include "hitrace_option_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceFilter"
#endif

namespace {
constexpr auto SET_EVENT_PID = "set_event_pid";

bool ParseNumberStr(const char* lineContent, std::string& result)
{
    size_t validStrLength = 0;
    while (isdigit(lineContent[validStrLength])) {
        validStrLength++;
    }
    if (validStrLength == 0) {
        return false;
    }
    result = std::string(lineContent, validStrLength);
    return true;
}

bool GetSetEventPid(std::set<std::string>& set)
{
    const std::string& traceRootPath = GetTraceRootPath();
    if (traceRootPath.empty()) {
        return false;
    }
    return TraverseFileLineByLine(traceRootPath + SET_EVENT_PID, [&set](const char* lineContent, size_t lineNum) {
        std::string tid;
        if (ParseNumberStr(lineContent, tid)) {
            set.insert(tid);
        }
        return true;
    });
}

bool ParseTGid(const char* lineContent, std::pair<std::string, std::string>& result)
{
    if (!ParseNumberStr(lineContent, result.first)) {
        return false;
    };
    if (!isspace(lineContent[result.first.length()])) {
        return false;
    }
    return ParseNumberStr(lineContent + result.first.length() + 1, result.second);
}
}

struct StandInThreadContext {
    std::mutex mutex;
    std::condition_variable cv;
    pid_t tid = -1;
    bool stopFlag = false;
    std::thread thread;
    StandInThreadContext();
    ~StandInThreadContext();
};

StandInThreadContext::StandInThreadContext()
{
    std::unique_lock<std::mutex> lock(mutex);
    thread = std::thread([this]() {
        std::unique_lock<std::mutex> lock(mutex);
        tid = gettid();
        cv.notify_one();
        cv.wait(lock, [this] {
            return stopFlag;
        });
    });
    cv.wait(lock, [this] {
        return tid >= 0;
    });
}

StandInThreadContext::~StandInThreadContext()
{
    if (thread.joinable()) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            stopFlag = true;
            cv.notify_one();
        }
        thread.join();
    }
}

TraceFilterContext::TraceFilterContext()
{
    initPid_ = getpid();
    standInThreadContext_ = new StandInThreadContext();
    standInTid_ = standInThreadContext_->tid;
    ClearTracePoint(SET_EVENT_PID);
}

TraceFilterContext::~TraceFilterContext()
{
    ClearTracePoint(SET_EVENT_PID);
    if (initPid_ == getpid()) {
        delete standInThreadContext_;
    }
}

bool TraceFilterContext::AddFilterPids(const std::vector<std::string> &filterPids)
{
    if (filterPids.empty()) {
        return false;
    }
    FileLock fileLock(GetTraceRootPath() + SET_EVENT_PID, O_RDWR);
    std::string initContent = std::to_string(standInTid_);
    for (const auto& tid : filterPids) {
        initContent += (" " + tid);
    }
    if (write(fileLock.Fd(), initContent.c_str(), initContent.size()) < 0) {
        HILOG_ERROR(LOG_CORE, "AppendToFile: set_event_pid %{public}s failed %{public}d", initContent.c_str(), errno);
        return false;
    }
    HILOG_INFO(LOG_CORE, "success add content %{public}s to set_event_pid", initContent.c_str());
    std::vector<std::string> tids;
    for (auto& pid : filterPids) {
        const std::string dirPath = "/proc/" + pid + "/task/";
        const auto dir = std::unique_ptr<DIR, void(*)(DIR*)>(opendir(dirPath.c_str()), [](DIR* dir) {
            if (dir != nullptr) {
                closedir(dir);
            }
        });
        if (!dir) {
            HILOG_ERROR(LOG_CORE, "failed open dirpath %{public}s for %{public}d", dirPath.c_str(), errno);
            continue;
        }
        for (dirent* entry = readdir(dir.get()); entry != nullptr; entry = readdir(dir.get())) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            originTGids_[entry->d_name] = pid;
            if (write(fileLock.Fd(), entry->d_name, strlen(entry->d_name)) < 0) {
                HILOG_ERROR(LOG_CORE, "AppendToFile: set_event_pid %{public}s failed %{public}d", entry->d_name, errno);
                return false;
            }
        }
        HILOG_INFO(LOG_CORE, "success add tids of %{public}s to set_event_pid", pid.c_str());
    }
    return true;
}

void TraceFilterContext::FilterSavedCmdLine()
{
    filterCmdLines_.clear();
    const std::string& traceRootPath = GetTraceRootPath();
    if (traceRootPath.empty()) {
        return;
    }
    TraverseFileLineByLine(traceRootPath + "saved_cmdlines",
        [this](const char* lineContent, size_t lineNum) {
            std::string tid;
            if (!ParseNumberStr(lineContent, tid)) {
                return true;
            }
            bool ret = std::any_of(filterTGidsContent_.begin(), filterTGidsContent_.end(),
                [&tid] (const std::pair<std::string, std::string>& tgids) {
                        return tid == tgids.first;
                });
            if (ret) {
                filterCmdLines_.emplace_back(lineContent);
            }
            return true;
        });
}

void TraceFilterContext::FilterTGidsContent()
{
    filterTGidsContent_.clear();
    filterPids_.clear();
    const std::string& traceRootPath = GetTraceRootPath();
    std::set<std::string> set;
    if (traceRootPath.empty() || !GetSetEventPid(set)) {
        return;
    }
    const auto standInTidStr = std::to_string(standInTid_);
    TraverseFileLineByLine(traceRootPath + "saved_tgids",
        [this, &set, &standInTidStr](const char* lineContent, size_t lineNum) {
            std::pair<std::string, std::string> tGid;
            if (!ParseTGid(lineContent, tGid)) {
                return true;
            }
            if (set.find(tGid.first) != set.end()) {
                if (tGid.first == standInTidStr) {
                    return true;
                }
                auto originTGid = originTGids_.find(tGid.first);
                if (originTGid != originTGids_.end() && originTGid->second != tGid.second) {
                    return true;
                }
                filterPids_.insert(tGid.second);
            }
            return true;
        });
    TraverseFileLineByLine(traceRootPath + "saved_tgids",
        [this, &standInTidStr](const char* lineContent, size_t lineNum) {
            std::pair<std::string, std::string> tGid;
            if (!ParseTGid(lineContent, tGid)) {
                return true;
            }
            if (filterPids_.find(tGid.second) != filterPids_.end()) {
                if (tGid.first != standInTidStr) {
                    filterTGidsContent_.emplace_back(tGid);
                }
            }
            return true;
        });
}

void TraceFilterContext::FilterTraceContent()
{
    FilterTGidsContent();
    FilterSavedCmdLine();
}

void TraceFilterContext::TraverseSavedCmdLine(const std::function<void(const std::string& savedCmdLine)>& handler) const
{
    if (handler) {
        std::for_each(filterCmdLines_.begin(),  filterCmdLines_.end(), handler);
    }
}

void TraceFilterContext::TraverseTGidsContent(
    const std::function<void(const std::pair<std::string, std::string>& tgid)>& handler) const
{
    if (handler) {
        std::for_each(filterTGidsContent_.begin(),  filterTGidsContent_.end(), handler);
    }
}

void TraceFilterContext::TraverseFilterPid(const std::function<void(const std::string& pid)>& handler) const
{
    if (handler) {
        std::for_each(filterPids_.begin(),  filterPids_.end(), handler);
    }
}

TraceContextManager &TraceContextManager::GetInstance()
{
    static TraceContextManager instance;
    return instance;
}

void TraceContextManager::ReleaseContext()
{
    traceFilterContext_ = nullptr;
}

std::shared_ptr<TraceFilterContext> TraceContextManager::GetTraceFilterContext(bool createIfNotExisted)
{
    if (traceFilterContext_ || !createIfNotExisted) {
        return traceFilterContext_;
    }
    traceFilterContext_ = std::make_shared<TraceFilterContext>();
    return traceFilterContext_;
}
}
}
}