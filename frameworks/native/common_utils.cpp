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

#include <unistd.h>
#include <cstdio>
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

std::string CanonicalizeSpecPath(const char* src)
{
    if (src == nullptr || strlen(src) >= PATH_MAX) {
        HiLog::Error(LABEL, "CanonicalizeSpecPath: %{pubilc}s failed.", src);
        return "";
    }
    char resolvedPath[PATH_MAX] = { 0 };

    if (access(src, F_OK) == 0) {
        if (realpath(src, resolvedPath) == nullptr) {
            HiLog::Error(LABEL, "CanonicalizeSpecPath: realpath %{pubilc}s failed.", src);
            return "";
        }
    } else {
        std::string fileName(src);
        if (fileName.find("..") == std::string::npos) {
            if (sprintf_s(resolvedPath, PATH_MAX, "%s", src) == -1) {
                HiLog::Error(LABEL, "CanonicalizeSpecPath: sprintf_s %{pubilc}s failed.", src);
                return "";
            }
        } else {
            HiLog::Error(LABEL, "CanonicalizeSpecPath: find .. src failed.");
            return "";
        }
    }

    std::string res(resolvedPath);
    return res;
}

} // Hitrace
} // HiviewDFX
} // OHOS
