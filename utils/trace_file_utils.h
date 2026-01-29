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
#include <set>
#include <string>
#include <vector>

#include "hitrace_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
struct TraceFileInfo {
    std::string filename = "";
    uint64_t traceStartTime = 0; // in ms
    uint64_t traceEndTime = 0; // in ms
    int64_t fileSize = 0;
    time_t ctime = 0;
    bool isNewFile = true;  // only used in record mode

    TraceFileInfo() = default;
    explicit TraceFileInfo(const std::string& name);
    TraceFileInfo(const std::string& name, time_t time, int64_t sizekB, bool newFile);
};

void GetTraceFilesInDir(std::vector<TraceFileInfo>& fileList, TraceDumpType traceType);
void GetTraceFileNamesInDir(std::set<std::string>& fileSet, TraceDumpType traceType);
bool RemoveFile(const std::string& fileName);
std::string GenerateTraceFileName(TraceDumpType traceType);
std::string GenerateTraceFileNameByTraceTime(TraceDumpType traceType,
    const uint64_t& firstPageTraceTime, const uint64_t& lastPageTraceTime);
void DelSavedEventsFormat();
void ClearCacheTraceFileByDuration(std::vector<TraceFileInfo>& cacheFileVec);
void ClearCacheTraceFileBySize(std::vector<TraceFileInfo>& cacheFileVec, const uint64_t& fileSizeLimit);
off_t GetFileSize(const std::string& filePath);
uint64_t GetCurUnixTimeMs();
void RefreshTraceVec(std::vector<TraceFileInfo>& traceVec, const TraceDumpType traceType);
std::string RenameCacheFile(const std::string& cacheFile);
bool SetFileInfo(const bool isFileExist, const std::string outPath, const uint64_t& firstPageTimestamp,
    const uint64_t& lastPageTimestamp, TraceFileInfo& traceFileInfo);
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_FILE_UTILS_H