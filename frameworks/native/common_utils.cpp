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

#include "common_utils.h"

#include <cinttypes>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <fcntl.h>
#include "cJSON.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

std::string CanonicalizeSpecPath(const char* src)
{
    if (src == nullptr || strlen(src) >= PATH_MAX) {
        HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: %{pubilc}s failed.", src);
        return "";
    }
    char resolvedPath[PATH_MAX] = { 0 };

    if (access(src, F_OK) == 0) {
        if (realpath(src, resolvedPath) == nullptr) {
            HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: realpath %{pubilc}s failed.", src);
            return "";
        }
    } else {
        std::string fileName(src);
        if (fileName.find("..") == std::string::npos) {
            if (sprintf_s(resolvedPath, PATH_MAX, "%s", src) == -1) {
                HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: sprintf_s %{pubilc}s failed.", src);
                return "";
            }
        } else {
            HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: find .. src failed.");
            return "";
        }
    }

    std::string res(resolvedPath);
    return res;
}

bool MarkClockSync(const std::string& traceRootPath)
{
    constexpr unsigned int bufferSize = 128;
    char buffer[bufferSize] = { 0 };
    std::string traceMarker = "trace_marker";
    std::string resolvedPath = CanonicalizeSpecPath((traceRootPath + traceMarker).c_str());
    int fd = open(resolvedPath.c_str(), O_WRONLY);
    if (fd == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: oepn %{public}s fail, errno(%{public}d)", resolvedPath.c_str(), errno);
        return false;
    }

    // write realtime_ts
    struct timespec rts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &rts) == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: get realtime error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr unsigned int nanoSeconds = 1000000000; // seconds converted to nanoseconds
    constexpr unsigned int nanoToMill = 1000000; // millisecond converted to nanoseconds
    int len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1,
        "trace_event_clock_sync: realtime_ts=%" PRId64 "\n",
        static_cast<int64_t>((rts.tv_sec * nanoSeconds + rts.tv_nsec) / nanoToMill));
    if (len < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: entering realtime_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }

    if (write(fd, buffer, len) < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: writing realtime error, errno(%{public}d)", errno);
    }

    // write parent_ts
    struct timespec mts = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &mts) == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: get parent_ts error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr float nanoToSecond = 1000000000.0f; // consistent with the ftrace timestamp format
    len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "trace_event_clock_sync: parent_ts=%f\n",
        static_cast<float>(((static_cast<float>(mts.tv_sec)) * nanoSeconds + mts.tv_nsec) / nanoToSecond));
    if (len < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: entering parent_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    if (write(fd, buffer, len) < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: writing parent_ts error, errno(%{public}d)", errno);
    }
    close(fd);
    return true;
}

static cJSON* ParseJsonFromFile(const std::string& filePath)
{
    std::ifstream inFile(filePath, std::ios::in);
    if (!inFile.is_open()) {
        HILOG_ERROR(LOG_CORE, "ParseJsonFromFile: %{public}s is not existed.", filePath.c_str());
        return nullptr;
    }
    std::string fileContent((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    cJSON* root = cJSON_Parse(fileContent.c_str());
    if (root == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseJsonFromFile: %{public}s is not in JSON format.", filePath.c_str());
    }
    inFile.close();
    return root;
}

static void ParseSysFiles(cJSON *tags, TagCategory& tagCategory)
{
    cJSON *sysFiles = cJSON_GetObjectItem(tags, "sysFiles");
    if (sysFiles != nullptr && cJSON_IsArray(sysFiles)) {
        cJSON *sysFile = nullptr;
        cJSON_ArrayForEach(sysFile, sysFiles) {
            if (cJSON_IsString(sysFile)) {
                tagCategory.sysFiles.push_back(sysFile->valuestring);
            }
        }
    }
}

static bool ParseTagCategory(cJSON* tagCategoryNode, std::map<std::string, TagCategory>& allTags)
{
    cJSON* tags = nullptr;
    cJSON_ArrayForEach(tags, tagCategoryNode) {
        if (tags == nullptr || tags->string == nullptr) {
            continue;
        }
        TagCategory tagCategory;
        cJSON* description = cJSON_GetObjectItem(tags, "description");
        if (description != nullptr && cJSON_IsString(description)) {
            tagCategory.description = description->valuestring;
        }
        cJSON* tagOffset = cJSON_GetObjectItem(tags, "tag_offset");
        if (tagOffset != nullptr && cJSON_IsNumber(tagOffset)) {
            tagCategory.tag = 1ULL << tagOffset->valueint;
        }
        cJSON* type = cJSON_GetObjectItem(tags, "type");
        if (type != nullptr && cJSON_IsNumber(type)) {
            tagCategory.type = type->valueint;
        }
        ParseSysFiles(tags, tagCategory);
        allTags.insert(std::pair<std::string, TagCategory>(tags->string, tagCategory));
    }
    return true;
}

static bool ParseTagGroups(cJSON* tagGroupsNode, std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    cJSON* tagGroup = nullptr;
    cJSON_ArrayForEach(tagGroup, tagGroupsNode) {
        if (tagGroup == nullptr || tagGroup->string == nullptr) {
            continue;
        }
        std::string tagGroupName = tagGroup->string;
        std::vector<std::string> tagList;
        cJSON* tag = nullptr;
        cJSON_ArrayForEach(tag, tagGroup) {
            if (cJSON_IsString(tag)) {
                tagList.push_back(tag->valuestring);
            }
        }
        tagGroupTable.insert(std::pair<std::string, std::vector<std::string>>(tagGroupName, tagList));
    }
    return true;
}

bool ParseTagInfo(std::map<std::string, TagCategory> &allTags,
                  std::map<std::string, std::vector<std::string>> &tagGroupTable)
{
    std::string traceUtilsPath = "/system/etc/hiview/hitrace_utils.json";
    cJSON* root = ParseJsonFromFile(traceUtilsPath);
    if (root == nullptr) {
        return false;
    }
    cJSON* tagCategory = cJSON_GetObjectItem(root, "tag_category");
    if (tagCategory == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTagInfo: %{public}s is not contain tag_category node.", traceUtilsPath.c_str());
        cJSON_Delete(root);
        return false;
    }
    if (!ParseTagCategory(tagCategory, allTags)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON* tagGroups = cJSON_GetObjectItem(root, "tag_groups");
    if (tagGroups == nullptr) {
        HILOG_ERROR(LOG_CORE, "ParseTagInfo: %{public}s is not contain tag_groups node.", traceUtilsPath.c_str());
        cJSON_Delete(root);
        return false;
    }
    if (!ParseTagGroups(tagGroups, tagGroupTable)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(root);
    HILOG_INFO(LOG_CORE, "ParseTagInfo: parse done.");
    return true;
}

bool IsNumber(const std::string &str)
{
    if (str.empty()) {
        return false;
    }
    for (auto c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}
} // Hitrace
} // HiviewDFX
} // OHOS
