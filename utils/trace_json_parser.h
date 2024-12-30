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

#ifndef TRACE_JSON_PARSER_H
#define TRACE_JSON_PARSER_H

#include <map>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
enum TraceType {
    USER = 0,
    KERNEL
};

struct TraceTag {
    std::string description;
    uint64_t tag;
    TraceType type;
    std::vector<std::string> enablePath;
    std::vector<std::string> formatPath;
};

enum TraceJsonInfo : uint8_t {
    TRACE_SNAPSHOT_BUFSZ = 1,
    TRACE_TAG_BASE_INFO = 1 << 1,
    TRACE_TAG_ENABLE_INFO = 1 << 2,
    TRACE_TAG_FORMAT_INFO = 1 << 3,
    TRACE_TAG_GROUP_INFO = 1 << 4,
    TRACE_RECORD_FILE_AGE = 1 << 5,
    TRACE_SNAPSHOT_FILE_AGE = 1 << 6,
};

enum ParsePolicy : uint8_t {
    PARSE_NONE = 0,
    PARSE_TRACE_BUFSZ_INFO = TRACE_SNAPSHOT_BUFSZ,
    PARSE_TRACE_BASE_INFO = TRACE_TAG_BASE_INFO,
    PARSE_TRACE_ENABLE_INFO = TRACE_TAG_BASE_INFO | TRACE_TAG_ENABLE_INFO,
    PARSE_TRACE_FORMAT_INFO = TRACE_TAG_BASE_INFO | TRACE_TAG_FORMAT_INFO,
    PARSE_TRACE_GROUP_INFO = PARSE_TRACE_ENABLE_INFO | PARSE_TRACE_FORMAT_INFO | TRACE_TAG_GROUP_INFO,
    PARSE_ALL_INFO = PARSE_TRACE_GROUP_INFO | TRACE_SNAPSHOT_BUFSZ,
};

class TraceJsonParser {
public:
    TraceJsonParser() = default;
    bool ParseTraceJson(const uint8_t policy);
    std::map<std::string, TraceTag>& GetAllTagInfos() { return traceTagInfos_; }
    std::map<std::string, std::vector<std::string>>& GetTagGroups() { return tagGroups_; }
    std::vector<std::string> GetBaseFmtPath() { return baseTraceFormats_; }
    int GetSnapShotBufSzKb() { return snapshotBufSzKb_; }
    bool GetRecordFileAge() { return recordFileAge_; }
    bool GetSnapShotFileAge() { return snapshotFileAge_; }

private:
    uint8_t parserState_ = PARSE_NONE;
    std::map<std::string, TraceTag> traceTagInfos_ = {};
    std::map<std::string, std::vector<std::string>> tagGroups_ = {};
    std::vector<std::string> baseTraceFormats_ = {};
    int snapshotBufSzKb_ = 0;
    bool recordFileAge_ = true;
    bool snapshotFileAge_ = true;
};
} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_JSON_PARSER_H