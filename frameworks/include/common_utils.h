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

#ifndef OHOS_HIVIEWDFX_HITRACE_COMMON_UTILS_H
#define OHOS_HIVIEWDFX_HITRACE_COMMON_UTILS_H

#include <string>

#include "hilog/log.h"

using OHOS::HiviewDFX::HiLog;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

constexpr uint64_t HITRACE_TAG = 0xD002D33;
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HITRACE_TAG, "HitraceInfo"};

/**
 * Canonicalization and verification of file paths.
*/
std::string CanonicalizeSpecPath(const char* src);


} // Hitrace
} // HiviewDFX
} // OHOS

#endif // OHOS_HIVIEWDFX_HITRACE_COMMON_UTILS_H