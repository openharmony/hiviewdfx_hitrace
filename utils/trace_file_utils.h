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

#ifndef TRACE_FILE_UTILS_H
#define TRACE_FILE_UTILS_H

#include <map>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
enum TRACE_TYPE : uint8_t {
    TRACE_SNAPSHOT = 0,
    TRACE_RECORDING = 1,
    TRACE_CACHE = 2,
};
struct FileWithTime {
    std::string filename;
    time_t ctime;
    uint64_t fileSize;
};

struct TraceFileInfo {
    std::string filename;
    uint64_t traceStartTime; // in ms
    uint64_t traceEndTime; // in ms
    uint64_t fileSize;
};

bool RemoveFile(const std::string& fileName);
std::string GenerateTraceFileName(TRACE_TYPE traceType);
void DelSnapshotTraceFile(const int& keepFileCount, std::vector<TraceFileInfo>& traceFileVec);
void DelOldRecordTraceFile(const int& fileLimit);
void ClearOldTraceFile(std::vector<std::string>& fileLists, const int& fileLimit);
void DelSavedEventsFormat();
void ClearCacheTraceFileByDuration(std::vector<TraceFileInfo>& cacheFileVec);
void ClearCacheTraceFileBySize(std::vector<TraceFileInfo>& cacheFileVec, const uint64_t& fileSizeLimit);
bool GetFileSize(const std::string& filePath, uint64_t& fileSize);
uint64_t GetCurUnixTimeMs();
uint64_t ConvertPageTraceTimeToUtTimeMs(const uint64_t& pageTraceTime);
bool RenameTraceFile(const std::string& fileName, std::string& newFileName,
    const uint64_t& firstPageTraceTime, const uint64_t& lastPageTraceTime);
void RefreshTraceVec(std::vector<TraceFileInfo>& traceVec, const TRACE_TYPE traceType);
} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_FILE_UTILS_H