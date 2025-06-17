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

#include "hitrace_define.h"

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

class TraceJsonParser {
public:
    static TraceJsonParser& Instance();

    const std::map<std::string, TraceTag>& GetAllTagInfos() const { return traceTagInfos_; }
    const std::map<std::string, std::vector<std::string>>& GetTagGroups() const { return tagGroups_; }
    const std::vector<std::string>& GetBaseFmtPath() const { return baseTraceFormats_; }

    const AgeingParam& GetAgeingParam(TRACE_TYPE type) const;

    int GetSnapshotDefaultBufferSizeKb() const { return snapshotBufSzKb_; }
private:
    std::map<std::string, TraceTag> traceTagInfos_ = {};
    std::map<std::string, std::vector<std::string>> tagGroups_ = {};
    std::vector<std::string> baseTraceFormats_ = {};

    int snapshotBufSzKb_ = 0;

    AgeingParam snapShotAgeingParam_ = {};
    AgeingParam recordAgeingParam_ = {};

    TraceJsonParser(const std::string& hitraceUtilsJson, const std::string& productConfigJson);

    void InitSnapshotDefaultBufferSize();
    void InitAgeingParam();

    void ParseHitraceUtilsJson(const std::string& hitraceUtilsJson);
    void ParseProductConfigJson(const std::string& productConfigJson);

    void PrintParseResult();

    ~TraceJsonParser() = default;
    TraceJsonParser(TraceJsonParser&) = delete;
    TraceJsonParser(TraceJsonParser&&) = delete;
    TraceJsonParser& operator=(const TraceJsonParser&) = delete;
    TraceJsonParser&& operator=(const TraceJsonParser&&) = delete;
};
} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_JSON_PARSER_H