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
    CALL_ERROR = 6,
};

enum TraceMode : uint8_t {
    CLOSE = 0,
    CMD_MODE = 1,
    SERVICE_MODE = 2,
};

struct TraceRetInfo {
    TraceErrorCode errorCode;
    std::vector<std::string> outputFiles;
};

std::vector<std::pair<std::string, uint64_t>> g_traceFilesTable;

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
*/
TraceRetInfo DumpTrace();

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
} // Hitrace

}
}

#endif // HITRACE_DUMP_H