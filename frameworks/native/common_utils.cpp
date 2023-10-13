/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common_utils.h"

#include <cinttypes>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

std::string CanonicalizeSpecPath(const char* src)
{
    if (src == nullptr || strlen(src) >= PATH_MAX) {
        HiLog::Error(LABEL, "CanonicalizeSpecPath: %{pubilc}s failed.", src);
        return "";
    }
    char resolvedPath[PATH_MAX] = { 0 };

    if (access(src, F_OK) == 0) {
        if (realpath(src, resolvedPath) == nullptr) {
            HiLog::Error(LABEL, "CanonicalizeSpecPath: realpath %{pubilc}s failed.", src);
            return "";
        }
    } else {
        std::string fileName(src);
        if (fileName.find("..") == std::string::npos) {
            if (sprintf_s(resolvedPath, PATH_MAX, "%s", src) == -1) {
                HiLog::Error(LABEL, "CanonicalizeSpecPath: sprintf_s %{pubilc}s failed.", src);
                return "";
            }
        } else {
            HiLog::Error(LABEL, "CanonicalizeSpecPath: find .. src failed.");
            return "";
        }
    }

    std::string res(resolvedPath);
    return res;
}

bool MarkClockSync(const std::string& traceRootPath)
{
    constexpr unsigned int bufferSize = 128;
    char buffer[bufferSize] = { 0 };
    std::string traceMarker = "trace_marker";
    std::string resolvedPath = CanonicalizeSpecPath((traceRootPath + traceMarker).c_str());
    int fd = open(resolvedPath.c_str(), O_WRONLY);
    if (fd == -1) {
        HiLog::Error(LABEL, "MarkClockSync: oepn %{public}s fail, errno(%{public}d)", resolvedPath.c_str(), errno);
        return false;
    }

    // write realtime_ts
    struct timespec rts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &rts) == -1) {
        HiLog::Error(LABEL, "MarkClockSync: get realtime error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr unsigned int nanoSeconds = 1000000000; // seconds converted to nanoseconds
    constexpr unsigned int nanoToMill = 1000000; // millisecond converted to nanoseconds
    int len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1,
        "trace_event_clock_sync: realtime_ts=%" PRId64 "\n",
        static_cast<int64_t>((rts.tv_sec * nanoSeconds + rts.tv_nsec) / nanoToMill));
    if (len < 0) {
        HiLog::Error(LABEL, "MarkClockSync: entering realtime_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }

    if (write(fd, buffer, len) < 0) {
        HiLog::Error(LABEL, "MarkClockSync: writing realtime error, errno(%{public}d)", errno);
    }

    // write parent_ts
    struct timespec mts = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &mts) == -1) {
        HiLog::Error(LABEL, "MarkClockSync: get parent_ts error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr float nanoToSecond = 1000000000.0f; // consistent with the ftrace timestamp format
    len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "trace_event_clock_sync: parent_ts=%f\n",
        static_cast<float>(((static_cast<float>(mts.tv_sec)) * nanoSeconds + mts.tv_nsec) / nanoToSecond));
    if (len < 0) {
        HiLog::Error(LABEL, "MarkClockSync: entering parent_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    if (write(fd, buffer, len) < 0) {
        HiLog::Error(LABEL, "MarkClockSync: writing parent_ts error, errno(%{public}d)", errno);
    }
    close(fd);
    return true;
}

} // Hitrace
} // HiviewDFX
} // OHOS
