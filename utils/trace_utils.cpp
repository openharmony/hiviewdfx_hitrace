/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#include "trace_utils.h"

#include <cinttypes>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
const std::string TRACE_DEFAULT_DIR = "/data/log/hitrace/";
const std::string TRACE_EVENT_FORMT = "saved_events_format";
const std::string TRACE_SNAPSHOT_PREFIX = "trace_";
const std::string TRACE_RECORDING_PREFIX = "record_trace_";
const size_t DEFAULT_TRACE_FILE_LIMIT = 15;

struct FileWithTime {
    std::string filename;
    time_t ctime;
};

void RemoveFile(const std::string& fileName)
{
    int fd = open(fileName.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        HILOG_WARN(LOG_CORE, "RemoveFile :: open file failed: %{public}s", fileName.c_str());
        return;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        HILOG_WARN(LOG_CORE, "RemoveFile :: get file lock failed, skip remove: %{public}s", fileName.c_str());
        return;
    }
    if (remove(fileName.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "RemoveFile :: Delete %{public}s success.", fileName.c_str());
    } else {
        HILOG_WARN(LOG_CORE, "RemoveFile :: Delete %{public}s failed.", fileName.c_str());
    }
    flock(fd, LOCK_UN);
    close(fd);
}

void GetRecordTraceFilesInDir(std::vector<FileWithTime>& fileList)
{
    struct stat fileStat;
    for (const auto &entry : std::filesystem::directory_iterator(TRACE_DEFAULT_DIR)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        if (fileName.substr(0, TRACE_RECORDING_PREFIX.size()) == TRACE_RECORDING_PREFIX) {
            if (stat((TRACE_DEFAULT_DIR + fileName).c_str(), &fileStat) == 0) {
                fileList.push_back({fileName, fileStat.st_ctime});
            }
        }
    }
    std::sort(fileList.begin(), fileList.end(), [](const FileWithTime& a, const FileWithTime& b) {
        return a.ctime < b.ctime;
    });
}
}

std::string GenerateTraceFileName(bool isSnapshot)
{
    // eg: /data/log/hitrace/trace_localtime@boottime.sys
    std::string name = TRACE_DEFAULT_DIR;

    if (isSnapshot) {
        name += TRACE_SNAPSHOT_PREFIX;
    } else {
        name += TRACE_RECORDING_PREFIX;
    }

    // get localtime
    time_t currentTime = time(nullptr);
    struct tm timeInfo = {};
    const int bufferSize = 16;
    char timeStr[bufferSize] = {0};
    if (localtime_r(&currentTime, &timeInfo) == nullptr) {
        HILOG_ERROR(LOG_CORE, "Failed to get localtime.");
        return "";
    }
    (void)strftime(timeStr, bufferSize, "%Y%m%d%H%M%S", &timeInfo);
    name += std::string(timeStr);
    // get boottime
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    name += "@" + std::to_string(bts.tv_sec) + "-" + std::to_string(bts.tv_nsec) + ".sys";

    struct timespec mts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &mts);
    HILOG_INFO(LOG_CORE, "output trace: %{public}s, boot_time(%{public}" PRId64 "), mono_time(%{public}" PRId64 ").",
        name.c_str(), static_cast<int64_t>(bts.tv_sec), static_cast<int64_t>(mts.tv_sec));
    return name;
}

