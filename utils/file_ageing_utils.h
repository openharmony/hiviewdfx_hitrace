/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#ifndef FILE_AGEING_UTILS_H
#define FILE_AGEING_UTILS_H

#include <cstdint>
#include <vector>

#include "trace_file_utils.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

class FileAgeingUtils {
public:
    static void HandleAgeing(std::vector<TraceFileInfo>& fileList, const TRACE_TYPE traceType);

private:
    FileAgeingUtils();
    ~FileAgeingUtils() = default;

    FileAgeingUtils(FileAgeingUtils&) = delete;
    FileAgeingUtils(FileAgeingUtils&&) = delete;
    FileAgeingUtils& operator=(const FileAgeingUtils&) = delete;
    FileAgeingUtils& operator=(const FileAgeingUtils&&) = delete;
};

} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS

#endif // FILE_AGEING_UTILS_H