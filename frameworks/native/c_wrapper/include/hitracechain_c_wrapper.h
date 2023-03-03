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

#ifndef HIVIEWDFX_HITRACECHAIN_C_WRAPPER_H
#define HIVIEWDFX_HITRACECHAIN_C_WRAPPER_H

#include "hitrace/hitracechainc.h"

#ifdef __cplusplus
extern "C" {
#endif

// rust ffi border redefinition adapts for function HiTraceChainTracepointEx.
void HiTraceChainTracepointExWrapper(int mode, int type, const HiTraceIdStruct* pId, const char* fmt, ...)
    __attribute__((__format__(os_log, 4, 5)));

// rust ffi border redefinition adapts for function HiTraceChainIsFlagEnabled.
int HiTraceChainIsFlagEnabledWrapper(const HiTraceIdStruct* pId, int flag);

// rust ffi border redefinition adapts for function HiTraceChainEnableFlag.
void HiTraceChainEnableFlagWrapper(HiTraceIdStruct* pId, int flag);

// rust ffi border redefinition adapts for function HiTraceChainIsValid.
int HiTraceChainIsValidWrapper(const HiTraceIdStruct* pId);

// rust ffi border redefinition adapts for function HiTraceChainSetFlags.
void HiTraceChainSetFlagsWrapper(HiTraceIdStruct* pId, int flags);

// rust ffi border redefinition adapts for function HiTraceChainGetFlags.
int HiTraceChainGetFlagsWrapper(const HiTraceIdStruct* pId);

// rust ffi border redefinition adapts for function HiTraceChainSetChainId.
void HiTraceChainSetChainIdWrapper(HiTraceIdStruct* pId, uint64_t chainId);

// rust ffi border redefinition adapts for function HiTraceChainGetChainId.
uint64_t HiTraceChainGetChainIdWrapper(const HiTraceIdStruct* pId);

// rust ffi border redefinition adapts for function HiTraceChainSetSpanId.
void HiTraceChainSetSpanIdWrapper(HiTraceIdStruct* pId, uint64_t spanId);

// rust ffi border redefinition adapts for function HiTraceChainGetSpanId.
uint64_t HiTraceChainGetSpanIdWrapper(const HiTraceIdStruct* pId);

// rust ffi border redefinition adapts for function HiTraceChainSetParentSpanId.
void HiTraceChainSetParentSpanIdWrapper(HiTraceIdStruct* pId, uint64_t parentSpanId);

// rust ffi border redefinition adapts for function HiTraceChainGetParentSpanId.
uint64_t HiTraceChainGetParentSpanIdWrapper(const HiTraceIdStruct* pId);

// rust ffi border redefinition adapts for function HiTraceChainIdToBytes.
int HiTraceChainIdToBytesWrapper(const HiTraceIdStruct* pId, uint8_t* pIdArray, int len);

// rust ffi border redefinition adapts for function HiTraceChainBytesToId.
HiTraceIdStruct HiTraceChainBytesToIdWrapper(const uint8_t* pIdArray, int len);

#ifdef __cplusplus
}
#endif

#endif //HIVIEWDFX_HITRACECHAIN_C_WRAPPER_H