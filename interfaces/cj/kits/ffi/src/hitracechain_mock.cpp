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

#include "cj_ffi/cj_common_ffi.h"

extern "C" {
FFI_EXPORT int FfiOHOSHiTraceChainBegin = 0;
FFI_EXPORT int FfiOHOSHiTraceChainEnd = 0;
FFI_EXPORT int FfiOHOSHiTraceChainGetId = 0;
FFI_EXPORT int FfiOHOSHiTraceChainSetId = 0;
FFI_EXPORT int FfiOHOSHiTraceChainClearId = 0;
FFI_EXPORT int FfiOHOSHiTraceChainCreateSpan = 0;
FFI_EXPORT int FfiOHOSHiTraceChainTracepoint = 0;
FFI_EXPORT int FfiOHOSHiTraceChainIsValid = 0;
FFI_EXPORT int FfiOHOSHiTraceChainIsFlagEnabled = 0;
FFI_EXPORT int FfiOHOSHiTraceChainEnableFlag = 0;
}