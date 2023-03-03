/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "hitracechain_c_wrapper.h"

#include "hitracechain_inner.h"

void HiTraceChainTracepointExWrapper(int mode, int type, const HiTraceIdStruct* pId, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    HiTraceChainTracepointInner(mode, type, pId, fmt, args);
    va_end(args);
    return;
}

int HiTraceChainIsFlagEnabledWrapper(const HiTraceIdStruct* pId, int flag)
{
    return HiTraceChainIsFlagEnabled(pId, flag);
}

void HiTraceChainEnableFlagWrapper(HiTraceIdStruct* pId, int flag)
{
    HiTraceChainEnableFlag(pId, flag);
}

int HiTraceChainIsValidWrapper(const HiTraceIdStruct* pId)
{
    return HiTraceChainIsValid(pId);
}

void HiTraceChainSetFlagsWrapper(HiTraceIdStruct* pId, int flags)
{
    HiTraceChainSetFlags(pId, flags);
}

int HiTraceChainGetFlagsWrapper(const HiTraceIdStruct* pId)
{
    return HiTraceChainGetFlags(pId);
}

void HiTraceChainSetChainIdWrapper(HiTraceIdStruct* pId, uint64_t chainId)
{
    HiTraceChainSetChainId(pId, chainId);
}

uint64_t HiTraceChainGetChainIdWrapper(const HiTraceIdStruct* pId)
{
    return HiTraceChainGetChainId(pId);
}

void HiTraceChainSetSpanIdWrapper(HiTraceIdStruct* pId, uint64_t spanId)
{
    HiTraceChainSetSpanId(pId, spanId);
}

uint64_t HiTraceChainGetSpanIdWrapper(const HiTraceIdStruct* pId)
{
    return HiTraceChainGetSpanId(pId);
}

void HiTraceChainSetParentSpanIdWrapper(HiTraceIdStruct* pId, uint64_t parentSpanId)
{
    HiTraceChainSetParentSpanId(pId, parentSpanId);
}

uint64_t HiTraceChainGetParentSpanIdWrapper(const HiTraceIdStruct* pId)
{
    return HiTraceChainGetParentSpanId(pId);
}

int HiTraceChainIdToBytesWrapper(const HiTraceIdStruct* pId, uint8_t* pIdArray, int len)
{
    return HiTraceChainIdToBytes(pId, pIdArray, len);
}

HiTraceIdStruct HiTraceChainBytesToIdWrapper(const uint8_t* pIdArray, int len)
{
    return HiTraceChainBytesToId(pIdArray, len);
}