/**
 * When the SERVICE_MODE is started, clear the remaining trace files in the folder.
*/
void DelSnapshotTraceFile(const bool deleteSavedFmt, const int keepFileCount)
{
    if (access(TRACE_DEFAULT_DIR.c_str(), F_OK) != 0) {
        return;
    }
    DIR* dirPtr = opendir(TRACE_DEFAULT_DIR.c_str());
    if (dirPtr == nullptr) {
        HILOG_ERROR(LOG_CORE, "Failed to opendir %{public}s.", TRACE_DEFAULT_DIR.c_str());
        return;
    }
    std::vector<FileWithTime> snapshotTraceFiles;
    struct dirent* ptr = nullptr;
    struct stat fileStat;
    while ((ptr = readdir(dirPtr)) != nullptr) {
        if (ptr->d_type == DT_REG) {
            std::string name = std::string(ptr->d_name);
            if (deleteSavedFmt && name.compare(0, TRACE_EVENT_FORMT.size(), TRACE_EVENT_FORMT) == 0) {
                RemoveFile(TRACE_DEFAULT_DIR + name);
                continue;
            }
            if (name.compare(0, TRACE_SNAPSHOT_PREFIX.size(), TRACE_SNAPSHOT_PREFIX) != 0) {
                continue;
            }
            if (stat((TRACE_DEFAULT_DIR + name).c_str(), &fileStat) == 0) {
                snapshotTraceFiles.push_back({name, fileStat.st_ctime});
            }
        }
    }
    closedir(dirPtr);

    std::sort(snapshotTraceFiles.begin(), snapshotTraceFiles.end(), [](const FileWithTime& a, const FileWithTime& b) {
        return a.ctime < b.ctime;
    });

    int deleteFileCnt = snapshotTraceFiles.size() - keepFileCount;
    for (int i = 0; i < deleteFileCnt && i < snapshotTraceFiles.size(); i++) {
        RemoveFile(TRACE_DEFAULT_DIR + snapshotTraceFiles[i].filename);
    }
}

/**
 * open trace file aging mechanism
 */
void DelOldRecordTraceFile(const std::string& fileLimit)
{
    size_t traceFileLimit = DEFAULT_TRACE_FILE_LIMIT;
    if (!fileLimit.empty()) {
        traceFileLimit = static_cast<size_t>(std::stoi(fileLimit));
    }
    HILOG_INFO(LOG_CORE, "DelOldRecordTraceFile: activate aging mechanism with file limit %{public}zu", traceFileLimit);

    std::vector<FileWithTime> fileList;
    GetRecordTraceFilesInDir(fileList);

    if (fileList.size() <= traceFileLimit) {
        HILOG_INFO(LOG_CORE, "DelOldRecordTraceFile: no record trace file need be deleted.");
        return;
    }

    size_t deleteNum = fileList.size() - traceFileLimit;
    for (int i = 0; i < deleteNum; ++i) {
        if (remove((TRACE_DEFAULT_DIR + fileList[i].filename).c_str()) == 0) {
            HILOG_INFO(LOG_CORE, "DelOldRecordTraceFile: delete first: %{public}s success.",
                fileList[i].filename.c_str());
        } else {
            HILOG_ERROR(LOG_CORE, "DelOldRecordTraceFile: delete first: %{public}s failed, errno: %{public}d.",
                fileList[i].filename.c_str(), errno);
        }
    }
}

void ClearOldTraceFile(std::vector<std::string>& fileLists, const std::string& fileLimit)
{
    if (fileLists.size() <= 0) {
        return;
    }

    size_t traceFileLimit = DEFAULT_TRACE_FILE_LIMIT;
    if (!fileLimit.empty()) {
        traceFileLimit = static_cast<size_t>(std::stoi(fileLimit));
    }
    HILOG_INFO(LOG_CORE, "ClearOldTraceFile: activate aging mechanism with file limit %{public}zu", traceFileLimit);

    if (fileLists.size() > traceFileLimit && access(fileLists[0].c_str(), F_OK) == 0) {
        if (remove(fileLists[0].c_str()) == 0) {
            fileLists.erase(fileLists.begin());
            HILOG_INFO(LOG_CORE, "ClearOldTraceFile: delete first success.");
        } else {
            HILOG_ERROR(LOG_CORE, "ClearOldTraceFile: delete first failed, errno: %{public}d.", errno);
        }
    }
}

/**
 * When the raw trace is started, clear the saved_events_format files in the folder.
 */
void DelSavedEventsFormat()
{
    const std::string savedEventsFormatPath = TRACE_DEFAULT_DIR + TRACE_EVENT_FORMT;
    if (access(savedEventsFormatPath.c_str(), F_OK) != 0) {
        // saved_events_format not exit
        return;
    }
    // saved_events_format exit
    if (remove(savedEventsFormatPath.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "Delete saved_events_format success.");
    } else {
        HILOG_ERROR(LOG_CORE, "Delete saved_events_format failed.");
    }
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
