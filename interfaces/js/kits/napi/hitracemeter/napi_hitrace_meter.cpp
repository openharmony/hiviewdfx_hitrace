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
#include <map>
#include <string>
#include <hilog/log.h>
#include "hitrace_meter.h"
#include "napi_hitrace_meter.h"

using namespace OHOS::HiviewDFX;
namespace {
constexpr int FIRST_ARG_INDEX = 0;
constexpr int SECOND_ARG_INDEX = 1;
constexpr int ARGC_NUMBER_TWO = 2;
constexpr int ARGC_NUMBER_THREE = 3;
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "HITRACE_METER_JS"

using STR_NUM_PARAM_FUNC = std::function<bool(std::string, napi_value&)>;
std::map<std::string, uint64_t> g_tagsMap = {
    {"ohos", HITRACE_TAG_OHOS}, {"ability", HITRACE_TAG_ABILITY_MANAGER}, {"camera", HITRACE_TAG_ZCAMERA},
    {"media", HITRACE_TAG_ZMEDIA}, {"image", HITRACE_TAG_ZIMAGE}, {"audio", HITRACE_TAG_ZAUDIO},
    {"distributeddatamgr", HITRACE_TAG_DISTRIBUTEDDATA}, {"graphic", HITRACE_TAG_GRAPHIC_AGP},
    {"ace", HITRACE_TAG_ACE}, {"notification", HITRACE_TAG_NOTIFICATION}, {"misc", HITRACE_TAG_MISC},
    {"multimodalinput", HITRACE_TAG_MULTIMODALINPUT}, {"rpc", HITRACE_TAG_RPC}, {"ark", HITRACE_TAG_ARK},
    {"window", HITRACE_TAG_WINDOW_MANAGER}, {"dscreen", HITRACE_TAG_DISTRIBUTED_SCREEN},
    {"dcamera", HITRACE_TAG_DISTRIBUTED_CAMERA}, {"dhfwk", HITRACE_TAG_DISTRIBUTED_HARDWARE_FWK},
    {"gresource", HITRACE_TAG_GLOBAL_RESMGR}, {"devicemanager", HITRACE_TAG_DEVICE_MANAGER},
    {"samgr", HITRACE_TAG_SAMGR}, {"power", HITRACE_TAG_POWER}, {"dsched", HITRACE_TAG_DISTRIBUTED_SCHEDULE},
    {"dinput", HITRACE_TAG_DISTRIBUTED_INPUT}, {"bluetooth", HITRACE_TAG_BLUETOOTH}, {"ffrt", HITRACE_TAG_FFRT},
    {"commonlibrary", HITRACE_TAG_COMMONLIBRARY}, {"hdf", HITRACE_TAG_HDF}, {"net", HITRACE_TAG_NET},
    {"nweb", HITRACE_TAG_NWEB}, {"daudio", HITRACE_TAG_DISTRIBUTED_AUDIO},
    {"filemanagement", HITRACE_TAG_FILEMANAGEMENT}, {"app", HITRACE_TAG_APP}
};

napi_value ParseParams(napi_env& env, napi_callback_info& info, size_t& argc, napi_value* argv)
{
    napi_value thisVar;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL));
    return nullptr;
}

bool TypeCheck(const napi_env& env, const napi_value& value, const napi_valuetype expectType)
{
    napi_valuetype valueType;
    napi_status status = napi_typeof(env, value, &valueType);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get the type of the argument.");
        return false;
    }
    if (valueType != expectType) {
        HILOG_ERROR(LOG_CORE, "Type of the parameter is invalid.");
        return false;
    }
    return true;
}

void GetStringParam(const napi_env& env, const napi_value& value, std::string& dest)
{
    constexpr int nameMaxSize = 1024;
    char buf[nameMaxSize] = {0};
    size_t len = 0;
    napi_get_value_string_utf8(env, value, buf, nameMaxSize, &len);
    dest = std::string {buf};
}

bool ParseStringParam(const napi_env& env, const napi_value& value, std::string& dest)
{
    if (TypeCheck(env, value, napi_string)) {
        GetStringParam(env, value, dest);
        return true;
    }
    if (TypeCheck(env, value, napi_number)) {
        int64_t destI64;
        napi_get_value_int64(env, value, &destI64);
        dest = std::to_string(destI64);
        return true;
    }
    if (TypeCheck(env, value, napi_undefined)) {
        dest = "undefined";
        return true;
    }
    if (TypeCheck(env, value, napi_null)) {
        dest = "null";
        return true;
    }
    return false;
}

bool ParseInt32Param(const napi_env& env, const napi_value& value, int& dest)
{
    if (!TypeCheck(env, value, napi_number)) {
        return false;
    }
    napi_get_value_int32(env, value, &dest);
    return true;
}

bool ParseInt64Param(const napi_env& env, const napi_value& value, int64_t& dest)
{
    if (!TypeCheck(env, value, napi_number)) {
        return false;
    }
    napi_get_value_int64(env, value, &dest);
    return true;
}

void SetTagsParam(const napi_env& env, const napi_value& value, uint64_t& tags)
{
    uint32_t arrayLength;
    napi_status status = napi_get_array_length(env, value, &arrayLength);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get the length of the array.");
        return;
    }

    for (uint32_t i = 0; i < arrayLength; i++) {
        napi_value tag;
        status = napi_get_element(env, value, i, &tag);
        if (status != napi_ok) {
            HILOG_ERROR(LOG_CORE, "Failed to get element.");
            return;
        }

        if (!TypeCheck(env, tag, napi_string)) {
            HILOG_ERROR(LOG_CORE, "tag is invalid, not a napi_string");
            return;
        }

        std::string tagStr = "";
        GetStringParam(env, tag, tagStr);
        if (g_tagsMap.count(tagStr) > 0) {
            tags |= g_tagsMap[tagStr];
        }
    }
}

