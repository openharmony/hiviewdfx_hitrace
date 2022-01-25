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
 
 #include "napi_hitrace_init.h"

#include <functional>
#include <map>

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
using ClassInitFunc = std::function<void(napi_env, std::map<const char*, napi_value>&)>;

const std::string HITRACE_FLAG_ENUM_NAME = "HiTraceFlag";
constexpr int DEFAULT_HITRACE_FLAG = 0;
constexpr int INCLUDE_ASYNC_HITRACE_FLAG = 1;
constexpr int DONOT_CREATE_SPAN_HITRACE_FLAG = 1 << 1;
constexpr int TP_INFO_HITRACE_FLAG = 1 << 2;
constexpr int NO_BE_INFO_HITRACE_FLAG = 1 << 3;
constexpr int DONOT_ENABLE_LOG_HITRACE_FLAG = 1 << 4;
constexpr int FAILURE_TRIGGER_HITRACE_FLAG = 1 << 5;
constexpr int D2D_TP_INFO_HITRACE_FLAG = 1 << 6;

const std::string HITRACE_TRACE_POINT_TYPE_ENUM_NAME = "HiTraceTracePointType";
constexpr int CS_HITRACE_TRACE_POINT_TYPE = 0;
constexpr int CR_HITRACE_TRACE_POINT_TYPE = 1;
constexpr int SS_HITRACE_TRACE_POINT_TYPE = 2;
constexpr int SR_HITRACE_TRACE_POINT_TYPE = 3;
constexpr int GENERAL_HITRACE_TRACE_POINT_TYPE = 4;

const std::string HITRACE_COMMUNICATION_MODE_ENUM_NAME = "HiTraceCommunicationMode";
constexpr int DEFAULT_COMMUNICATE_MODE = 0;
constexpr int THREAD_COMMUNICATE_MODE = 1;
constexpr int PROCESS_COMMUNICATE_MODE = 2;
constexpr int DEVICE_COMMUNICATE_MODE = 3;

napi_value ClassConstructor(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value argv = nullptr;
    napi_value thisArg = nullptr;
    void* data = nullptr;
    napi_get_cb_info(env, info, &argc, &argv, &thisArg, &data);

    napi_value global = 0;
    napi_get_global(env, &global);

    return thisArg;
}

void InitHiTraceFlagEnum(napi_env env, std::map<const char*, napi_value>& traceFlagMap)
{
    napi_value defaultFlag = nullptr;
    napi_create_int32(env, DEFAULT_HITRACE_FLAG, &defaultFlag);
    napi_value includeAsyncFlag = nullptr;
    napi_create_int32(env, INCLUDE_ASYNC_HITRACE_FLAG, &includeAsyncFlag);
    napi_value doNotCreateSpanFlag = nullptr;
    napi_create_int32(env, DONOT_CREATE_SPAN_HITRACE_FLAG, &doNotCreateSpanFlag);
    napi_value tpInfoFlag = nullptr;
    napi_create_int32(env, TP_INFO_HITRACE_FLAG, &tpInfoFlag);
    napi_value noBeInfoFlag = nullptr;
    napi_create_int32(env, NO_BE_INFO_HITRACE_FLAG, &noBeInfoFlag);
    napi_value doNotEnableLogFlag = nullptr;
    napi_create_int32(env, DONOT_ENABLE_LOG_HITRACE_FLAG, &doNotEnableLogFlag);
    napi_value failureTriggerFlag = nullptr;
    napi_create_int32(env, FAILURE_TRIGGER_HITRACE_FLAG, &failureTriggerFlag);
    napi_value d2dTpInfoFlag = nullptr;
    napi_create_int32(env, D2D_TP_INFO_HITRACE_FLAG, &d2dTpInfoFlag);

    traceFlagMap["DEFAULT"] = defaultFlag;
    traceFlagMap["INCLUDE_ASYNC"] = includeAsyncFlag;
    traceFlagMap["DONOT_CREATE_SPAN"] = doNotCreateSpanFlag;
    traceFlagMap["TP_INFO"] = tpInfoFlag;
    traceFlagMap["NO_BE_INFO"] = noBeInfoFlag;
    traceFlagMap["DONOT_ENABLE_LOG"] = doNotEnableLogFlag;
    traceFlagMap["FAILURE_TRIGGER"] = failureTriggerFlag;
    traceFlagMap["D2D_TP_INFO"] = d2dTpInfoFlag;
}

