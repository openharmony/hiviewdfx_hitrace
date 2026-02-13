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

#include "hitrace_option_util.h"

#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <string>
#include <unistd.h>

#include "hilog/log.h"

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
#define LOG_TAG "HitraceUtil"
#endif

constexpr auto DEBUGFS_TRACING_DIR = "/sys/kernel/debug/tracing/";
constexpr auto TRACEFS_DIR = "/sys/kernel/tracing/";
constexpr auto TRACE_MARKER_NODE = "trace_marker";

void FILECloser(FILE* fp)
{
    if (fp != nullptr) {
        fclose(fp);
    }
}

void DirCloser(DIR* dir)
{
    if (dir != nullptr) {
        closedir(dir);
    }
}

void CharDeleter(char** buff)
{
    if (buff != nullptr && *buff != nullptr) {
        free(*buff);
    }
}
}

FileLock::FileLock(const std::string& filename, int flags)
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

FileLock::~FileLock()
{
    if (fd_) {
#ifdef ENABLE_LOCK
        flock(fd_, LOCK_UN);
        HILOG_INFO(LOG_CORE, "FileLock: %{public}d unlock succ", fd_);
#endif
    }
}

int FileLock::Fd() const
{
    return fd_.GetFd();
}

bool AppendToFile(const std::string& filename, const std::string& str)
{
    FileLock fileLock(filename, O_RDWR);
    if (write(fileLock.Fd(), str.c_str(), str.size()) < 0) {
        HILOG_ERROR(LOG_CORE, "AppendToFile: %{public}s write failed %{public}d", filename.c_str(), errno);
        return false;
    }
    return true;
}

bool TraverseFileLineByLine(const std::string &filePath,
    const std::function<bool(const char* lineContent, size_t lineNum)> &lineHandler)
{
    if (filePath.empty() || lineHandler == nullptr) {
        return false;
    }
    const auto fp = std::unique_ptr<FILE, void(*)(FILE*)>(fopen(filePath.c_str(), "r"), FILECloser);
    if (fp == nullptr) {
        return false;
    }
    char* lineBuf = nullptr;
    auto lineBufRelease = std::unique_ptr<char*, void(*)(char**)>(&lineBuf, CharDeleter);
    size_t bufSize = 0;
    int lineNum = 0;
    while (getline(&lineBuf, &bufSize, fp.get()) != -1) {
        lineNum++;
        if (!lineHandler(lineBuf, lineNum)) {
            return true;
        }
    }
    if (!feof(fp.get())) {
        return false;
    }
    return true;
}

bool GetSubThreadIds(pid_t pid, std::vector<std::string>& subThreadIds)
{
    return GetSubThreadIds(std::to_string(pid), subThreadIds);
}

bool GetSubThreadIds(const std::string& pid, std::vector<std::string>& subThreadIds)
{
    const std::string dirPath = "/proc/" + pid + "/task/";
    const auto dir = std::unique_ptr<DIR, void(*)(DIR*)>(opendir(dirPath.c_str()), DirCloser);
    if (!dir) {
        HILOG_ERROR(LOG_CORE, "failed open dirpath %{public}s for %{public}d", dirPath.c_str(), errno);
        return false;
    }
    for (dirent* entry = readdir(dir.get()); entry != nullptr; entry = readdir(dir.get())) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        subThreadIds.emplace_back(entry->d_name);
    }
    return true;
}

const std::string& GetTraceRootPath()
{
    static const std::string traceRootPath = [] {
        if (access((std::string(DEBUGFS_TRACING_DIR) + std::string(TRACE_MARKER_NODE)).c_str(), F_OK) != -1) {
            return DEBUGFS_TRACING_DIR;
        }
        if (access((std::string(TRACEFS_DIR) + std::string(TRACE_MARKER_NODE)).c_str(), F_OK) != -1) {
            return TRACEFS_DIR;
        }
        return "";
    }();
    return traceRootPath;
}

bool AppendTracePoint(const std::string& relativePath, const std::string& pidContent)
{
    const std::string& traceRootPath = GetTraceRootPath();
    if (traceRootPath.empty()) {
        return false;
    }
    return AppendToFile(traceRootPath + relativePath, pidContent);
}

bool ClearTracePoint(const std::string& relativePath)
{
    const std::string& traceRootPath = GetTraceRootPath();
    if (traceRootPath.empty()) {
        return false;
    }
    const std::string absolutePath = traceRootPath + relativePath;
    if (SmartFd(creat(absolutePath.c_str(), 0))) {
        HILOG_INFO(LOG_CORE, "success clear target trace point %{public}s", relativePath.c_str());
        return true;
    }
    return false;
}
}
}
}
