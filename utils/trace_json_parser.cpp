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
#include <sstream>

#include "common_define.h"
#include "cJSON.h"
#include "hilog/log.h"

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
const std::string HTIRACE_TAG_CONFIG_FILE = "/system/etc/hiview/hitrace_utils.json";

void ParseEnablePath(cJSON *tagNode, TraceTag& tag)
{
    cJSON *enablePathNode = cJSON_GetObjectItem(tagNode, "enable_path");
    if (enablePathNode != nullptr && cJSON_IsArray(enablePathNode)) {
        cJSON *pathItem = nullptr;
        cJSON_ArrayForEach(pathItem, enablePathNode) {
            if (cJSON_IsString(pathItem)) {
                tag.enablePath.push_back(pathItem->valuestring);
            }
        }
    }
}

void ParseFormatPath(cJSON *tagNode, TraceTag& tag)
{
    cJSON *formatPathNode = cJSON_GetObjectItem(tagNode, "format_path");
    if (formatPathNode != nullptr && cJSON_IsArray(formatPathNode)) {
        cJSON *pathItem = nullptr;
        cJSON_ArrayForEach(pathItem, formatPathNode) {
            if (cJSON_IsString(pathItem)) {
                tag.formatPath.push_back(pathItem->valuestring);
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

bool ParseTagCategory(cJSON* jsonNode, bool parseEnable, bool parseFmt, std::map<std::string, TraceTag>& tagInfos)
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
            if (parseEnable) {
                ParseEnablePath(tagNode, tagInfos[tagNode->string]);
            }
            if (parseFmt) {
                ParseFormatPath(tagNode, tagInfos[tagNode->string]);
            }
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
        if (parseEnable) {
            ParseEnablePath(tagNode, tag);
        }
        if (parseFmt) {
            ParseFormatPath(tagNode, tag);
        }
        tagInfos.insert(std::pair<std::string, TraceTag>(tagNode->string, tag));
    }
    return true;
}

bool ParseBaseFormatPath(cJSON* jsonNode, std::vector<std::string>& baseTraceFormats)
{
    cJSON* baseTraceFormatsNode = cJSON_GetObjectItem(jsonNode, "base_format_path");
    if (baseTraceFormatsNode == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson: base_format_path json node not found.");
        return false;
    }
    if (cJSON_IsArray(baseTraceFormatsNode)) {
        cJSON *formatItem = nullptr;
        cJSON_ArrayForEach(formatItem, baseTraceFormatsNode) {
            if (cJSON_IsString(formatItem)) {
                baseTraceFormats.push_back(formatItem->valuestring);
            }
        }
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

bool ParseTraceBufSz(cJSON* jsonNode, int& bufSz)
{
    cJSON* bufSzNode = cJSON_GetObjectItem(jsonNode, "snapshot_buffer_kb");
    if (bufSzNode == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson: snapshot_buffer_kb json node not found.");
        return false;
    }
    if (!cJSON_IsNumber(bufSzNode)) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson: snapshot_buffer_kb item is illegal.");
        return false;
    }
    bufSz = bufSzNode->valueint;
    return true;
}

bool ParseTraceFileAge(cJSON* jsonNode, const std::string& jsonLabel, bool& agingSwitcher)
{
    cJSON* agingNode = cJSON_GetObjectItem(jsonNode, jsonLabel.c_str());
    if (agingNode == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson: %{public}s json node not found.", jsonLabel.c_str());
        return false;
    }
    if (!cJSON_IsNumber(agingNode)) {
        HILOG_ERROR(LOG_CORE, "ParseTraceJson:  %{public}s item is illegal.", jsonLabel.c_str());
        return false;
    }
    agingSwitcher = agingNode->valueint == 0 ? false : true;
    return true;
}

uint8_t UpdateParseItem(const uint8_t parseItem)
{
    uint8_t retParseItem = parseItem;
    if ((parseItem & TRACE_TAG_ENABLE_INFO) > 0 || (parseItem & TRACE_TAG_FORMAT_INFO) > 0 ||
        (parseItem & TRACE_TAG_GROUP_INFO) > 0) {
        retParseItem |= TRACE_TAG_BASE_INFO;
    }
    return retParseItem;
}
}

bool TraceJsonParser::ParseTraceJson(const uint8_t policy)
{
    if ((policy & parserState_) == policy) {
        return true;
    }

    uint8_t needParseItem = UpdateParseItem(policy);
    needParseItem &= (~parserState_);

    cJSON* rootNode = ParseJsonFromFile(HTIRACE_TAG_CONFIG_FILE);
    if (rootNode == nullptr) {
        return false;
    }

    if ((needParseItem & TRACE_SNAPSHOT_BUFSZ) > 0 && ParseTraceBufSz(rootNode, snapshotBufSzKb_)) {
        parserState_ |= TRACE_SNAPSHOT_BUFSZ;
    }
    if ((needParseItem & TRACE_SNAPSHOT_FILE_AGE) > 0 &&
        ParseTraceFileAge(rootNode, "snapshot_file_aging", snapshotFileAge_)) {
        parserState_ |= TRACE_SNAPSHOT_FILE_AGE;
    }

    bool needParseTagEnableInfo = (needParseItem & TRACE_TAG_ENABLE_INFO) > 0;
    bool needParseTagFmtInfo = (needParseItem & TRACE_TAG_FORMAT_INFO) > 0;
    if ((needParseItem & TRACE_TAG_BASE_INFO) > 0 || needParseTagEnableInfo || needParseTagFmtInfo) {
        if (ParseTagCategory(rootNode, needParseTagEnableInfo, needParseTagFmtInfo, traceTagInfos_)) {
            parserState_ |= TRACE_TAG_BASE_INFO;
            if (needParseTagEnableInfo) {
                parserState_ |= TRACE_TAG_ENABLE_INFO;
            }
        }
    }

    if (needParseTagFmtInfo && ParseBaseFormatPath(rootNode, baseTraceFormats_)) {
        parserState_ |= TRACE_TAG_FORMAT_INFO;
    }

    if ((needParseItem & TRACE_TAG_GROUP_INFO) > 0 && ParseTagGroups(rootNode, tagGroups_)) {
        parserState_ |= TRACE_TAG_GROUP_INFO;
    }

    cJSON_Delete(rootNode);
    HILOG_INFO(LOG_CORE, "ParseTraceJson: parse done, input policy(%{public}u), parser state(%{public}u)",
        policy, parserState_);
    return (policy & parserState_) == policy;
}

bool GetUint64FromJson(cJSON* jsonNode, const std::string& key, uint64_t& value)
{
    cJSON* item = cJSON_GetObjectItem(jsonNode, key.c_str());
    if (item == nullptr) {
        HILOG_ERROR(LOG_CORE, "GetUint64FromJson: [%{public}s] not found.", key.c_str());
        return false;
    }
    if (!cJSON_IsNumber(item)) {
        HILOG_ERROR(LOG_CORE, "GetUint64FromJson: [%{public}s] item is illegal.", key.c_str());
        return false;
    }
    value = static_cast<uint64_t>(item->valueint);
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

ProductConfigJsonParser::ProductConfigJsonParser(const std::string& configJsonPath)
{
    cJSON* rootNode = ParseJsonFromFile(configJsonPath);
    if (rootNode == nullptr) {
        return;
    }

    GetUint64FromJson(rootNode, "record_file_kb_size", recordFileSizeKb);
    GetUint64FromJson(rootNode, "snapshot_file_kb_size", snapshotFileSizeKb);
    GetIntFromJson(rootNode, "default_buffer_kb_size", defaultBufferSize);

    int tRootAgeingEnable = -1;
    if (GetIntFromJson(rootNode, "root_ageing_enable", tRootAgeingEnable)) {
        if (tRootAgeingEnable == 0) {
            rootAgeingEnable = ConfigStatus::DISABLE;
        } else {
            rootAgeingEnable = ConfigStatus::ENABLE;
        }
    }

    cJSON_Delete(rootNode);
}

uint64_t ProductConfigJsonParser::GetRecordFileSizeKb() const
{
    return recordFileSizeKb;
}

uint64_t ProductConfigJsonParser::GetSnapshotFileSizeKb() const
{
    return snapshotFileSizeKb;
}

int ProductConfigJsonParser::GetDefaultBufferSize() const
{
    return defaultBufferSize;
}

ConfigStatus ProductConfigJsonParser::GetRootAgeingStatus() const
{
    return rootAgeingEnable;
}
} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS