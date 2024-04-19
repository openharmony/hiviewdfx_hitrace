/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_HIVIEWDFX_HITRACE_COMMON_UTILS_H
#define OHOS_HIVIEWDFX_HITRACE_COMMON_UTILS_H

#include <string>
#include <map>
#include <vector>
#include "hilog/log.h"

using OHOS::HiviewDFX::HiLog;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

struct TagCategory {
    std::string description;
    uint64_t tag;
    int type;
    std::vector<std::string> sysFiles;
};

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "HitraceInfo"

/**
 * Canonicalization and verification of file paths.
*/
std::string CanonicalizeSpecPath(const char* src);

/**
 * Clock sync: Use trace to mark the real-time and monotonic at this moment
 * Example of trace output:
 *  ...... 88.767049: tracing_mark_write: trace_event_clock_sync: realtime_ts=1501929219561
 *  ...... 88.767072: tracing_mark_write: trace_event_clock_sync: parent_ts=88.767036
*/
bool MarkClockSync(const std::string& traceRootPath);

/**
 * Parse trace tags info from /system/etc/hiview/hitrace_utils.json
*/
bool ParseTagInfo(std::map<std::string, TagCategory> &allTags,
                  std::map<std::string, std::vector<std::string>> &tagGroupTable);

bool IsNumber(const std::string &str);

} // Hitrace
} // HiviewDFX
} // OHOS

#endif // OHOS_HIVIEWDFX_HITRACE_COMMON_UTILS_H