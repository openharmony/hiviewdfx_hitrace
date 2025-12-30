/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <functional>
#include <hilog/log.h>
#include <map>
#include <string>

#include "hitrace_meter.h"
#include "hitrace_meter_c.h"
#include "napi_hitrace_meter.h"

using namespace OHOS::HiviewDFX;
namespace {
enum ArgcNum {
    ARGC_ONE = 1,
    ARGC_TWO = 2,
    ARGC_THREE = 3,
    ARGC_FOURTH = 4,
    ARGC_FIVE = 5,
};

enum ArgIndex {
    ARG_FIRST = 0,
    ARG_SECOND = 1,
    ARG_THIRD = 2,
    ARG_FOURTH = 3,
    ARG_FIFTH = 4,
};

const char* const HITRACE_OUTPUT_LEVEL = "HiTraceOutputLevel";

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceMeterNapi"
#endif

using STR_NUM_PARAM_FUNC = std::function<bool(std::string, napi_value&)>;
constexpr int32_t INVALID_PARAMETER = -2;
constexpr int32_t INVALID_INDEX = -2;
struct CallbackContext {
    napi_threadsafe_function tsFunc = nullptr;
    bool traceEnable;
};

bool TypeCheck(const napi_env& env, const napi_value& value, const napi_valuetype expectType)
{
    napi_valuetype valueType;
    napi_status status = napi_typeof(env, value, &valueType);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get the type of the argument.");
        return false;
    }
    if (valueType != expectType) {
        HILOG_DEBUG(LOG_CORE, "Type of the parameter is invalid.");
        return false;
    }
    return true;
}

bool GetStringParam(const napi_env& env, const napi_value& value, std::string& dest)
{
    constexpr int nameMaxSize = 1024;
    char buf[nameMaxSize] = {0};
    size_t len = 0;
    napi_status status = napi_get_value_string_utf8(env, value, buf, nameMaxSize, &len);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get string argument.");
        return false;
    }
    dest = std::string {buf};
    return true;
}

bool ParseStringParam(const napi_env& env, const napi_value& value, std::string& dest)
{
    if (TypeCheck(env, value, napi_string)) {
        if (GetStringParam(env, value, dest)) {
            return true;
        }
    } else if (TypeCheck(env, value, napi_undefined) || TypeCheck(env, value, napi_null)) {
        dest = "";
        return true;
    }
    return false;
}

bool ParseInt32Param(const napi_env& env, const napi_value& value, int& dest)
{
    if (!TypeCheck(env, value, napi_number)) {
        return false;
    }
    napi_status status = napi_get_value_int32(env, value, &dest);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get int32 argument.");
        return false;
    }
    return true;
}

bool ParseInt64Param(const napi_env& env, const napi_value& value, int64_t& dest)
{
    if (!TypeCheck(env, value, napi_number)) {
        return false;
    }
    napi_status status = napi_get_value_int64(env, value, &dest);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get int64 argument.");
        return false;
    }
    return true;
}

bool JsStrNumParamsFunc(napi_env& env, napi_callback_info& info, STR_NUM_PARAM_FUNC nativeCall)
{
    size_t argc = static_cast<size_t>(ARGC_TWO);
    napi_value argv[ARGC_TWO];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return false;
    }
    if (argc != ARGC_TWO) {
        HILOG_ERROR(LOG_CORE, "Wrong number of parameters.");
        return false;
    }
    std::string name;
    if (!ParseStringParam(env, argv[ARG_FIRST], name)) {
        return false;
    }
    if (!nativeCall(name, argv[ARG_SECOND])) {
        return false;
    }
    return true;
}
}

static napi_value JSTraceStart(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE;
    napi_value argv[ARGC_THREE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    std::string name;
    if (!ParseStringParam(env, argv[ARG_FIRST], name)) {
        return nullptr;
    }
    int taskId = 0;
    if (!ParseInt32Param(env, argv[ARG_SECOND], taskId)) {
        return nullptr;
    }
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name.c_str(), taskId, "", "");
    return nullptr;
}

static napi_value JSTraceFinish(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_TWO;
    napi_value argv[ARGC_TWO];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    (void)JsStrNumParamsFunc(env, info, [&env] (std::string name, napi_value& nValue) -> bool {
        int taskId = 0;
        if (!ParseInt32Param(env, nValue, taskId)) {
            return false;
        }
        FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name.c_str(), taskId);
        return true;
    });
    return nullptr;
}

static napi_value JSCountTrace(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE;
    napi_value argv[ARGC_THREE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    int32_t level;
    if (!ParseInt32Param(env, argv[ARG_FIRST], level)) {
        return nullptr;
    }

    std::string name;
    if (!ParseStringParam(env, argv[ARG_SECOND], name)) {
        return nullptr;
    }

    int64_t count;
    if (!ParseInt64Param(env, argv[ARG_THIRD], count)) {
        return nullptr;
    }

    CountTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(), count);
    return nullptr;
}