void InitHiTraceTracePointTypeEnum(napi_env env,
    std::map<const char*, napi_value>& tracePointMap)
{
    napi_value csTracePoint = nullptr;
    napi_create_int32(env, CS_HITRACE_TRACE_POINT_TYPE, &csTracePoint);
    napi_value crTracePoint = nullptr;
    napi_create_int32(env, CR_HITRACE_TRACE_POINT_TYPE, &crTracePoint);
    napi_value ssTracePoint = nullptr;
    napi_create_int32(env, SS_HITRACE_TRACE_POINT_TYPE, &ssTracePoint);
    napi_value srTracePoint = nullptr;
    napi_create_int32(env, SR_HITRACE_TRACE_POINT_TYPE, &srTracePoint);
    napi_value generalTracePoint = nullptr;
    napi_create_int32(env, GENERAL_HITRACE_TRACE_POINT_TYPE, &generalTracePoint);

    tracePointMap["CS"] = csTracePoint;
    tracePointMap["CR"] = crTracePoint;
    tracePointMap["SS"] = ssTracePoint;
    tracePointMap["SR"] = srTracePoint;
    tracePointMap["GENERAL"] = generalTracePoint;
}

void InitHiTraceCommunicationModeEnum(napi_env env,
    std::map<const char*, napi_value>& commuicationModeMap)
{
    napi_value defaultMode = nullptr;
    napi_create_int32(env, DEFAULT_COMMUNICATE_MODE, &defaultMode);
    napi_value threadMode = nullptr;
    napi_create_int32(env, THREAD_COMMUNICATE_MODE, &threadMode);
    napi_value processMode = nullptr;
    napi_create_int32(env, PROCESS_COMMUNICATE_MODE, &processMode);
    napi_value deviceMode = nullptr;
    napi_create_int32(env, DEVICE_COMMUNICATE_MODE, &deviceMode);

    commuicationModeMap["DEFAULT"] = defaultMode;
    commuicationModeMap["THREAD"] = threadMode;
    commuicationModeMap["PROCESS"] = processMode;
    commuicationModeMap["DEVICE"] = deviceMode;
}

void InitConstClassByName(napi_env env, napi_value exports, std::string name)
{
    std::map<const char*, napi_value> propertyMap;
    if (name == HITRACE_FLAG_ENUM_NAME) {
        InitHiTraceFlagEnum(env, propertyMap);
    } else if (name == HITRACE_TRACE_POINT_TYPE_ENUM_NAME) {
        InitHiTraceTracePointTypeEnum(env, propertyMap);
    } else if (name == HITRACE_COMMUNICATION_MODE_ENUM_NAME) {
        InitHiTraceCommunicationModeEnum(env, propertyMap);
    } else {
        return;
    }
    int i = 0;
    napi_property_descriptor descriptors[propertyMap.size()];
    for (auto it : propertyMap) {
        descriptors[i++] = DECLARE_NAPI_STATIC_PROPERTY(it.first, it.second);
    }
    napi_value result = nullptr;
    napi_define_class(env, name.c_str(), NAPI_AUTO_LENGTH, ClassConstructor,
        nullptr, sizeof(descriptors) / sizeof(*descriptors), descriptors, &result);
    napi_set_named_property(env, exports, name.c_str(), result);
}
}

napi_value InitNapiClass(napi_env env, napi_value exports)
{
    InitConstClassByName(env, exports, HITRACE_FLAG_ENUM_NAME);
    InitConstClassByName(env, exports, HITRACE_TRACE_POINT_TYPE_ENUM_NAME);
    InitConstClassByName(env, exports, HITRACE_COMMUNICATION_MODE_ENUM_NAME);
    return exports;
}
} // namespace HiviewDFX
} // namespace OHOS