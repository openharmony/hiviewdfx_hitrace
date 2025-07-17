/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#ifndef HITRACE_DEFINE_H
#define HITRACE_DEFINE_H

#include <inttypes.h>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
constexpr int TRACE_FILE_LEN = 128;

enum TraceMode : uint8_t {
    CLOSE = 0,
    OPEN = 1 << 0,
    RECORD = 1 << 1,
    CACHE = 1 << 2,
};

enum TRACE_TYPE : uint8_t {
    TRACE_SNAPSHOT = 0,
    TRACE_RECORDING = 1,
    TRACE_CACHE = 2,
};

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
    ASYNC_DUMP = 12,
    TRACE_TASK_SUBMIT_ERROR = 13,
    TRACE_TASK_DUMP_TIMEOUT = 14,
    UNSET = 255,
};

struct TraceDumpRequest {
    TRACE_TYPE type;
    int fileSize; // bytes
    bool limitFileSz;
    uint64_t traceStartTime;
    uint64_t traceEndTime;
    uint64_t taskId;
};

struct TraceRetInfo {
    TraceErrorCode errorCode;
    uint8_t mode = 0;
    bool isOverflowControl = false;
    std::vector<std::string> outputFiles;
    int64_t fileSize = 0;
    int32_t coverRatio = 0;
    int32_t coverDuration = 0;
    std::vector<std::string> tags;
};

enum class TraceDumpStatus : uint8_t {
    START = 0,
    READ_DONE,
    WAIT_WRITE,
    WRITE_DONE,
    FINISH
};

struct TraceDumpTask {
    uint64_t time = 0;
    uint64_t traceStartTime = 0;
    uint64_t traceEndTime = 0;
    char outputFile[TRACE_FILE_LEN] = { 0 };
    int64_t fileSize = 0;
    int64_t fileSizeLimit = 0;
    bool hasSyncReturn = false;
    TraceErrorCode code = TraceErrorCode::UNSET;
    bool isFileSizeOverLimit = false;
    TraceDumpStatus status = TraceDumpStatus::START;
};

struct AgeingParam {
    bool rootEnable = true;
    int64_t fileNumberLimit = 0;
    int64_t fileSizeKbLimit = 0;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // HITRACE_DEFINE_H