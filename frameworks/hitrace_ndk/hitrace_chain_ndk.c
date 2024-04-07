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

#include <stdbool.h>

#include "hitrace/hitracechainc.h"

typedef HiTraceIdValid HiTraceId_Valid;
typedef HiTraceVersion HiTrace_Version;
typedef HiTraceFlag HiTrace_Flag;
typedef HiTraceTracepointType HiTrace_Tracepoint_Type;
typedef HiTraceCommunicationMode HiTrace_Communication_Mode;
typedef HiTraceIdStruct HiTraceId;

HiTraceId OH_HiTrace_BeginChain(const char* name, int flags)
{
    return HiTraceChainBegin(name, flags);
}

void OH_HiTrace_EndChain()
{
    HiTraceId id = HiTraceChainGetId();
    HiTraceChainEnd(&id);
}

HiTraceId OH_HiTrace_GetId()
{
    return HiTraceChainGetId();
}

void OH_HiTrace_SetId(const HiTraceId* id)
{
    HiTraceChainSetId(id);
}

void OH_HiTrace_ClearId(void)
{
    HiTraceChainClearId();
}

HiTraceId OH_HiTrace_CreateSpan(void)
{
    return HiTraceChainCreateSpan();
}

void OH_HiTrace_Tracepoint(HiTrace_Communication_Mode mode, HiTrace_Tracepoint_Type type,
                           const HiTraceId* id, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    HiTraceChainTracepointExWithArgs(mode, type, id, fmt, args);
    va_end(args);
}

void OH_HiTrace_InitId(HiTraceId* id)
{
    HiTraceChainInitId(id);
}

void OH_HiTrace_IdFromBytes(HiTraceId *id, const uint8_t* pIdArray, int len)
{
    *id = HiTraceChainBytesToId(pIdArray, len);
}

bool OH_HiTrace_IsIdValid(const HiTraceId* id)
{
    return HiTraceChainIsValid(id);
}

bool OH_HiTrace_IsFlagEnabled(const HiTraceId* id, HiTrace_Flag flag)
{
    return HiTraceChainIsFlagEnabled(id, flag);
}

void OH_HiTrace_EnableFlag(const HiTraceId* id, HiTrace_Flag flag)
{
    HiTraceChainEnableFlag((HiTraceId*)id, flag);
}

int OH_HiTrace_GetFlags(const HiTraceId* id)
{
    return HiTraceChainGetFlags(id);
}

void OH_HiTrace_SetFlags(HiTraceId* id, int flags)
{
    HiTraceChainSetFlags(id, flags);
}

uint64_t OH_HiTrace_GetChainId(const HiTraceId* id)
{
    return HiTraceChainGetChainId(id);
}

void OH_HiTrace_SetChainId(HiTraceId* id, uint64_t chainId)
{
    HiTraceChainSetChainId(id, chainId);
}

uint64_t OH_HiTrace_GetSpanId(const HiTraceId* id)
{
    return HiTraceChainGetSpanId(id);
}

void OH_HiTrace_SetSpanId(HiTraceId* id, uint64_t spanId)
{
    HiTraceChainSetSpanId(id, spanId);
}

uint64_t OH_HiTrace_GetParentSpanId(const HiTraceId* id)
{
    return HiTraceChainGetParentSpanId(id);
}

void OH_HiTrace_SetParentSpanId(HiTraceId* id, uint64_t parentSpanId)
{
    HiTraceChainSetParentSpanId(id, parentSpanId);
}

int OH_HiTrace_IdToBytes(const HiTraceId* id, uint8_t* pIdArray, int len)
{
    return HiTraceChainIdToBytes(id, pIdArray, len);
}