/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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

#include "common_utils.h"

#include <cinttypes>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "common_define.h"
#include "hilog/log.h"
#include "parameters.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceUtils"
#endif
namespace {
static const char* CPUFREQ_PREFIX = "/sys/devices/system/cpu/cpu";
static const char* CPUFREQ_AFTERFIX = "/cpufreq/scaling_cur_freq";
constexpr int DECIMAL_SCALE = 10;
constexpr int BUFFER_LEN = 32;
constexpr int EXPECT_NUM = 2;
}

std::string CanonicalizeSpecPath(const char* src)
{
    if (src == nullptr || strlen(src) >= PATH_MAX) {
        HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: %{pubilc}s failed.", src);
        return "";
    }
    char resolvedPath[PATH_MAX] = { 0 };

    if (access(src, F_OK) == 0) {
        if (realpath(src, resolvedPath) == nullptr) {
            HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: realpath %{pubilc}s failed.", src);
            return "";
        }
    } else {
        std::string fileName(src);
        if (fileName.find("..") == std::string::npos) {
            if (sprintf_s(resolvedPath, PATH_MAX, "%s", src) == -1) {
                HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: sprintf_s %{pubilc}s failed.", src);
                return "";
            }
        } else {
            HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: find .. src failed.");
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
    std::string resolvedPath = CanonicalizeSpecPath((traceRootPath + TRACE_MARKER_NODE).c_str());
    int fd = open(resolvedPath.c_str(), O_WRONLY);
    if (fd == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: open %{public}s fail, errno(%{public}d)", resolvedPath.c_str(), errno);
        return false;
    }

    // write realtime_ts
    struct timespec rts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &rts) == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: get realtime error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr unsigned int nanoSeconds = 1000000000; // seconds converted to nanoseconds
    constexpr unsigned int nanoToMill = 1000000; // millisecond converted to nanoseconds
    int len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1,
        "trace_event_clock_sync: realtime_ts=%" PRId64 "\n",
        static_cast<int64_t>((rts.tv_sec * nanoSeconds + rts.tv_nsec) / nanoToMill));
    if (len < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: entering realtime_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }

    if (write(fd, buffer, len) < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: writing realtime error, errno(%{public}d)", errno);
    }

    // write parent_ts
    struct timespec mts = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &mts) == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: get parent_ts error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr float nanoToSecond = 1000000000.0f; // consistent with the ftrace timestamp format
    len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "trace_event_clock_sync: parent_ts=%f\n",
        static_cast<float>(((static_cast<float>(mts.tv_sec)) * nanoSeconds + mts.tv_nsec) / nanoToSecond));
    if (len < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: entering parent_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    if (write(fd, buffer, len) < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: writing parent_ts error, errno(%{public}d)", errno);
    }
    close(fd);
    return true;
}

