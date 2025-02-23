/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#ifndef HITRACE_DUMP_H
#define HITRACE_DUMP_H

#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
constexpr uint64_t DEFAULT_TRACE_SLICE_DURATION = 10;
constexpr uint64_t DEFAULT_TOTAL_CACHE_FILE_SIZE = 800;

enum TraceErrorCode : uint8_t {
    SUCCESS = 0,
    TRACE_NOT_SUPPORTED = 1,
    TRACE_IS_OCCUPIED = 2,
    TAG_ERROR = 3,
    FILE_ERROR = 4,
    WRITE_TRACE_INFO_ERROR = 5,
    WRONG_TRACE_MODE = 6,
    OUT_OF_TIME = 7,
    FORK_ERROR = 8,
    INVALID_MAX_DURATION = 9,
    EPOLL_WAIT_ERROR = 10,
    PIPE_CREATE_ERROR = 11,
    SYSINFO_READ_FAILURE = 12,
    UNSET = 255,
};

enum TraceMode : uint8_t {
    CLOSE = 0,
    OPEN = 1 << 0,
    RECORD = 1 << 1,
    CACHE = 1 << 2,
};

struct TraceRetInfo {
    TraceErrorCode errorCode;
    std::vector<std::string> outputFiles;
    int32_t coverRatio = 0;
    int32_t coverDuration = 0;
    std::vector<std::string> tags;
};

#ifdef HITRACE_UNITTEST
void SetSysInitParamTags(uint64_t sysInitParamTags);
bool SetCheckParam();
#endif

/**
 * Get the current trace mode.
*/
uint8_t GetTraceMode();

/**
 * Open trace with customized args.
*/
TraceErrorCode OpenTrace(const std::string& args);

/**
 * Open trace by tag groups using default parameters.
 * Default parameters: buffersize = 144MB, clockType = boot, overwrite = true.
*/
TraceErrorCode OpenTrace(const std::vector<std::string>& tagGroups);

/**
 * Reading trace data once from ftrace ringbuffer in the kernel.
 * Using child processes to process trace tasks.
 * happenTime: the retrospective starting time stamp of target trace.
 * ----If happenTime = 0, it is not set.
 * return TraceErrorCode::SUCCESS if any trace is captured between the designated interval
 * return TraceErrorCode::OUT_OF_TIME otherwise.
 * maxDuration: the maximum time(s) allowed for the trace task.
 * ---- If maxDuration is 0, means that is no limit for the trace task.
 * ---- If maxDuration is less than 0, it is illegal input parameter.
*/
TraceRetInfo DumpTrace(int maxDuration = 0, uint64_t happenTime = 0);

/**
 * Enable sub threads to periodically drop disk trace data.
 * End the periodic disk drop task until the next call to RecordTraceOff().
*/
TraceErrorCode RecordTraceOn();

/**
 * End the periodic disk drop task.
*/
TraceRetInfo RecordTraceOff();

/**
 * Enable sub threads to periodically dump cache data
 * End dumping with CacheTraceOff()
 * CacheTraceOn() function call during the caching phase will return corresponding cache files.
*/
TraceErrorCode CacheTraceOn(uint64_t totalFileSize = DEFAULT_TOTAL_CACHE_FILE_SIZE,
    uint64_t sliceMaxDuration = DEFAULT_TRACE_SLICE_DURATION);

/**
 * End the periodic cache task.
*/
TraceErrorCode CacheTraceOff();

/**
 * Turn off trace mode.
*/
TraceErrorCode CloseTrace();

/**
 * Get g_traceFilesTable.
*/
std::vector<std::pair<std::string, int>> GetTraceFilesTable();

/**
 * Set g_traceFilesTable.
*/
void SetTraceFilesTable(const std::vector<std::pair<std::string, int>>& traceFilesTable);
} // Hitrace

}
}

#endif // HITRACE_DUMP_H