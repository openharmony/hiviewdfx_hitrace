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

#include "hitrace_option/hitrace_option.h"

#include <fcntl.h>
#include <sstream>
#include <filesystem>
#include <unistd.h>

#include "hilog/log.h"
#include "parameters.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceOption"
#endif

static const char* const TELEMETRY_APP_PARAM = "debug.hitrace.telemetry.app";
static const char* const SET_EVENT_PID = "/sys/kernel/tracing/set_event_pid";
static const char* const DEBUG_SET_EVENT_PID = "/sys/kernel/debug/tracing/set_event_pid";
static const char* const SET_NO_FILTER_EVENT = "/sys/kernel/tracing/no_filter_events";
static const char* const DEBUG_SET_NO_FILTER_EVENT = "/sys/kernel/debug/tracing/no_filter_events";
class FileLock {
public:
    explicit FileLock(const std::string& filename, int flags)
    {
        char canonicalPath[PATH_MAX + 1] = {0};
        if (realpath(filename.c_str(), canonicalPath) == nullptr) {
            HILOG_ERROR(LOG_CORE, "FileLock: %{public}s realpath failed, errno%{public}d", filename.c_str(), errno);
            return;
        }
        fd_ = SmartFd(open(canonicalPath, flags));
        if (!fd_) {
            HILOG_ERROR(LOG_CORE, "FileLock: %{public}s open failed, errno%{public}d", filename.c_str(), errno);
            return;
        }
#ifdef ENABLE_LOCK
        if (flock(fd_.GetFd(), LOCK_EX) != 0) {
            HILOG_ERROR(LOG_CORE, "FileLock: %{public}s lock failed.", filename.c_str());
            fd_.Reset();
        }
        HILOG_INFO(LOG_CORE, "FileLock: %{public}s lock succ, fd = %{public}d", filename.c_str(), fd_.GetFd());
#endif
    }

    ~FileLock()
    {
        if (fd_) {
#ifdef ENABLE_LOCK
            flock(fd_, LOCK_UN);
            HILOG_INFO(LOG_CORE, "FileLock: %{public}d unlock succ", fd_);
#endif
        }
    }

    int Fd()
    {
        return fd_.GetFd();
    }
private:
    SmartFd fd_;
};

std::vector<std::string> GetSubThreadIds(const std::string& pid)
{
    std::vector<std::string> ret;
    std::string path = "/proc/" + pid + "/task/";

    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            ret.emplace_back(entry.path().filename());
        }
    }

    return ret;
}

bool AppendToFile(const std::string& filename, const std::string& str)
{
    FileLock fileLock(filename, O_RDWR);

    int fd = fileLock.Fd();
    if (fd == -1) {
        return false;
    }

    if (write(fd, str.c_str(), str.size()) < 0) {
        HILOG_ERROR(LOG_CORE, "AppendToFile: %{public}s write failed %{public}d", filename.c_str(), errno);
        return false;
    }
    return true;
}

int32_t SetFilterAppName(const std::string& app)
{
    bool ret = OHOS::system::SetParameter(TELEMETRY_APP_PARAM, app);
    HILOG_INFO(LOG_CORE, "SetTelemetryAppName %{public}s ret=%{public}d", app.c_str(), ret);
    return ret ? HITRACE_NO_ERROR : HITRACE_SET_PARAM_ERROR;
}

int32_t SetFilterAppName(const std::vector<std::string>& apps)
{
    auto appsSize = apps.size();
    constexpr auto maxAppNameLen = 10;
    if (appsSize > maxAppNameLen) {
        return HITRACE_SET_PARAM_ERROR;
    }
    std::string appNames;
    for (size_t i = 0; i < appsSize; i++) {
        appNames.append(apps[i]);
        if (i != apps.size() - 1) {
            appNames.append("\t");
        }
    }
    return SetFilterAppName(appNames);
}

int32_t AddFilterPid(const pid_t pid)
{
    std::vector<std::string> vec = { std::to_string(pid) };
    return AddFilterPids(vec);
}

int32_t AddFilterPids(const std::vector<std::string>& pids)
{
    std::stringstream ss(" ");
    for (const auto& pid : pids) {
        for (const auto& tid : GetSubThreadIds(pid)) {
            ss << tid << " ";
        }
    }

    std::string pidStr = ss.str();
    if (AppendToFile(DEBUG_SET_EVENT_PID, pidStr) || AppendToFile(SET_EVENT_PID, pidStr)) {
        HILOG_INFO(LOG_CORE, "AddFilterPids %{public}s success", pidStr.c_str());
        return HITRACE_NO_ERROR;
    }

    HILOG_INFO(LOG_CORE, "AddFilterPids %{public}s fail", pidStr.c_str());
    return HITRACE_WRITE_FILE_ERROR;
}

int32_t ClearFilterPid()
{
    int fd = creat(DEBUG_SET_EVENT_PID, 0);
    if (fd != -1) {
        close(fd);
        HILOG_INFO(LOG_CORE, "ClearFilterPid success");
        return HITRACE_NO_ERROR;
    }

    fd = creat(SET_EVENT_PID, 0);
    if (fd != -1) {
        close(fd);
        HILOG_INFO(LOG_CORE, "ClearFilterPid success");
        return HITRACE_NO_ERROR;
    }

    HILOG_INFO(LOG_CORE, "ClearFilterPid fail");
    return HITRACE_WRITE_FILE_ERROR;
}

void FilterAppTrace(const char* app, pid_t pid)
{
    if (app == nullptr) {
        HILOG_INFO(LOG_CORE, "FilterAppTrace: app is null");
        return;
    }
    HILOG_INFO(LOG_CORE, "FilterAppTrace %{public}s %{public}d", app, pid);
    std::string paramApp = OHOS::system::GetParameter(TELEMETRY_APP_PARAM, "");
    auto pos = paramApp.find(app);
    while (pos != std::string::npos) {
        size_t left = pos;
        size_t right = pos + strlen(app);
        if ((left > 0 && paramApp.at(left - 1) != '\t') ||
            (right < paramApp.size() && paramApp.at(right) != '\t')) {
            pos = paramApp.find(app, right);
            continue;
        }
        AddFilterPid(pid);
        return;
    }
}

int32_t AddNoFilterEvents(const std::vector<std::string>& events)
{
    std::stringstream ss(" ");
    for (size_t i = 0; i < events.size(); i++) {
        ss << events[i] << " ";
    }
    std::string eventStr = ss.str();
    if (AppendToFile(DEBUG_SET_NO_FILTER_EVENT, eventStr) || AppendToFile(SET_NO_FILTER_EVENT, eventStr)) {
        HILOG_INFO(LOG_CORE, "AddNoFilterEvents %{public}s success", eventStr.c_str());
        return HITRACE_NO_ERROR;
    }
    HILOG_INFO(LOG_CORE, "AddNoFilterEvents %{public}s fail", eventStr.c_str());
    return HITRACE_WRITE_FILE_ERROR;
}

int32_t ClearNoFilterEvents()
{
    int fd = creat(DEBUG_SET_NO_FILTER_EVENT, 0);
    if (fd != -1) {
        close(fd);
        HILOG_INFO(LOG_CORE, "ClearNoFilterEvents success");
        return HITRACE_NO_ERROR;
    }

    fd = creat(SET_NO_FILTER_EVENT, 0);
    if (fd != -1) {
        close(fd);
        HILOG_INFO(LOG_CORE, "ClearNoFilterEvents success");
        return HITRACE_NO_ERROR;
    }

    HILOG_INFO(LOG_CORE, "ClearNoFilterEvents fail");
    return HITRACE_WRITE_FILE_ERROR;
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
