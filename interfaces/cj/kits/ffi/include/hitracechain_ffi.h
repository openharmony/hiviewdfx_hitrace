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

#ifndef HITRACECHAIN_FFI_H
#define HITRACECHAIN_FFI_H

#include "hitraceid.h"
#include <cstdint>

extern "C" {
    struct CHiTraceId {
        uint64_t chainId;
        uint64_t spanId;
        uint64_t parentSpanId;
        int32_t flags;
    };

    static CHiTraceId Parse(OHOS::HiviewDFX::HiTraceId id)
    {
        CHiTraceId traceId;
        traceId.chainId = id.GetChainId();
        traceId.spanId = id.GetSpanId();
        traceId.parentSpanId = id.GetParentSpanId();
        traceId.flags = id.GetFlags();
        return traceId;
    }

    static OHOS::HiviewDFX::HiTraceId Parse(CHiTraceId id)
    {
        OHOS::HiviewDFX::HiTraceId traceId;
        traceId.SetChainId(id.chainId);
        traceId.SetSpanId(id.spanId);
        traceId.SetParentSpanId(id.parentSpanId);
        traceId.SetFlags(id.flags);
        return traceId;
    }

    CHiTraceId FfiOHOSHiTraceChainBegin(const char* name, int flag);
    void FfiOHOSHiTraceChainEnd(CHiTraceId id);
    CHiTraceId FfiOHOSHiTraceChainGetId();
    void FfiOHOSHiTraceChainSetId(CHiTraceId id);
    void FfiOHOSHiTraceChainClearId();
    CHiTraceId FfiOHOSHiTraceChainCreateSpan();
    void FfiOHOSHiTraceChainTracepoint(uint32_t mode, uint32_t type, CHiTraceId id, const char* str);
    bool FfiOHOSHiTraceChainIsValid(CHiTraceId id);
    bool FfiOHOSHiTraceChainIsFlagEnabled(CHiTraceId id, int32_t flag);
    void FfiOHOSHiTraceChainEnableFlag(CHiTraceId id, int32_t flag);
}

#endif