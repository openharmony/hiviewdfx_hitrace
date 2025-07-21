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
    static bool IsRefUndefined(ani_env* env, ani_ref ref);
    static ani_status GetAniStringValue(ani_env* env, ani_ref aniStrRef, std::string& name);
    static ani_long GetAniBigIntValue(ani_env* env, ani_ref elementRef);
    static ani_int GetAniIntValue(ani_env* env, ani_ref elementRef);
    static ani_status AniEnumToInt(ani_env* env, ani_enum_item enumItem, int& value);
    static ani_object CreateAniBigInt(ani_env* env, uint64_t value);
    static ani_object CreateAniInt(ani_env* env, uint64_t value);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // HITRACE_CHAIN_ANI_UTIL_H
