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

#ifndef HITRACE_CHAIN_ANI_PARAMETER_NAME_H
#define HITRACE_CHAIN_ANI_PARAMETER_NAME_H

namespace OHOS {
namespace HiviewDFX {
const char* const FUNC_NAME_GETLONG = "getLong";
const char* const FUNC_NAME_UNBOXED = "unboxed";
const char* const CLASS_NAME_INT = "std.core.Int";
const char* const CLASS_NAME_DOUBLE = "std.core.Double";
const char* const CLASS_NAME_BIGINT = "escompat.BigInt";

const char* const CLASS_NAME_HITRACEID = "@ohos.hiTraceChain.HiTraceIdAni";
const char* const CLASS_NAME_HITRACECHAIN = "@ohos.hiTraceChain.hiTraceChain";
const char* const CLASS_NAME_HITRACEID_CHAINID = "@ohos.hiTraceChain.HiTraceIdAni.chainId";
const char* const CLASS_NAME_HITRACEID_SPANID = "@ohos.hiTraceChain.HiTraceIdAni.spanId";
const char* const CLASS_NAME_HITRACEID_PARENTSSPANID = "@ohos.hiTraceChain.HiTraceIdAni.parentSpanId";
const char* const CLASS_NAME_HITRACEID_FLAGS = "@ohos.hiTraceChain.HiTraceIdAni.flags";
} // namespace HiviewDFX
} // namespace OHOS

#endif // HITRACE_CHAIN_ANI_PARAMETER_NAME_H
