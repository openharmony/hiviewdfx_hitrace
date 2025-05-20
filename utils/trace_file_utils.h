/*
 * Copyright (C) 2024-2025 Huawei Device Co., Ltd.
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

#ifndef TRACE_FILE_UTILS_H
#define TRACE_FILE_UTILS_H

#include <map>
#include <string>
#include <vector>

#include "hitrace_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
struct FileWithTime {
    std::string filename;
    time_t ctime;
    uint64_t fileSize;
    bool isNewFile = true;

    explicit FileWithTime(const std::string& name);
    FileWithTime(const std::string& name, time_t time, uint64_t size, bool newFile);
};

struct TraceFileInfo {
    std::string filename;
    uint64_t traceStartTime; // in ms
    uint64_t traceEndTime; // in ms
    uint64_t fileSize;
};

void GetTraceFilesInDir(std::vector<FileWithTime>& fileList, TRACE_TYPE traceType);
bool RemoveFile(const std::string& fileName);
std::string GenerateTraceFileName(TRACE_TYPE traceType);
void DelSnapshotTraceFile(const int keepFileCount, std::vector<TraceFileInfo>& traceFileVec,
    const uint64_t fileLimitSizeKb);
void DelOldRecordTraceFile(std::vector<FileWithTime>& fileList, const int fileLimit, const uint64_t fileLimitSizeKb);
void ClearOldTraceFile(std::vector<FileWithTime>& fileLists, const int fileLimit, const uint64_t fileLimitSizeKb);
void DelSavedEventsFormat();
void ClearCacheTraceFileByDuration(std::vector<TraceFileInfo>& cacheFileVec);
void ClearCacheTraceFileBySize(std::vector<TraceFileInfo>& cacheFileVec, const uint64_t& fileSizeLimit);
bool GetFileSize(const std::string& filePath, uint64_t& fileSize);
uint64_t GetCurUnixTimeMs();
void RefreshTraceVec(std::vector<TraceFileInfo>& traceVec, const TRACE_TYPE traceType);
std::string RenameCacheFile(const std::string& cacheFile);
bool SetFileInfo(const std::string outPath, const uint64_t& firstPageTimestamp,
    const uint64_t& lastPageTimestamp, TraceFileInfo& traceFileInfo);
} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_FILE_UTILS_H