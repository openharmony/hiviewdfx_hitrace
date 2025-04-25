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

#include <string>

namespace OHOS {
namespace HiviewDFX {
constexpr uint64_t INVALID_CHAIN_ID = 0;
constexpr uint32_t UINT32_T_PRO_DEFAULT_VALUE = 0;
constexpr uint64_t UINT64_T_PRO_DEFAULT_VALUE = 0;

const std::string FUNC_NAME_GETLONG = "getLong";
const std::string FUNC_NAME_UNBOXED = "unboxed";
const std::string CLASS_NAME_INT = "Lstd/core/Int;";
const std::string CLASS_NAME_DOUBLE = "Lstd/core/Double;";
const std::string CLASS_NAME_BIGINT = "Lescompat/BigInt;";

const std::string CLASS_NAME_HITRACEID = "L@ohos/hiTraceChain/HiTraceIdAni;";
const std::string CLASS_NAME_HITRACECHAIN = "L@ohos/hiTraceChain/hiTraceChain;";
const std::string CLASS_NAME_HITRACEID_CHAINID = "L@ohos/hiTraceChain/HiTraceIdAni/chainId;";
const std::string CLASS_NAME_HITRACEID_SPANID = "L@ohos/hiTraceChain/HiTraceIdAni/spanId;";
const std::string CLASS_NAME_HITRACEID_PARENTSSPANID = "L@ohos/hiTraceChain/HiTraceIdAni/parentSpanId;";
const std::string CLASS_NAME_HITRACEID_FLAGS = "L@ohos/hiTraceChain/HiTraceIdAni/flags;";
} // namespace HiviewDFX
} // namespace OHOS

#endif // HITRACE_CHAIN_ANI_PARAMETER_NAME_H