static napi_value JSTraceCount(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_TWO;
    napi_value argv[ARGC_TWO];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    if (argc == ARGC_TWO) {
        (void)JsStrNumParamsFunc(env, info, [&env] (std::string name, napi_value& nValue) -> bool {
            int64_t count = 0;
            if (!ParseInt64Param(env, nValue, count)) {
                return false;
            }
            CountTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name.c_str(), count);
            return true;
        });
    } else {
        JSCountTrace(env, info);
    }
    return nullptr;
}

static napi_value JSStartSyncTrace(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE;
    napi_value argv[ARGC_THREE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    int32_t level;
    if (!ParseInt32Param(env, argv[ARG_FIRST], level)) {
        return nullptr;
    }

    std::string name;
    if (!ParseStringParam(env, argv[ARG_SECOND], name)) {
        return nullptr;
    }

    std::string customArgs;
    if (!ParseStringParam(env, argv[ARG_THIRD], customArgs)) {
        return nullptr;
    }

    StartTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(), customArgs.c_str());
    return nullptr;
}

static napi_value JSFinishSyncTrace(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE;
    napi_value argv[ARGC_THREE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    int32_t level;
    if (!ParseInt32Param(env, argv[ARG_FIRST], level)) {
        return nullptr;
    }

    FinishTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP);
    return nullptr;
}

static napi_value JSStartAsyncTrace(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_FIVE;
    napi_value argv[ARGC_FIVE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    int32_t level;
    if (!ParseInt32Param(env, argv[ARG_FIRST], level)) {
        return nullptr;
    }

    std::string name;
    if (!ParseStringParam(env, argv[ARG_SECOND], name)) {
        return nullptr;
    }

    int32_t taskId;
    if (!ParseInt32Param(env, argv[ARG_THIRD], taskId)) {
        return nullptr;
    }

    std::string customCategory;
    if (!ParseStringParam(env, argv[ARG_FOURTH], customCategory)) {
        return nullptr;
    }

    std::string customArgs;
    if (!ParseStringParam(env, argv[ARG_FIFTH], customArgs)) {
        return nullptr;
    }

    StartAsyncTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(),
                      taskId, customCategory.c_str(), customArgs.c_str());
    return nullptr;
}

static napi_value JSFinishAsyncTrace(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE;
    napi_value argv[ARGC_THREE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    int32_t level;
    if (!ParseInt32Param(env, argv[ARG_FIRST], level)) {
        return nullptr;
    }

    std::string name;
    if (!ParseStringParam(env, argv[ARG_SECOND], name)) {
        return nullptr;
    }

    int32_t taskId;
    if (!ParseInt32Param(env, argv[ARG_THIRD], taskId)) {
        return nullptr;
    }

    FinishAsyncTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(), taskId);
    return nullptr;
}

static napi_value JSIsTraceEnabled(napi_env env, napi_callback_info info)
{
    bool isTagEnable = IsTagEnabled(HITRACE_TAG_APP);
    napi_value val = nullptr;
    napi_get_boolean(env, isTagEnable, &val);
    return val;
}

static void JsExcuteCallBack(void* ctx, bool enable)
{
    CallbackContext* context = reinterpret_cast<CallbackContext*>(ctx);
    if (context != nullptr) {
        context->traceEnable = enable;
        napi_acquire_threadsafe_function(context->tsFunc);
        napi_call_threadsafe_function(context->tsFunc, context, napi_tsfn_nonblocking);
    }
}

static void JsDeleteCallback(void* ctx)
{
    CallbackContext* context = reinterpret_cast<CallbackContext*>(ctx);
    if (context != nullptr) {
        napi_release_threadsafe_function(context->tsFunc, napi_tsfn_release);
        delete context;
    }
}

static void JsThreadSafeCall(napi_env env, napi_value callback, void* context, void* data)
{
    if (env == nullptr) {
        HILOG_ERROR(LOG_CORE, "JsThreadSafeCall: env is nullptr.");
        return;
    }
    CallbackContext* callbackContext = reinterpret_cast<CallbackContext*>(data);
    if (callbackContext == nullptr) {
        HILOG_ERROR(LOG_CORE, "JsThreadSafeCall: callbackContext is nullptr.");
        return;
    }

    napi_value argv;
    napi_get_boolean(env, callbackContext->traceEnable, &argv);
    napi_value result = nullptr;
    napi_call_function(env, nullptr, callback, 1, &argv, &result);
}