bool ParseTagsParam(const napi_env& env, const napi_value& value, uint64_t& tags)
{
    bool isArray = false;
    napi_status status = napi_is_array(env, value, &isArray);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to get array type.");
        return false;
    }

    if (isArray) {
        SetTagsParam(env, value, tags);
        return true;
    } else {
        HILOG_ERROR(LOG_CORE, "The argument isn't an array type.");
        return false;
    }
}

bool JsStrNumParamsFunc(napi_env& env, napi_callback_info& info, STR_NUM_PARAM_FUNC nativeCall)
{
    size_t argc = ARGC_NUMBER_TWO;
    napi_value argv[ARGC_NUMBER_TWO];
    ParseParams(env, info, argc, argv);
    if (argc != ARGC_NUMBER_TWO) {
        HILOG_ERROR(LOG_CORE, "Wrong number of parameters.");
        return false;
    }
    std::string name;
    if (!ParseStringParam(env, argv[FIRST_ARG_INDEX], name)) {
        return false;
    }
    if (!nativeCall(name, argv[SECOND_ARG_INDEX])) {
        return false;
    }
    return true;
}
}

static napi_value JSTraceStart(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_NUMBER_THREE;
    napi_value argv[ARGC_NUMBER_THREE];
    ParseParams(env, info, argc, argv);
    NAPI_ASSERT(env, argc >= ARGC_NUMBER_TWO, "Wrong number of arguments");
    if (argc < ARGC_NUMBER_TWO) {
        HILOG_ERROR(LOG_CORE, "Wrong number of parameters.");
    }
    std::string name;
    if (!ParseStringParam(env, argv[FIRST_ARG_INDEX], name)) {
        return nullptr;
    }
    if (name == "null" || name == "undefined") {
        return nullptr;
    }
    int taskId = 0;
    if (!ParseInt32Param(env, argv[SECOND_ARG_INDEX], taskId)) {
        return nullptr;
    }
    StartAsyncTrace(HITRACE_TAG_APP, name, taskId);
    return nullptr;
}

static napi_value JSTraceFinish(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_NUMBER_TWO;
    napi_value argv[ARGC_NUMBER_TWO];
    napi_value thisVar;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL));
    NAPI_ASSERT(env, argc == ARGC_NUMBER_TWO, "Wrong number of arguments");
    (void)JsStrNumParamsFunc(env, info, [&env] (std::string name, napi_value& nValue) -> bool {
        if (name == "null" || name == "undefined") {
            return false;
        }
        int taskId = 0;
        if (!ParseInt32Param(env, nValue, taskId)) {
            return false;
        }
        FinishAsyncTrace(HITRACE_TAG_APP, name, taskId);
        return true;
    });
    return nullptr;
}

static napi_value JSTraceCount(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_NUMBER_TWO;
    napi_value argv[ARGC_NUMBER_TWO];
    napi_value thisVar;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL));
    NAPI_ASSERT(env, argc == ARGC_NUMBER_TWO, "Wrong number of arguments");
    (void)JsStrNumParamsFunc(env, info, [&env] (std::string name, napi_value& nValue) -> bool {
        if (name == "null" || name == "undefined") {
            return false;
        }
        int64_t count = 0;
        if (!ParseInt64Param(env, nValue, count)) {
            return false;
        }
        CountTrace(HITRACE_TAG_APP, name, count);
        return true;
    });
    return nullptr;
}

static napi_value JSStartCaptureAppTrace(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_NUMBER_THREE;
    napi_value argv[ARGC_NUMBER_THREE];
    ParseParams(env, info, argc, argv);
    NAPI_ASSERT(env, argc >= ARGC_NUMBER_THREE, "Wrong number of arguments");

    uint64_t tags = HITRACE_TAG_APP;
    if (!ParseTagsParam(env, argv[FIRST_ARG_INDEX], tags)) {
        return nullptr;
    }

    int64_t flag = 0;
    if (!ParseInt64Param(env, argv[SECOND_ARG_INDEX], flag)) {
        return nullptr;
    }

    int64_t limitSize = 0;
    if (!ParseInt64Param(env, argv[ARGC_NUMBER_TWO], limitSize)) {
        return nullptr;
    }

    std::string file = "";
    if (StartCaptureAppTrace((TraceFlag)flag, tags, limitSize, file) != RET_SUCC) {
        HILOG_ERROR(LOG_CORE, "StartCaptureAppTrace failed");
        return nullptr;
    }

    napi_value napiFile;
    napi_status status = napi_create_string_utf8(env, file.c_str(), file.length(), &napiFile);
    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "create napi string failed: %{public}d(%{public}s)", errno, strerror(errno));
        return nullptr;
    }

    return napiFile;
}

static napi_value JSStopCaptureAppTrace(napi_env env, napi_callback_info info)
{
    StopCaptureAppTrace();
    return nullptr;
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
        DECLARE_NAPI_FUNCTION("startCaptureAppTrace", JSStartCaptureAppTrace),
        DECLARE_NAPI_FUNCTION("stopCaptureAppTrace", JSStopCaptureAppTrace),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
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
