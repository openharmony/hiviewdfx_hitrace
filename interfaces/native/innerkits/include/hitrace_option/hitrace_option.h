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

#ifndef HITRACE_OPTION_H
#define HITRACE_OPTION_H

#ifndef __cplusplus
#include <sys/types.h>
void FilterAppTrace(const char* app, pid_t pid);
#else
#include <cstdint>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

constexpr int32_t HITRACE_NO_ERROR = 0;
constexpr int32_t HITRACE_SET_PARAM_ERROR = -1;
constexpr int32_t HITRACE_WRITE_FILE_ERROR = -2;

int32_t SetFilterAppName(const std::string& app);

int32_t AddFilterPid(const pid_t pid);

int32_t AddFilterPids(const std::vector<std::string>& pids);

int32_t ClearFilterPid();

int32_t AddNoFilterEvents(const std::vector<std::string>& events);

int32_t ClearNoFilterEvents();

extern "C" {
void FilterAppTrace(const char* app, pid_t pid);
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // __cplusplus

#endif // HITRACE_OPTION_H