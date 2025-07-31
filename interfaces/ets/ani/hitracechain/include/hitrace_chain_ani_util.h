/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef HITRACE_CHAIN_ANI_UTIL_H
#define HITRACE_CHAIN_ANI_UTIL_H

#include <ani.h>
#include <map>
#include <string>
#include "hilog/log.h"
#include "hilog/log_cpp.h"

namespace OHOS {
namespace HiviewDFX {

class HiTraceChainAniUtil {
public:
    static bool GetAniStringValue(ani_env* env, ani_string strAni, std::string& content);
    static bool GetAniBigIntValue(ani_env* env, ani_object bigIntObj, ani_long& value);
    static bool CreateAniBigInt(ani_env* env, uint64_t value, ani_object& bigIntObj);
    static bool CreateAniInt(ani_env* env, uint64_t value, ani_object& intObj);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // HITRACE_CHAIN_ANI_UTIL_H
