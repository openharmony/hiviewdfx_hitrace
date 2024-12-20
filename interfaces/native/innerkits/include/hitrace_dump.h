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
    CMD_MODE = 1,
    SERVICE_MODE = 2,
    RECORDING_MODE = CMD_MODE,
    SNAPSHOT_MODE = SERVICE_MODE,
};

struct TraceRetInfo {
    TraceErrorCode errorCode;
    std::vector<std::string> outputFiles;
};

#ifdef HITRACE_UNITTEST
void SetSysInitParamTags(uint64_t sysInitParamTags);
bool SetCheckParam();
#endif

/**
 * Get the current trace mode.
*/
TraceMode GetTraceMode();

/**
 * Set trace parameters based on args for CMD_MODE.
*/
TraceErrorCode OpenTrace(const std::string &args);

/**
 * Set trace tags based on tagGroups for SERVICE_MODE.
*/
TraceErrorCode OpenTrace(const std::vector<std::string> &tagGroups);

/**
 * Reading trace data once from ftrace ringbuffer in the kernel.
 * Using child processes to process trace tasks.
*/
TraceRetInfo DumpTrace();

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
TraceRetInfo DumpTrace(int maxDuration, uint64_t happenTime = 0);

/**
 * Enable sub threads to periodically drop disk trace data.
 * End the periodic disk drop task until the next call to DumpTraceOff().
*/
TraceErrorCode DumpTraceOn();

/**
 * End the periodic disk drop task.
*/
TraceRetInfo DumpTraceOff();

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