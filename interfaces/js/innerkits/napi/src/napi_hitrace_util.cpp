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

#include "napi_hitrace_util.h"

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
constexpr HiLogLabel LABEL = { LOG_CORE, 0xD002D03, "NapiHitraceUtil" };
napi_value NapiHitraceUtil::InitUndefinedObj(const napi_env env)
{
    napi_value result = nullptr;
    NAPI_CALL(env, napi_get_undefined(env, &result));
    return result;
}

bool NapiHitraceUtil::NapiTypeCheck(const napi_env env, const napi_value& jsObj,
    const napi_valuetype typeName)
{
    napi_valuetype valueType;
    napi_typeof(env, jsObj, &valueType);
    if (valueType != typeName) {
        HiLog::Error(LABEL, "you have called a function with parameters of wrong type.");
        return false;
    }
    return true;
}

void NapiHitraceUtil::CreateHiTraceIdObject(const napi_env env, HiTraceId& traceId,
    napi_value& valueObject)
{
    napi_create_object(env, &valueObject);
    NapiHitraceUtil::SetPropertyInt64(env, valueObject, "chainId", traceId.GetChainId());
    NapiHitraceUtil::SetPropertyInt64(env, valueObject, "spandId", traceId.GetSpanId());
    NapiHitraceUtil::SetPropertyInt64(env, valueObject, "parentSpanId", traceId.GetParentSpanId());
    NapiHitraceUtil::SetPropertyInt32(env, valueObject, "flags", traceId.GetFlags());
}

void NapiHitraceUtil::SetPropertyInt32(napi_env env, napi_value& object,
    std::string propertyName, int32_t value)
{
    napi_value propertyValue = InitUndefinedObj(env);
    napi_create_int32(env, value, &propertyValue);
    napi_set_named_property(env, object, propertyName.c_str(), propertyValue);
}

void NapiHitraceUtil::SetPropertyInt64(napi_env env, napi_value& object,
    std::string propertyName, int64_t value)
{
    napi_value propertyValue = InitUndefinedObj(env);
    napi_create_int64(env, value, &propertyValue);
    napi_set_named_property(env, object, propertyName.c_str(), propertyValue);
}

void NapiHitraceUtil::SetPropertyStringUtf8(napi_env env, napi_value& object,
    std::string propertyName, std::string value)
{
    napi_value propertyValue = InitUndefinedObj(env);
    napi_set_named_property(env, object, propertyName.c_str(), propertyValue);
}

uint32_t NapiHitraceUtil::GetPropertyInt32(napi_env env, napi_value& object,
    std::string propertyName)
{
    napi_value propertyValue = InitUndefinedObj(env);
    napi_get_named_property(env, object, propertyName.c_str(), &propertyValue);
    int32_t value;
    napi_get_value_int32(env, propertyValue, &value);
    return value;
}

uint64_t NapiHitraceUtil::GetPropertyInt64(napi_env env, napi_value& object,
    std::string propertyName)
{
    napi_value propertyValue = InitUndefinedObj(env);
    napi_get_named_property(env, object, propertyName.c_str(), &propertyValue);
    int64_t value;
    napi_get_value_int64(env, propertyValue, &value);
    return value;
}

void NapiHitraceUtil::TransHiTraceIdObjectToNative(napi_env env, HiTraceId& traceId,
    napi_value& valueObject)
{
    uint64_t chainId = NapiHitraceUtil::GetPropertyInt64(env, valueObject, "chainId");
    traceId.SetChainId(chainId);
    uint32_t spanId = NapiHitraceUtil::GetPropertyInt64(env, valueObject, "spanId");
    traceId.SetSpanId(spanId);
    uint32_t parentSpanId = NapiHitraceUtil::GetPropertyInt64(env, valueObject, "parentSpanId");
    traceId.SetParentSpanId(parentSpanId);
    uint32_t flags = NapiHitraceUtil::GetPropertyInt32(env, valueObject, "flags");
    traceId.SetFlags(flags);
}

void NapiHitraceUtil::EnableTraceIdObjectFlag(napi_env env, HiTraceId& traceId,
    napi_value& traceIdObject)
{
    NapiHitraceUtil::SetPropertyInt32(env, traceIdObject, "flags", traceId.GetFlags());
}
} // namespace HiviewDFX
} // namespace OHOS