static napi_value JSRegisterTraceListener(napi_env env, napi_callback_info info)
{
    napi_value ret = nullptr;
    napi_create_int32(env, INVALID_PARAMETER, &ret);
    size_t argc = ARGC_ONE;
    napi_value argv[ARGC_ONE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSRegisterTraceListener napi_get_cb_info failed.");
        return ret;
    }

    napi_value callback = argv[ARG_FIRST];
    if (!TypeCheck(env, callback, napi_function)) {
        return ret;
    }

    napi_value workName;
    napi_create_string_utf8(env, "TraceListenerCallback", NAPI_AUTO_LENGTH, &workName);
    CallbackContext* asyncContext = new CallbackContext();
    napi_create_threadsafe_function(env, callback, nullptr, workName, 0, 1, nullptr, nullptr, nullptr,
        JsThreadSafeCall, &asyncContext->tsFunc);

    SetCallbacksNapi(JsExcuteCallBack, JsDeleteCallback);
    int32_t registerRet = RegisterTraceListenerNapi(reinterpret_cast<void *>(asyncContext));
    if (registerRet < 0) {
        JsDeleteCallback(reinterpret_cast<void *>(asyncContext));
    }
    napi_create_int32(env, registerRet, &ret);
    return ret;
}

static napi_value JSUnregisterTraceListener(napi_env env, napi_callback_info info)
{
    napi_value ret = nullptr;
    napi_create_int32(env, INVALID_INDEX, &ret);
    size_t argc = ARGC_ONE;
    napi_value argv[ARGC_ONE];
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSUnregisterTraceListener napi_get_cb_info failed.");
        return ret;
    }
    int32_t index;
    if (!ParseInt32Param(env, argv[ARG_FIRST], index)) {
        return ret;
    }
    int32_t unregisterRet = UnregisterTraceListenerNapi(index);
    napi_create_int32(env, unregisterRet, &ret);
    return ret;
}

static napi_value TraceLevelConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisArg = nullptr;
    napi_status status = napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, nullptr);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "napi_get_cb_info failed.");
        return nullptr;
    }
    napi_get_global(env, nullptr);
    return thisArg;
}

static void InitTraceLevelEnum(napi_env env, napi_value exports)
{
    napi_value debug = nullptr;
    napi_value info = nullptr;
    napi_value critical = nullptr;
    napi_value commercial = nullptr;

    napi_create_int32(env, HITRACE_LEVEL_DEBUG, &debug);
    napi_create_int32(env, HITRACE_LEVEL_INFO, &info);
    napi_create_int32(env, HITRACE_LEVEL_CRITICAL, &critical);
    napi_create_int32(env, HITRACE_LEVEL_COMMERCIAL, &commercial);

    napi_property_descriptor descriptors[] = {
        DECLARE_NAPI_STATIC_PROPERTY("DEBUG", debug),
        DECLARE_NAPI_STATIC_PROPERTY("INFO", info),
        DECLARE_NAPI_STATIC_PROPERTY("CRITICAL", critical),
        DECLARE_NAPI_STATIC_PROPERTY("COMMERCIAL", commercial),
        DECLARE_NAPI_STATIC_PROPERTY("MAX", commercial),
    };

    napi_value result = nullptr;
    napi_define_class(env, HITRACE_OUTPUT_LEVEL, NAPI_AUTO_LENGTH, TraceLevelConstructor,
        nullptr, sizeof(descriptors) / sizeof(*descriptors), descriptors, &result);
    napi_set_named_property(env, exports, HITRACE_OUTPUT_LEVEL, result);
}

/*
 * function for module exports
 */
EXTERN_C_START
static napi_value HiTraceMeterInit(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("startTrace", JSTraceStart),
        DECLARE_NAPI_FUNCTION("finishTrace", JSTraceFinish),
        DECLARE_NAPI_FUNCTION("traceByValue", JSTraceCount),
        DECLARE_NAPI_FUNCTION("startSyncTrace", JSStartSyncTrace),
        DECLARE_NAPI_FUNCTION("finishSyncTrace", JSFinishSyncTrace),
        DECLARE_NAPI_FUNCTION("startAsyncTrace", JSStartAsyncTrace),
        DECLARE_NAPI_FUNCTION("finishAsyncTrace", JSFinishAsyncTrace),
        DECLARE_NAPI_FUNCTION("isTraceEnabled", JSIsTraceEnabled),
        DECLARE_NAPI_FUNCTION("registerTraceListener", JSRegisterTraceListener),
        DECLARE_NAPI_FUNCTION("unregisterTraceListener", JSUnregisterTraceListener),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    InitTraceLevelEnum(env, exports);
    return exports;
}
EXTERN_C_END

/*
 * hiTraceMeter module definition
 */
static napi_module hitracemeter_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = HiTraceMeterInit,
    .nm_modname = "hiTraceMeter",
    .nm_priv = ((void *)0),
    .reserved = {0}
};

/*
 * Module registration
 */
extern "C" __attribute__((constructor)) void RegisterModule(void)
{
    napi_module_register(&hitracemeter_module);
}
