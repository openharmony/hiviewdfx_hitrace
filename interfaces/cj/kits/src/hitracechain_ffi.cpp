/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License"){
}
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

#include "hitracechain_ffi.h"
#include "hitracechain_impl.h"

using namespace OHOS::CJSystemapi;

extern "C" {
    CHiTraceId FfiOHOSHiTraceChainBegin(const char* name, int flag)
    {
        OHOS::HiviewDFX::HiTraceId id = HiTraceChainImpl::Begin(name, flag);
        return Parse(id);
    }

    void FfiOHOSHiTraceChainEnd(CHiTraceId id)
    {
        OHOS::HiviewDFX::HiTraceId traceId = Parse(id);
        HiTraceChainImpl::End(traceId);
    }

    CHiTraceId FfiOHOSHiTraceChainGetId()
    {
        OHOS::HiviewDFX::HiTraceId id = HiTraceChainImpl::GetId();
        return Parse(id);
    }

    void FfiOHOSHiTraceChainSetId(CHiTraceId id)
    {
        OHOS::HiviewDFX::HiTraceId traceId = Parse(id);
        HiTraceChainImpl::SetId(traceId);
    }

    void FfiOHOSHiTraceChainClearId()
    {
        HiTraceChainImpl::ClearId();
    }

    CHiTraceId FfiOHOSHiTraceChainCreateSpan()
    {
        OHOS::HiviewDFX::HiTraceId id = HiTraceChainImpl::CreateSpan();
        return Parse(id);
    }

    void FfiOHOSHiTraceChainTracepoint(uint32_t mode, uint32_t type, CHiTraceId id, const char* str)
    {
        OHOS::HiviewDFX::HiTraceId traceId = Parse(id);
        HiTraceChainImpl::Tracepoint(mode, type, traceId, str);
    }

    bool FfiOHOSHiTraceChainIsValid(CHiTraceId id)
    {
        OHOS::HiviewDFX::HiTraceId traceId = Parse(id);
        return HiTraceChainImpl::IsValid(traceId);
    }

    bool FfiOHOSHiTraceChainIsFlagEnabled(CHiTraceId id, int32_t flag)
    {
        OHOS::HiviewDFX::HiTraceId traceId = Parse(id);
        return HiTraceChainImpl::IsFlagEnabled(traceId, flag);
    }

    void FfiOHOSHiTraceChainEnableFlag(CHiTraceId id, int32_t flag)
    {
        OHOS::HiviewDFX::HiTraceId traceId = Parse(id);
        return HiTraceChainImpl::EnableFlag(traceId, flag);
    }
}