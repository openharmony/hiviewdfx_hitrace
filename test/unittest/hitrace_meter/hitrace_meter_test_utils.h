/*
 * Copyright (C) 2022-2024 Huawei Device Co., Ltd.
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

#ifndef HITRACE_METER_TEST_UTILS_H
#define HITRACE_METER_TEST_UTILS_H

#include <fstream>
#include <hilog/log.h>

#ifdef HITRACE_METER_SDK_C
#include "trace.h"
#else
#include "hitrace_meter.h"
#include "hitrace/hitraceid.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
constexpr int RECORD_SIZE_MAX = 1024;

struct TraceInfo {
    char type;
#ifdef HITRACE_METER_SDK_C
    HiTrace_Output_Level level;
#else
    HiTraceOutputLevel level;
#endif
    uint64_t tag;
    int64_t value;
    const char* name;
    const char* customCategory;
    const char* customArgs;
    HiTraceId* hiTraceId = nullptr;
};

bool Init(const char (&pid)[6]);
bool CleanTrace();
bool CleanFtrace();
bool SetFtrace(const std::string& filename, bool enabled);
bool SetFtrace(const std::string& filename, uint64_t value);
std::vector<std::string> ReadTrace(std::string filename = "");
bool FindResult(std::string record, const std::vector<std::string>& list);
bool GetTraceResult(TraceInfo& traceInfo, const std::vector<std::string>& list,
    char (&record)[RECORD_SIZE_MAX + 1]);
}
}
}
#endif
