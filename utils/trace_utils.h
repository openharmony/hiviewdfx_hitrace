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

#ifndef TRACE_UTILS_H
#define TRACE_UTILS_H

#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "HitraceInfo"

std::string GenerateTraceFileName(bool isSnapshot = true);
void DelSnapshotTraceFile(const bool deleteSavedFmt = true, const int keepFileCount = 0);
void DelOldRecordTraceFile(const int& fileLimit);
void ClearOldTraceFile(std::vector<std::string>& fileLists, const int& fileLimit);
void DelSavedEventsFormat();
} // namespace HiTrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_UTILS_H