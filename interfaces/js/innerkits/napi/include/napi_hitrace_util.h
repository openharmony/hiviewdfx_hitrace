/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef HIVIEWDFX_NAPI_HITRACE_UTIL_H
#define HIVIEWDFX_NAPI_HITRACE_UTIL_H

#include <functional>

#include "hitrace/trace.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace HiviewDFX {
constexpr int NAPI_VALUE_STRING_LEN = 10240;
class NapiHitraceUtil {
public:
    static napi_value InitUndefinedObj(napi_env env);
    static bool NapiTypeCheck(napi_env env, const napi_value& jsObj,
        const napi_valuetype typeName);
    static void SetPropertyInt32(napi_env env, napi_value& object,
        std::string propertyName, int32_t value);
    static void SetPropertyInt64(napi_env env, napi_value& object,
        std::string propertyName, int64_t value);
    static void SetPropertyStringUtf8(napi_env env, napi_value& object,
        std::string propertyName, std::string value);
    static void CreateHiTraceIdObject(napi_env env, HiTraceId& traceId,
        napi_value& valueObject);
    static uint32_t GetPropertyInt32(napi_env env, napi_value& object,
        std::string propertyName);
    static uint64_t GetPropertyInt64(napi_env env, napi_value& object,
        std::string propertyName);
    static void TransHiTraceIdObjectToNative(napi_env env, HiTraceId& traceId,
        napi_value& valueObject);
    static void EnableTraceIdObjectFlag(napi_env env, HiTraceId& traceId, napi_value& traceIdObject);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // HIVIEWDFX_NAPI_HITRACE_UTIL_H