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

#include <array>
#include <iostream>
#include <ani.h>
#include <hilog/log.h>
#include <string>
#include "hitrace_meter.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceMeterAni"
#endif

using namespace OHOS::HiviewDFX;
static const char NAMESPACE_HITRACEMETER[] = "@ohos.hiTraceMeter.hiTraceMeter";
constexpr size_t MIN_SIZE = 1;
constexpr size_t MAX_SIZE = 1024;

static bool AniStringToStdString(ani_env* env, ani_string aniStr, std::string& content)
{
    ani_size strSize = 0;
    env->String_GetUTF8Size(aniStr, &strSize);

    if (strSize < MIN_SIZE || strSize > MAX_SIZE) {
        return false;
    }
    std::vector<char> buffer(strSize + 1);
    char* charBuffer = buffer.data();

    ani_size bytesWritten = 0;
    env->String_GetUTF8(aniStr, charBuffer, strSize + 1, &bytesWritten);

    charBuffer[bytesWritten] = '\0';
    content = std::string(charBuffer);
    return true;
}

static bool AniEnumToInt32(ani_env* env, ani_enum_item enumItem, int32_t& value)
{
    ani_int aniInt = 0;
    if (env->EnumItem_GetValue_Int(enumItem, &aniInt) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Failed to get the int32 value of enum.");
        return false;
    }
    value = static_cast<int32_t>(aniInt);
    return true;
}

static bool IsRefUndefined(ani_env* env, ani_ref value)
{
    ani_boolean isUndefined = ANI_FALSE;
    env->Reference_IsUndefined(value, &isUndefined);
    return isUndefined;
}

static void EtsStartTrace(ani_env* env, ani_string name, ani_double taskId)
{
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, nameStr.c_str(), static_cast<int32_t>(taskId), "", "");
}

static void EtsFinishTrace(ani_env* env, ani_string name, ani_double taskId)
{
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, nameStr.c_str(), static_cast<int32_t>(taskId));
}

static void EtsCountTrace(ani_env* env, ani_string name, ani_double count)
{
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, nameStr.c_str(), static_cast<int64_t>(count));
}

static void EtsTraceByValue(ani_env* env, ani_enum_item level, ani_string name, ani_double count)
{
    int32_t levelVal = 0;
    if (!AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    CountTraceEx(static_cast<HiTraceOutputLevel>(levelVal), HITRACE_TAG_APP, nameStr.c_str(),
        static_cast<int64_t>(count));
}

static void EtsStartSyncTrace(ani_env* env, ani_enum_item level, ani_string name, ani_object customArgs)
{
    int32_t levelVal = 0;
    if (!AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    std::string customArgsStr = "";
    if (!IsRefUndefined(env, static_cast<ani_ref>(customArgs))) {
        if (!AniStringToStdString(env, static_cast<ani_string>(customArgs), customArgsStr)) {
            return;
        }
    }
    StartTraceEx(static_cast<HiTraceOutputLevel>(levelVal), HITRACE_TAG_APP, nameStr.c_str(), customArgsStr.c_str());
}

static void EtsFinishSyncTrace(ani_env* env, ani_enum_item level)
{
    int32_t levelVal = 0;
    if (!AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    FinishTraceEx(static_cast<HiTraceOutputLevel>(levelVal), HITRACE_TAG_APP);
}

static void EtsStartAsyncTrace(ani_env* env, ani_enum_item level, ani_string name, ani_double taskId,
    ani_string customCategory, ani_object customArgs)
{
    int32_t levelVal = 0;
    if (!AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    std::string customCategoryStr = "";
    if (!AniStringToStdString(env, customCategory, customCategoryStr)) {
        return;
    }
    std::string customArgsStr = "";
    if (!IsRefUndefined(env, static_cast<ani_ref>(customArgs))) {
        if (!AniStringToStdString(env, static_cast<ani_string>(customArgs), customArgsStr)) {
            return;
        }
    }
    StartAsyncTraceEx(static_cast<HiTraceOutputLevel>(levelVal), HITRACE_TAG_APP, nameStr.c_str(),
        static_cast<int32_t>(taskId), customCategoryStr.c_str(), customArgsStr.c_str());
}

static void EtsFinishAsyncTrace(ani_env* env, ani_enum_item level, ani_string name, ani_double taskId)
{
    int32_t levelVal = 0;
    if (!AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    std::string nameStr = "";
    if (!AniStringToStdString(env, name, nameStr)) {
        return;
    }
    FinishAsyncTraceEx(static_cast<HiTraceOutputLevel>(levelVal), HITRACE_TAG_APP, nameStr.c_str(),
        static_cast<int32_t>(taskId));
}

static ani_boolean EtsIsTraceEnabled(ani_env* env)
{
    return static_cast<ani_boolean>(IsTagEnabled(HITRACE_TAG_APP));
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm* vm, uint32_t* result)
{
    ani_env* env;
    if (vm->GetEnv(ANI_VERSION_1, &env) != ANI_OK) {
        return ANI_ERROR;
    }

    ani_namespace ns;
    if (env->FindNamespace(NAMESPACE_HITRACEMETER, &ns) != ANI_OK) {
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"startTrace", nullptr, reinterpret_cast<void *>(EtsStartTrace)},
        ani_native_function {"finishTrace", nullptr, reinterpret_cast<void *>(EtsFinishTrace)},
        ani_native_function {"traceByValue", "C{std.core.String}d:", reinterpret_cast<void *>(EtsCountTrace)},
        ani_native_function {"traceByValue",
            "C{@ohos.hiTraceMeter.hiTraceMeter.HiTraceOutputLevel}C{std.core.String}d:",
            reinterpret_cast<void *>(EtsTraceByValue)},
        ani_native_function {"startSyncTrace", nullptr, reinterpret_cast<void *>(EtsStartSyncTrace)},
        ani_native_function {"finishSyncTrace", nullptr, reinterpret_cast<void *>(EtsFinishSyncTrace)},
        ani_native_function {"startAsyncTrace", nullptr, reinterpret_cast<void *>(EtsStartAsyncTrace)},
        ani_native_function {"finishAsyncTrace", nullptr, reinterpret_cast<void *>(EtsFinishAsyncTrace)},
        ani_native_function {"isTraceEnabled", nullptr, reinterpret_cast<void *>(EtsIsTraceEnabled)},
    };

    if (env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size()) != ANI_OK) {
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
