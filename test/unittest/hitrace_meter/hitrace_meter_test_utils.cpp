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

#include <fcntl.h>
#include <sstream>
#include <unistd.h>
#include <unordered_map>
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

constexpr int NAME_SIZE_MAX = 320;
constexpr int CATEGORY_SIZE_MAX = 64;
constexpr int HITRACEID_LEN = 64;
const char g_traceLevel[4] = {'D', 'I', 'C', 'M'};
static std::string g_traceRootPath;
static char g_pid[6];

#ifndef HITRACE_METER_SDK_C
static const std::unordered_map<uint64_t, const char* const> hitraceTags = {
    {HITRACE_TAG_DRM, "06"}, {HITRACE_TAG_SECURITY, "07"},
    {HITRACE_TAG_ANIMATION, "09"}, {HITRACE_TAG_PUSH, "10"}, {HITRACE_TAG_VIRSE, "11"},
    {HITRACE_TAG_MUSL, "12"}, {HITRACE_TAG_FFRT, "13"}, {HITRACE_TAG_CLOUD, "14"},
    {HITRACE_TAG_DEV_AUTH, "15"}, {HITRACE_TAG_COMMONLIBRARY, "16"}, {HITRACE_TAG_HDCD, "17"},
    {HITRACE_TAG_HDF, "18"}, {HITRACE_TAG_USB, "19"}, {HITRACE_TAG_INTERCONNECTION, "20"},
    {HITRACE_TAG_DLP_CREDENTIAL, "21"}, {HITRACE_TAG_ACCESS_CONTROL, "22"}, {HITRACE_TAG_NET, "23"},
    {HITRACE_TAG_NWEB, "24"}, {HITRACE_TAG_HUKS, "25"}, {HITRACE_TAG_USERIAM, "26"},
    {HITRACE_TAG_DISTRIBUTED_AUDIO, "27"}, {HITRACE_TAG_DLSM, "28"}, {HITRACE_TAG_FILEMANAGEMENT, "29"},
    {HITRACE_TAG_OHOS, "30"}, {HITRACE_TAG_ABILITY_MANAGER, "31"}, {HITRACE_TAG_ZCAMERA, "32"},
    {HITRACE_TAG_ZMEDIA, "33"}, {HITRACE_TAG_ZIMAGE, "34"}, {HITRACE_TAG_ZAUDIO, "35"},
    {HITRACE_TAG_DISTRIBUTEDDATA, "36"}, {HITRACE_TAG_MDFS, "37"}, {HITRACE_TAG_GRAPHIC_AGP, "38"},
    {HITRACE_TAG_ACE, "39"}, {HITRACE_TAG_NOTIFICATION, "40"}, {HITRACE_TAG_MISC, "41"},
    {HITRACE_TAG_MULTIMODALINPUT, "42"}, {HITRACE_TAG_SENSORS, "43"}, {HITRACE_TAG_MSDP, "44"},
    {HITRACE_TAG_DSOFTBUS, "45"}, {HITRACE_TAG_RPC, "46"}, {HITRACE_TAG_ARK, "47"},
    {HITRACE_TAG_WINDOW_MANAGER, "48"}, {HITRACE_TAG_ACCOUNT_MANAGER, "49"}, {HITRACE_TAG_DISTRIBUTED_SCREEN, "50"},
    {HITRACE_TAG_DISTRIBUTED_CAMERA, "51"}, {HITRACE_TAG_DISTRIBUTED_HARDWARE_FWK, "52"},
    {HITRACE_TAG_GLOBAL_RESMGR, "53"}, {HITRACE_TAG_DEVICE_MANAGER, "54"}, {HITRACE_TAG_SAMGR, "55"},
    {HITRACE_TAG_POWER, "56"}, {HITRACE_TAG_DISTRIBUTED_SCHEDULE, "57"}, {HITRACE_TAG_DEVICE_PROFILE, "58"},
    {HITRACE_TAG_DISTRIBUTED_INPUT, "59"}, {HITRACE_TAG_BLUETOOTH, "60"},
    {HITRACE_TAG_ACCESSIBILITY_MANAGER, "61"}, {HITRACE_TAG_APP, "62"}
};
#endif

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

#ifndef HITRACE_METER_SDK_C
static std::string ParseTagBits(const uint64_t tag)
{
    auto it = hitraceTags.find(tag & (~HITRACE_TAG_COMMERCIAL));
    if (EXPECTANTLY(it != hitraceTags.end())) {
        return it->second;
    }
    std::string result;
    for (auto& [hitraceTag, bitStr] : hitraceTags) {
        if ((tag & hitraceTag) != 0) {
            result += bitStr;
        }
    }
    return result;
}
#endif

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

bool GetTraceResult(TraceInfo& traceInfo, const std::vector<std::string>& list,
    char (&record)[RECORD_SIZE_MAX + 1])
{
    if (list.empty()) {
        return false;
    }

#ifdef HITRACE_METER_SDK_C
    std::string bitsStr = "62";
#else
    std::string bitsStr = ParseTagBits(traceInfo.tag);
#endif
    std::string chainStr = "";
    if (traceInfo.hiTraceId != nullptr) {
        chainStr = GetRecord(traceInfo.hiTraceId);
    }
    if (traceInfo.name == nullptr) {
        traceInfo.name = "";
    }
    if (traceInfo.customCategory == nullptr) {
        traceInfo.customCategory = "";
    }
    if (traceInfo.customArgs == nullptr) {
        traceInfo.customArgs = "";
    }

    if (traceInfo.type == 'B') {
        (void)snprintf_s(record, sizeof(record), sizeof(record) - 1, "B|%s|H:%s%.*s|%c%s|%s",
            g_pid, chainStr.c_str(), NAME_SIZE_MAX, traceInfo.name, g_traceLevel[traceInfo.level],
            bitsStr.c_str(), traceInfo.customArgs);
    } else if (traceInfo.type == 'E') {
        (void)snprintf_s(record, sizeof(record), sizeof(record) - 1, "E|%s|%c%s",
            g_pid, g_traceLevel[traceInfo.level], bitsStr.c_str());
    } else if (traceInfo.type == 'S') {
        (void)snprintf_s(record, sizeof(record), sizeof(record) - 1, "S|%s|H:%s%.*s|%lld|%c%s|%.*s|%s", g_pid,
            chainStr.c_str(), NAME_SIZE_MAX, traceInfo.name, traceInfo.value, g_traceLevel[traceInfo.level],
            bitsStr.c_str(), CATEGORY_SIZE_MAX, traceInfo.customCategory, traceInfo.customArgs);
    } else {
        (void)snprintf_s(record, sizeof(record), sizeof(record) - 1, "%c|%s|H:%s%.*s|%lld|%c%s",
            traceInfo.type, g_pid, chainStr.c_str(), NAME_SIZE_MAX, traceInfo.name,
            traceInfo.value, g_traceLevel[traceInfo.level], bitsStr.c_str());
    }

    return FindResult(std::string(record), list);
}
}
}
}
