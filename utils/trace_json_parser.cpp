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

#include "trace_json_parser.h"

#include <cinttypes>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <fcntl.h>

#include "common_define.h"
#include "cJSON.h"
#include "hilog/log.h"

#include "common_utils.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceUtils"
#endif
namespace {
static const char* HTIRACE_UTILS_JSON = "/system/etc/hiview/hitrace_utils.json";
static const char* PRODUCT_CONFIG_JSON = "/sys_prod/etc/hiview/hitrace/hitrace_param.json";
const char* const MEM_TOTAL = "MemTotal";

constexpr int DEFAULT_SNAPSHOT_BUFFER_SIZE_KB = 12 * 1024;
constexpr int HM_DEFAULT_SNAPSHOT_BUFFER_SIZE_KB = 144 * 1024;
constexpr int EXTRA_BUFFER_SIZE_KB_GE_16G_DEVICE = 48 * 1024;
constexpr int MEM_GB = 1024 * 1024;

constexpr uint32_t DEFAULT_RECORD_FILE_NUMBER_LIMIT = 15;

constexpr uint32_t DEFAULT_SNAPSHOT_FILE_NUMBER_LIMIT = 35;

bool GetInt64FromJson(cJSON* jsonNode, const std::string& key, int64_t& value)
{
    cJSON* item = cJSON_GetObjectItem(jsonNode, key.c_str());
    if (item == nullptr) {
        HILOG_ERROR(LOG_CORE, "GetInt64FromJson: [%{public}s] not found.", key.c_str());
        return false;
    }
    if (!cJSON_IsNumber(item)) {
        HILOG_ERROR(LOG_CORE, "GetInt64FromJson: [%{public}s] item is illegal.", key.c_str());
        return false;
    }
    value = static_cast<int64_t>(item->valueint);
    return true;
}

bool GetIntFromJson(cJSON* jsonNode, const std::string& key, int& value)
{
    cJSON* item = cJSON_GetObjectItem(jsonNode, key.c_str());
    if (item == nullptr) {
        HILOG_ERROR(LOG_CORE, "GetIntFromJson: [%{public}s] not found.", key.c_str());
        return false;
    }
    if (!cJSON_IsNumber(item)) {
        HILOG_ERROR(LOG_CORE, "GetIntFromJson: [%{public}s] item is illegal.", key.c_str());
        return false;
    }
    value = item->valueint;
    return true;
}

void GetStringFromJsonVector(cJSON *jsonNode, const std::string& key, std::vector<std::string>& vec)
{
    cJSON* node = cJSON_GetObjectItem(jsonNode, key.c_str());
    if (node != nullptr && cJSON_IsArray(node)) {
        cJSON *item = nullptr;
        cJSON_ArrayForEach(item, node) {
            if (cJSON_IsString(item)) {
                vec.push_back(item->valuestring);
            }
        }
    }
}

cJSON* ParseJsonFromFile(const std::string& filePath)
{
    std::ifstream inFile(filePath, std::ios::in);
    if (!inFile.is_open()) {
        HILOG_ERROR(LOG_CORE, "ParseJsonFromFile: %{public}s is not existed.", filePath.c_str());
        return nullptr;
    }
    std::string fileContent((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    cJSON* rootNode = cJSON_Parse(fileContent.c_str());
    if (rootNode == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseJsonFromFile: %{public}s is not in JSON format.", filePath.c_str());
    }
    inFile.close();
    return rootNode;
}

bool ParseTagCategory(cJSON* jsonNode, std::map<std::string, TraceTag>& tagInfos)
{
    cJSON* tagCategoryNode = cJSON_GetObjectItem(jsonNode, "tag_category");
    if (tagCategoryNode == nullptr) {
        HILOG_ERROR(LOG_CORE, "TraceJsonParser: tag_category json node not found.");
        return false;
    }
    cJSON* tagNode = nullptr;
    cJSON_ArrayForEach(tagNode, tagCategoryNode) {
        if (tagNode == nullptr || tagNode->string == nullptr) {
            continue;
        }
        // when base info has been parsed, only try to parse particial trace infos.
        if (tagInfos.find(tagNode->string) != tagInfos.end()) {
            GetStringFromJsonVector(tagNode, "enable_path", tagInfos[tagNode->string].enablePath);
            GetStringFromJsonVector(tagNode, "format_path", tagInfos[tagNode->string].formatPath);
            continue;
        }
        TraceTag tag;
        cJSON* description = cJSON_GetObjectItem(tagNode, "description");
        if (description != nullptr && cJSON_IsString(description)) {
            tag.description = description->valuestring;
        }
        cJSON* tagOffset = cJSON_GetObjectItem(tagNode, "tag_offset");
        if (tagOffset != nullptr && cJSON_IsNumber(tagOffset)) {
            tag.tag = 1ULL << tagOffset->valueint;
        }
        cJSON* type = cJSON_GetObjectItem(tagNode, "type");

        if (type != nullptr && cJSON_IsNumber(type)) {
            tag.type = static_cast<TraceType>(type->valueint);
        }
        GetStringFromJsonVector(tagNode, "enable_path", tag.enablePath);
        GetStringFromJsonVector(tagNode, "format_path", tag.formatPath);
        tagInfos.insert(std::pair<std::string, TraceTag>(tagNode->string, tag));
    }
    return true;
}

bool ParseTagGroups(cJSON* jsonNode, std::map<std::string, std::vector<std::string>>& tagGroups)
{
    cJSON* tagGroupsNode = cJSON_GetObjectItem(jsonNode, "tag_groups");
    if (tagGroupsNode == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson: tag_groups json node not found.");
        return false;
    }
    cJSON* tagGroupNode = nullptr;
    cJSON_ArrayForEach(tagGroupNode, tagGroupsNode) {
        if (tagGroupNode == nullptr || tagGroupNode->string == nullptr) {
            continue;
        }
        std::string tagGroupName = tagGroupNode->string;
        std::vector<std::string> tagList;
        cJSON* tagNameNode = nullptr;
        cJSON_ArrayForEach(tagNameNode, tagGroupNode) {
            if (cJSON_IsString(tagNameNode)) {
                tagList.push_back(tagNameNode->valuestring);
            }
        }
        tagGroups.insert(std::pair<std::string, std::vector<std::string>>(tagGroupName, tagList));
    }
    return true;
}
}

TraceJsonParser& TraceJsonParser::Instance()
{
    static TraceJsonParser parser(HTIRACE_UTILS_JSON, PRODUCT_CONFIG_JSON);
    return parser;
}

TraceJsonParser::TraceJsonParser(const std::string& hitraceUtilsJson, const std::string& productConfigJson)
{
    InitSnapshotDefaultBufferSize();
    InitAgeingParam();

    ParseHitraceUtilsJson(hitraceUtilsJson);
    ParseProductConfigJson(productConfigJson);

    PrintParseResult();
}

void TraceJsonParser::ParseHitraceUtilsJson(const std::string& hitraceUtilsJson)
{
    cJSON* hitraceUtilsJsonRoot = ParseJsonFromFile(hitraceUtilsJson);
    if (hitraceUtilsJsonRoot == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson: open %{public}s fail", hitraceUtilsJson.c_str());
    } else {
        ParseTagCategory(hitraceUtilsJsonRoot, traceTagInfos_);
        GetStringFromJsonVector(hitraceUtilsJsonRoot, "base_format_path", baseTraceFormats_);
        ParseTagGroups(hitraceUtilsJsonRoot, tagGroups_);

        int value = 0;
        if (GetIntFromJson(hitraceUtilsJsonRoot, "snapshot_file_aging", value)) {
            snapShotAgeingParam_.rootEnable = (value != 0);
        }
        if (GetIntFromJson(hitraceUtilsJsonRoot, "record_file_aging", value)) {
            recordAgeingParam_.rootEnable = (value != 0);
        }
        if (GetIntFromJson(hitraceUtilsJsonRoot, "snapshot_buffer_kb", value) && value != 0) {
            snapshotBufSzKb_ = value;
        }
    }
    cJSON_Delete(hitraceUtilsJsonRoot);
}

void TraceJsonParser::ParseProductConfigJson(const std::string& productConfigJson)
{
    cJSON* productConfigJsonRoot = ParseJsonFromFile(productConfigJson);
    if (productConfigJsonRoot == nullptr) {
        HILOG_DEBUG(LOG_CORE, "ParseTraceJson: open %{public}s fail", productConfigJson.c_str());
    } else {
        GetInt64FromJson(productConfigJsonRoot, "record_file_kb_size", recordAgeingParam_.fileSizeKbLimit);
        GetInt64FromJson(productConfigJsonRoot, "snapshot_file_kb_size", snapShotAgeingParam_.fileSizeKbLimit);
        GetIntFromJson(productConfigJsonRoot, "default_buffer_kb_size", snapshotBufSzKb_);

        int tRootAgeingEnable = -1;
        if (GetIntFromJson(productConfigJsonRoot, "root_ageing_enable", tRootAgeingEnable)) {
            bool enable = (tRootAgeingEnable != 0);
            snapShotAgeingParam_.rootEnable = enable;
            recordAgeingParam_.rootEnable = enable;
        }
    }

    cJSON_Delete(productConfigJsonRoot);
}

const AgeingParam& TraceJsonParser::GetAgeingParam(TraceDumpType type) const
{
    if (type == TraceDumpType::TRACE_RECORDING) {
        return recordAgeingParam_;
    }
    if (type == TraceDumpType::TRACE_SNAPSHOT) {
        return snapShotAgeingParam_;
    }

    static AgeingParam defaultParam;
    return defaultParam;
}

void TraceJsonParser::InitSnapshotDefaultBufferSize()
{
    snapshotBufSzKb_ = DEFAULT_SNAPSHOT_BUFFER_SIZE_KB;

    if (IsHmKernel()) {
        snapshotBufSzKb_ = HM_DEFAULT_SNAPSHOT_BUFFER_SIZE_KB;
        int memTotal = GetMemInfoByName(MEM_TOTAL);
        constexpr int base = 4;
        // Round up the obtained RAM size by multiples of four.
        // Add extra buffer size for devices with RAM greater than or equal to 16GB.
        if ((((memTotal / MEM_GB) + base - 1) / base * base) >= 16) {
            snapshotBufSzKb_ += EXTRA_BUFFER_SIZE_KB_GE_16G_DEVICE;
        }
    }

#if defined(SNAPSHOT_TRACEBUFFER_SIZE) && (SNAPSHOT_TRACEBUFFER_SIZE != 0)
    snapshotBufSzKb_ = SNAPSHOT_TRACEBUFFER_SIZE;
#endif
}

void TraceJsonParser::InitAgeingParam()
{
    // record file number limit
    recordAgeingParam_.fileNumberLimit = DEFAULT_RECORD_FILE_NUMBER_LIMIT;
#if defined(RECORD_FILE_LIMIT) && (RECORD_FILE_LIMIT != 0)
    recordAgeingParam_.fileNumberLimit = RECORD_FILE_LIMIT;
#endif

    // snapshot file number limit
    snapShotAgeingParam_.fileNumberLimit = DEFAULT_SNAPSHOT_FILE_NUMBER_LIMIT;
#if defined(SNAPSHOT_FILE_LIMIT) && (SNAPSHOT_FILE_LIMIT != 0)
    snapShotAgeingParam_.fileNumberLimit = SNAPSHOT_FILE_LIMIT;
#endif
}

void TraceJsonParser::PrintParseResult()
{
    HILOG_INFO(LOG_CORE, "PrintParseResult snap:[%{public}d %{public}" PRId64" %{public}" PRId64 "] "
        "reco:[%{public}d %{public}" PRId64 " %{public}" PRId64 "] bufsz:[%{public}d]",
        snapShotAgeingParam_.rootEnable, snapShotAgeingParam_.fileNumberLimit, snapShotAgeingParam_.fileSizeKbLimit,
        recordAgeingParam_.rootEnable, recordAgeingParam_.fileNumberLimit, recordAgeingParam_.fileSizeKbLimit,
        snapshotBufSzKb_);
}

} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS
