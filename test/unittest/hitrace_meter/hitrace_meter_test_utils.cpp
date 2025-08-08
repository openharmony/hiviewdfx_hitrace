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

#include <algorithm>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>
#include "common_define.h"
#include "hitrace_meter_test_utils.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceTest"
#endif

constexpr int HITRACEID_LEN = 64;
const char g_traceLevel[4] = {'D', 'I', 'C', 'M'};
static std::string g_traceRootPath;
static char g_pid[6];
const std::string SEPARATOR = "|";

bool Init(const char (&pid)[6])
{
    int ret = strcpy_s(g_pid, sizeof(g_pid), pid);
    if (ret != 0) {
        HILOG_ERROR(LOG_CORE, "pid[%{public}s] strcpy_s fail ret: %{public}d.", pid, ret);
        return false;
    }
    if (access((DEBUGFS_TRACING_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        g_traceRootPath = DEBUGFS_TRACING_DIR;
    } else if (access((TRACEFS_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        g_traceRootPath = TRACEFS_DIR;
    } else {
        HILOG_ERROR(LOG_CORE, "Error: Finding trace folder failed");
        return false;
    }
    return true;
}

bool CleanTrace()
{
    if (g_traceRootPath.empty()) {
        HILOG_ERROR(LOG_CORE, "Error: trace path not found.");
        return false;
    }
    std::ofstream ofs;
    ofs.open(g_traceRootPath + TRACE_NODE, std::ofstream::out);
    if (!ofs.is_open()) {
        HILOG_ERROR(LOG_CORE, "Error: opening trace path failed.");
        return false;
    }
    ofs << "";
    ofs.close();
    return true;
}

static bool WriteStringToFile(const std::string& filename, const std::string& str)
{
    if (access((g_traceRootPath + filename).c_str(), W_OK) == 0) {
        if (g_traceRootPath == "") {
            HILOG_ERROR(LOG_CORE, "Error: trace path not found.");
            return false;
        }
        std::ofstream out;
        out.open(g_traceRootPath + filename, std::ios::out);
        out << str;
        out.close();
        return true;
    }
    return false;
}

bool CleanFtrace()
{
    return WriteStringToFile("events/enable", "0");
}

bool SetFtrace(const std::string& filename, bool enabled)
{
    return WriteStringToFile(filename, enabled ? "1" : "0");
}

static std::stringstream ReadFile(const std::string& filename)
{
    std::stringstream ss;
    char resolvedPath[PATH_MAX] = { 0 };
    if (realpath(filename.c_str(), resolvedPath) == nullptr) {
        fprintf(stderr, "Error: _fullpath %s failed", filename.c_str());
        return ss;
    }
    std::ifstream fin(resolvedPath);
    if (!fin.is_open()) {
        fprintf(stderr, "opening file: %s failed!", filename.c_str());
        return ss;
    }
    ss << fin.rdbuf();
    fin.close();
    return ss;
}

std::vector<std::string> ReadTrace(std::string filename)
{
    if (filename == "") {
        filename = g_traceRootPath + TRACE_NODE;
    }
    std::vector<std::string> list;
    if (access(filename.c_str(), F_OK) != -1) {
        std::stringstream ss = ReadFile(filename);
        std::string line;
        while (getline(ss, line)) {
            list.emplace_back(move(line));
        }
    }
    return list;
}

bool FindResult(std::string record, const std::vector<std::string>& list)
{
    for (int i = list.size() - 1; i >= 0; i--) {
        std::string ele = list[i];
        if (ele.find(record) != std::string::npos) {
            HILOG_INFO(LOG_CORE, "FindResult: %{public}s", ele.c_str());
            return true;
        }
    }
    return false;
}

static std::string GetRecord(const HiTraceId* hiTraceId)
{
    std::string record = "";
    char buf[HITRACEID_LEN] = {0};
#ifdef HITRACE_METER_SDK_C
    int bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "[%llx,%llx,%llx]#",
        OH_HiTrace_GetChainId(hiTraceId), OH_HiTrace_GetSpanId(hiTraceId), OH_HiTrace_GetParentSpanId(hiTraceId));
#else
    int bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "[%llx,%llx,%llx]#",
        (*hiTraceId).GetChainId(), (*hiTraceId).GetSpanId(), (*hiTraceId).GetParentSpanId());
#endif
    if (EXPECTANTLY(bytes > 0)) {
        record += buf;
    }
    std::transform(record.cbegin(), record.cend(), record.begin(), [](unsigned char c) { return tolower(c); });
    return record;
}

static void SetNullptrToEmpty(TraceInfo& traceInfo)
{
    if (traceInfo.name == nullptr) {
        traceInfo.name = "";
    }
    if (traceInfo.customCategory == nullptr) {
        traceInfo.customCategory = "";
    }
    if (traceInfo.customArgs == nullptr) {
        traceInfo.customArgs = "";
    }
}

bool GetTraceResult(TraceInfo& traceInfo, const std::vector<std::string>& list,
    char (&record)[RECORD_SIZE_MAX + 1])
{
    if (list.empty()) {
        return false;
    }
#ifdef HITRACE_METER_SDK_C
    std::string bitStr = "62";
#else
    constexpr int bitStrSize = 7;
    char bitStrC[bitStrSize] = {0};
    ParseTagBits(traceInfo.tag, bitStrC, bitStrSize);
    std::string bitStr = std::string(bitStrC);
#endif
    std::string chainStr = "";
    if (traceInfo.hiTraceId != nullptr) {
        chainStr = GetRecord(traceInfo.hiTraceId);
    }
    SetNullptrToEmpty(traceInfo);
    std::string name = std::string(traceInfo.name);
    std::string customCategory = std::string(traceInfo.customCategory);
    std::string customArgs = std::string(traceInfo.customArgs);
    std::string recordStr = std::string(1, traceInfo.type) + SEPARATOR + g_pid + SEPARATOR;
    if (traceInfo.type == 'E') {
        recordStr += g_traceLevel[traceInfo.level] + bitStr;
    } else {
        recordStr += "H:" + chainStr + name + SEPARATOR;
        if (traceInfo.type == 'B') {
            recordStr += g_traceLevel[traceInfo.level] + bitStr;
            if (customArgs != "") {
                recordStr += SEPARATOR + customArgs;
            }
        } else if (traceInfo.type == 'S') {
            recordStr += std::to_string(traceInfo.value) + SEPARATOR + g_traceLevel[traceInfo.level] + bitStr;
            if (customArgs != "") {
                recordStr += SEPARATOR + customCategory + SEPARATOR + customArgs;
            } else if (customCategory != "") {
                recordStr += SEPARATOR + customCategory;
            }
        } else {
            recordStr += std::to_string(traceInfo.value) + SEPARATOR + g_traceLevel[traceInfo.level] + bitStr;
        }
    }
    recordStr = recordStr.substr(0, sizeof(record) - 1);
    int bytes = snprintf_s(record, sizeof(record), sizeof(record) - 1, "%s", recordStr.c_str());
    if (bytes == -1) {
        HILOG_INFO(LOG_CORE, "GetTraceResult: recordStr may be truncated");
    }
    return FindResult(std::string(record), list);
}
}
}
}
