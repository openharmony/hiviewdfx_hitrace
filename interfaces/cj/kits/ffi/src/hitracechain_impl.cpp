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

#include "hitracechain_impl.h"
#include <string>

using namespace OHOS::HiviewDFX;

#ifndef TRACE_DOMAIN
#define TRACE_DOMAIN 0xD002D33
#endif

namespace OHOS {
namespace CJSystemapi {

HiTraceId HiTraceChainImpl::Begin(const char* name, int flags)
{
    std::string traceName = (name == nullptr) ? "" : name;
    return HiTraceChain::Begin(traceName, flags, TRACE_DOMAIN);
}

void HiTraceChainImpl::End(const HiTraceId& id)
{
    return HiTraceChain::End(id, TRACE_DOMAIN);
}

HiTraceId HiTraceChainImpl::GetId()
{
    return HiTraceChain::GetId();
}

void HiTraceChainImpl::SetId(const HiTraceId& id)
{
    return HiTraceChain::SetId(id);
}

void HiTraceChainImpl::ClearId()
{
    return HiTraceChain::ClearId();
}

HiTraceId HiTraceChainImpl::CreateSpan()
{
    return HiTraceChain::CreateSpan();
}

void HiTraceChainImpl::Tracepoint(uint32_t mode, uint32_t type, HiTraceId id, const char* msg)
{
    HiTraceCommunicationMode communicationMode = HiTraceCommunicationMode(mode);
    HiTraceTracepointType tracePointType = HiTraceTracepointType(type);
    return HiTraceChain::Tracepoint(communicationMode, tracePointType, id, TRACE_DOMAIN, "%s", msg);
}

bool HiTraceChainImpl::IsValid(const HiTraceId& id)
{
    return id.IsValid();
}

bool HiTraceChainImpl::IsFlagEnabled(const HiTraceId& traceId, int32_t flag)
{
    HiTraceFlag traceFlag = HiTraceFlag(flag);
    bool isFalgEnabled = traceId.IsFlagEnabled(traceFlag);
    return isFalgEnabled;
}

void HiTraceChainImpl::EnableFlag(HiTraceId& traceId, int32_t flag)
{
    HiTraceFlag traceFlag = HiTraceFlag(flag);
    traceId.EnableFlag(traceFlag);
}

} // CJSystemapi
} // OHOS

