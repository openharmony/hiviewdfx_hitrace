/*
* Copyright (C) 2026 Huawei Device Co., Ltd.
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

#ifndef HITRACE_HITRACE_UTIL_H
#define HITRACE_HITRACE_UTIL_H
#include <dirent.h>
#include <functional>
#include <string>
#include <vector>

#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

class FileLock {
public:
    explicit FileLock(const std::string& filename, int flags);
    ~FileLock();
    int Fd() const;
private:
    SmartFd fd_;
};

bool AppendToFile(const std::string& filename, const std::string& str);

bool TraverseFileLineByLine(const std::string &filePath,
    const std::function<bool(const char* lineContent, size_t lineNum)> &lineHandler);

bool GetSubThreadIds(pid_t pid, std::vector<std::string>& subThreadIds);

bool GetSubThreadIds(const std::string& pid, std::vector<std::string>& subThreadIds);

const std::string& GetTraceRootPath();

bool AppendTracePoint(const std::string& relativePath, const std::string& pidContent);

bool ClearTracePoint(const std::string& relativePath);
}
}
}


#endif //HITRACE_HITRACE_UTIL_H