bool IsNumber(const std::string& str)
{
    if (str.empty()) {
        return false;
    }
    for (auto c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}

int GetCpuProcessors()
{
    int processors = 0;
    processors = sysconf(_SC_NPROCESSORS_CONF);
    return (processors == 0) ? 1 : processors;
}

uint64_t GetCurBootTime()
{
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    return static_cast<uint64_t>(bts.tv_sec * S_TO_NS + bts.tv_nsec);
}

void ReadCurrentCpuFrequencies(std::string& freqs)
{
    int cpuNum = GetCpuProcessors();
    std::ifstream file;
    std::string line;
    for (int i = 0; i < cpuNum; ++i) {
        std::string freq = "0";
        std::string cpuFreqPath = CPUFREQ_PREFIX + std::to_string(i) + CPUFREQ_AFTERFIX;
        file.open(cpuFreqPath);
        if (file.is_open()) {
            if (std::getline(file, line)) {
                freq = line;
            }
            file.close();
        }
        freqs += "cpu_id=" + std::to_string(i) + " state=" + freq;
        if (i != cpuNum - 1) {
            freqs += ",";
        }
    }
}

bool SetPropertyInner(const std::string& property, const std::string& value)
{
    bool result = OHOS::system::SetParameter(property, value);
    if (!result) {
        fprintf(stderr, "Error: Failed to set %s property.\n", property.c_str());
    }
    return result;
}

std::string GetPropertyInner(const std::string& property, const std::string& value)
{
    return OHOS::system::GetParameter(property, value);
}

bool IsHmKernel()
{
    bool isHM = false;
    utsname unameBuf;
    if ((uname(&unameBuf)) == 0) {
        std::string osRelease = unameBuf.release;
        isHM = osRelease.find("HongMeng") != std::string::npos;
    }
    return isHM;
}

bool IsDeveloperMode()
{
    return OHOS::system::GetBoolParameter("const.security.developermode.state", false);
}

bool IsRootVersion()
{
    return OHOS::system::GetBoolParameter("const.debuggable", false);
}

bool IsTraceMounted(std::string& traceRootPath)
{
    if (access((DEBUGFS_TRACING_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        traceRootPath = DEBUGFS_TRACING_DIR;
        return true;
    }
    if (access((TRACEFS_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        traceRootPath = TRACEFS_DIR;
        return true;
    }
    return false;
}

std::string GetFilePath(const std::string& fileName, const std::string& traceRootPath)
{
    return traceRootPath + fileName;
}

std::string ReadFileInner(const std::string& filename)
{
    std::string resolvedPath = CanonicalizeSpecPath(filename.c_str());
    std::ifstream fileIn(resolvedPath.c_str());
    if (!fileIn.is_open()) {
        HILOG_ERROR(LOG_CORE, "ReadFile: %{public}s open failed.", filename.c_str());
        return "";
    }
    std::string str((std::istreambuf_iterator<char>(fileIn)), std::istreambuf_iterator<char>());
    fileIn.close();
    return str;
}

std::string ReadFile(const std::string& filename, const std::string& traceRootPath)
{
    std::string filePath = GetFilePath(filename, traceRootPath);
    return ReadFileInner(filePath);
}

bool IsTracingOn(const std::string& traceRootPath)
{
    const std::string enable = "1";
    if (ReadFile(TRACING_ON_NODE, traceRootPath).substr(0, enable.size()) == enable) {
        HILOG_INFO(LOG_CORE, "tracing_on is 1.");
        return true;
    }
    HILOG_INFO(LOG_CORE, "tracing_on is 0.");
    return false;
}

bool StringToInt(const std::string &str, int &val)
{
    char *endPtr = nullptr;
    errno = 0;
    long num = std::strtol(str.c_str(), &endPtr, OHOS::HiviewDFX::Hitrace::DECIMAL_SCALE);
    if (endPtr == str.c_str() || *endPtr != '\0' || errno != 0 || num > INT_MAX || num < INT_MIN) {
        HILOG_ERROR(LOG_CORE, "get int failed, str: %s", str.c_str());
        return false;
    }
    val = static_cast<int>(num);
    return true;
}

bool StringToInt64(const std::string &str, int64_t &val)
{
    char *endPtr = nullptr;
    errno = 0;
    int64_t num = std::strtoll(str.c_str(), &endPtr, OHOS::HiviewDFX::Hitrace::DECIMAL_SCALE);
    if (endPtr == str.c_str() || *endPtr != '\0' || errno != 0 || num > LLONG_MAX || num < LLONG_MIN) {
        HILOG_ERROR(LOG_CORE, "get int64 failed, str: %s", str.c_str());
        return false;
    }
    val = num;
    return true;
}

bool StringToUint64(const std::string &str, uint64_t &val)
{
    char *endPtr = nullptr;
    errno = 0;
    uint64_t num = std::strtoull(str.c_str(), &endPtr, OHOS::HiviewDFX::Hitrace::DECIMAL_SCALE);
    if (endPtr == str.c_str() || *endPtr != '\0' || errno != 0 || num > ULLONG_MAX || str.c_str()[0] == '-') {
        HILOG_ERROR(LOG_CORE, "get uint64 failed, str: %s", str.c_str());
        return false;
    }
    val = num;
    return true;
}

bool StringToDouble(const std::string &str, double &val)
{
    char *endPtr = nullptr;
    errno = 0;
    double num = std::strtod(str.c_str(), &endPtr);
    if (endPtr == str.c_str() || *endPtr != '\0' || errno != 0) {
        HILOG_ERROR(LOG_CORE, "get double failed, str: %s", str.c_str());
        return false;
    }
    val = num;
    return true;
}

void WriteEventFile(const std::string& srcPath, const int fd)
{
    uint8_t buffer[PAGE_SIZE] = {0};
    std::string srcSpecPath = CanonicalizeSpecPath(srcPath.c_str());
    int srcFd = open(srcSpecPath.c_str(), O_RDONLY);
    if (srcFd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteEventFile: open %{public}s failed.", srcPath.c_str());
        return;
    }
    int64_t readLen = 0;
    do {
        int64_t len = read(srcFd, buffer, PAGE_SIZE);
        if (len <= 0) {
            break;
        }
        ssize_t writeRet = write(fd, buffer, len);
        if (writeRet < 0) {
            HILOG_ERROR(LOG_CORE, "WriteEventFile: write failed, err(%{public}s)", strerror(errno));
            break;
        }
        readLen += len;
    } while (true);
    close(srcFd);
    HILOG_INFO(LOG_CORE, "WriteEventFile end, path: %{public}s, data size: (%{public}" PRIu64 ").",
        srcPath.c_str(), static_cast<uint64_t>(readLen));
}

std::string GetKernelVersion()
{
    utsname unameBuf;
    if (uname(&unameBuf) == 0) {
        return unameBuf.release;
    } else {
        HILOG_ERROR(LOG_CORE, "GetKernelVersion failed, errno: %{public}s", strerror(errno));
        return "";
    }
}

uint64_t GetRemainingSpace(const std::string& path)
{
    struct statvfs fsInfo;

    if (statvfs(path.c_str(), &fsInfo) != 0) {
        HILOG_ERROR(LOG_CORE, "GetRemainingSpace: statvfs failed, errno: %{public}d", errno);
        return UINT64_MAX;
    }

    uint64_t ret = fsInfo.f_bavail * static_cast<uint64_t>(fsInfo.f_frsize);
    HILOG_INFO(LOG_CORE, "GetRemainingSpace: %{public}s current remaining space %{public}" PRIu64, path.c_str(), ret);
    return ret;
}

int GetMemInfoByName(const char* name, const char* path)
{
    int memKB = 0;
    std::ifstream file(path);
    if (!file.is_open()) {
        return memKB;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(name) != std::string::npos) {
            auto firstDigitIt = std::find_if(line.begin(), line.end(), ::isdigit);
            auto lastDigitIt = std::find_if_not(firstDigitIt, line.end(), ::isdigit);
            std::string memTotalStr(firstDigitIt, lastDigitIt);
            if (!StringToInt(memTotalStr, memKB)) {
                HILOG_WARN(LOG_CORE, "get %{public}s failed", name);
            }
            break;
        }
    }
    file.close();
    return memKB;
}

bool IsProcessExist(const pid_t pid)
{
    if (pid <= 0) {
        HILOG_WARN(LOG_CORE, "IsProcessExist: pid %{public}d is invalid", pid);
        return false;
    }
    int status;
    pid_t result = TEMP_FAILURE_RETRY(waitpid(pid, &status, WNOHANG));
    if (result == 0) {
        HILOG_INFO(LOG_CORE, "IsProcessExist: %{public}d process exist", pid);
        return true;
    } else if (result == pid) {
        HILOG_WARN(LOG_CORE, "IsProcessExist: %{public}d process is a zombie, cleaning up", pid);
        return false;
    } else if (errno == ECHILD) {
        HILOG_WARN(LOG_CORE, "IsProcessExist: %{public}d process not exist", pid);
        return false;
    } else {
        HILOG_WARN(LOG_CORE, "IsProcessExist: waitpid failed with error: %s", strerror(errno));
        return false;
    }
}

std::vector<std::string> GetNoFilterEvents(const std::vector<std::string>& enablePath)
{
    std::vector<std::string> noFilterEvents;
    for (auto str : enablePath) {
        char tag[BUFFER_LEN] = {0};
        char event[BUFFER_LEN] = {0};
        int ret = sscanf_s(str.c_str(), "%*[^/]/%[^/]/%[^/]/%*s", tag, sizeof(tag), event, sizeof(event));
        if (ret == EXPECT_NUM) {
            std::string res = std::string(tag) + ":" + std::string(event);
            noFilterEvents.push_back(res);
        } else {
            HILOG_WARN(LOG_CORE, "enablePath maybe illegal");
        }
    }
    return noFilterEvents;
}

bool IsTraceFilePathLegal(const std::string& fileName, char *realFilePath, size_t bufLen)
{
    if (fileName.size() >= bufLen || realpath(fileName.c_str(), realFilePath) == nullptr) {
        HILOG_ERROR(LOG_CORE, "illegal file path");
        return false;
    }
    realFilePath[bufLen - 1] = '\0';
    std::string pathStr(realFilePath);
    if (pathStr.substr(0, TRACE_FILE_DEFAULT_DIR.size()) != TRACE_FILE_DEFAULT_DIR) {
        HILOG_ERROR(LOG_CORE, "file not at hitrace dir");
        return false;
    }
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
