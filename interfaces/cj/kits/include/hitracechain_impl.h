/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef HITRACECHAIN_IMPL_H
#define HITRACECHAIN_IMPL_H

#include "tracechain.h"

namespace OHOS {
namespace CJSystemapi {

class HiTraceChainImpl {
public:
    static HiviewDFX::HiTraceId Begin(const char* name, int taskId);
    static void End(const HiviewDFX::HiTraceId& id);
    static HiviewDFX::HiTraceId GetId();
    static void SetId(const HiviewDFX::HiTraceId& id);
    static void ClearId();
    static HiviewDFX::HiTraceId CreateSpan();
    static void Tracepoint(uint32_t mode, uint32_t type, HiviewDFX::HiTraceId id, const char* fmt);
    static bool IsValid(const HiviewDFX::HiTraceId& id);
    static bool IsFlagEnabled(const HiviewDFX::HiTraceId& traceId, int32_t flag);
    static void EnableFlag(const HiviewDFX::HiTraceId& traceId, int32_t flag);
};

} // CJSystemapi
} // OHOS

#endif