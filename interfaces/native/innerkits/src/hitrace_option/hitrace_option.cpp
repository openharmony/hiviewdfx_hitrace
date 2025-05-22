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
#include <fstream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "hilog/log.h"
#include "parameters.h"

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

const std::string TELEMETRY_APP_PARAM = "debug.hitrace.telemetry.app";
const std::string SET_EVENT_PID = "/sys/kernel/tracing/set_event_pid";
const std::string DEBUG_SET_EVENT_PID = "/sys/kernel/debug/tracing/set_event_pid";

class FileLock {
private:
    int fd_ = -1;
public:
    explicit FileLock(const std::string& filename, int flags)
    {
        fd_ = open(filename.c_str(), flags);
        if (fd_ == -1) {
            HILOG_ERROR(LOG_CORE, "FileLock: %{public}s open failed, errno%{public}d", filename.c_str(), errno);
            return;
        }

#ifdef ENABLE_LOCK
        if (flock(fd_, LOCK_EX) != 0) {
            HILOG_ERROR(LOG_CORE, "FileLock: %{public}s lock failed.", filename.c_str());
            close(fd_);
            fd_ = -1;
        }
        HILOG_INFO(LOG_CORE, "FileLock: %{public}s lock succ, fd = %{public}d", filename.c_str(), fd_);
#endif
    }

    ~FileLock()
    {
        if (fd_ != -1) {
#ifdef ENABLE_LOCK
            flock(fd_, LOCK_UN);
            HILOG_INFO(LOG_CORE, "FileLock: %{public}d unlock succ", fd_);
#endif
            close(fd_);
        }
    }

    int fd()
    {
        return fd_;
    }
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

    int fd = fileLock.fd();
    if (fd == -1) {
        return false;
    }

    off_t offset = lseek(fd, 0, SEEK_END);
    if (offset == -1) {
        HILOG_ERROR(LOG_CORE, "AppendToFile: %{public}s lseek failed %{public}d", filename.c_str(), errno);
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
    int fd = creat(DEBUG_SET_EVENT_PID.c_str(), 0);
    if (fd != -1) {
        close(fd);
        HILOG_INFO(LOG_CORE, "ClearFilterPid success");
        return HITRACE_NO_ERROR;
    }

    fd = creat(SET_EVENT_PID.c_str(), 0);
    if (fd != -1) {
        close(fd);
        HILOG_INFO(LOG_CORE, "ClearFilterPid success");
        return HITRACE_NO_ERROR;
    }

    HILOG_INFO(LOG_CORE, "ClearFilterPid fail");
    return HITRACE_WRITE_FILE_ERROR;
}

void FilterAppTrace(const std::string& app, pid_t pid)
{
    HILOG_INFO(LOG_CORE, "FilterAppTrace %{public}s %{public}d", app.c_str(), pid);
    std::string paramApp = OHOS::system::GetParameter(TELEMETRY_APP_PARAM, "");
    if (app == paramApp) {
        AddFilterPid(pid);
    }
}

void FilterAppTrace(const char* app, pid_t pid)
{
    if (app == nullptr) {
        HILOG_INFO(LOG_CORE, "FilterAppTrace: app is null");
        return;
    }
    FilterAppTrace(std::string(app), pid);
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
