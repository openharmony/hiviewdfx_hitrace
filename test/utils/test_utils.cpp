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

#include "test_utils.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "common_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
const std::string TRACE_SNAPSHOT_PREFIX = "trace_";
const std::string TRACE_RECORD_PREFIX = "record_";

std::vector<std::string> GetTraceFileByPrefix(const std::string& prefix)
{
    if (access(TRACE_FILE_DEFAULT_DIR.c_str(), F_OK) != 0) {
        return {};
    }
    DIR* dirPtr = opendir(TRACE_FILE_DEFAULT_DIR.c_str());
    if (dirPtr == nullptr) {
        return {};
    }

    std::vector<std::string> filelist = {};
    struct dirent* ptr = nullptr;
    while ((ptr = readdir(dirPtr)) != nullptr) {
        if (ptr->d_type == DT_REG) {
            std::string name = std::string(ptr->d_name);
            if (name.compare(0, prefix.size(), prefix) == 0) {
                filelist.push_back(name);
            }
        }
    }
    closedir(dirPtr);
    return filelist;
}
}

int CountSnapShotTraceFile()
{
    return GetTraceFileByPrefix(TRACE_SNAPSHOT_PREFIX).size();
}

int CountRecordingTraceFile()
{
    return GetTraceFileByPrefix(TRACE_RECORD_PREFIX).size();
}

void GetSnapShotTraceFileList(std::vector<std::string>& fileList)
{
    fileList = GetTraceFileByPrefix(TRACE_SNAPSHOT_PREFIX);
}

void GetRecordingTraceFileList(std::vector<std::string>& fileList)
{
    fileList = GetTraceFileByPrefix(TRACE_RECORD_PREFIX);
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS