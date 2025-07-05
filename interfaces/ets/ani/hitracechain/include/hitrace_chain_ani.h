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

#ifndef HITRACE_CHAIN_ANI_H
#define HITRACE_CHAIN_ANI_H

#include <ani.h>
#include <unordered_map>
#include "hitrace_chain_ani_util.h"

namespace OHOS {
namespace HiviewDFX {
class HiTraceChainAni {
public:
    static ani_object Begin(ani_env* env, ani_string nameAni, ani_object flagAni);
    static ani_object Result(ani_env* env, HiTraceId& traceId);
    static void End(ani_env* env, ani_object HiTraceId);
    static ani_object GetId(ani_env* env);
    static void SetId(ani_env* env, ani_object HiTraceId);
    static void ClearId(ani_env* env);
    static ani_object CreateSpan(ani_env* env);
    static void Tracepoint(ani_env* env, ani_enum_item modeAni,
        ani_enum_item typeAni, ani_object HiTraceIdAni, ani_string msgAni);
    static ani_boolean IsValid(ani_env* env, ani_object HiTraceIdAni);
    static ani_boolean IsFlagEnabled(ani_env* env, ani_object HiTraceIdAni,
        ani_enum_item flagAni);
    static void EnableFlag(ani_env* env, ani_object HiTraceIdAni, ani_enum_item flagAni);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // HITRACE_CHAIN_ANI_H
