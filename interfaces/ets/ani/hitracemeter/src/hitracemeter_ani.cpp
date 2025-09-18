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
const char* const NAMESPACE_HITRACEMETER = "@ohos.hiTraceMeter.hiTraceMeter";

static bool GetAniStringValue(ani_env* env, ani_string strAni, std::string& content)
{
    ani_size strSize = 0;
    if (env->String_GetUTF8Size(strAni, &strSize) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8Size failed");
        return false;
    }
    std::vector<char> buffer(strSize + 1);
    char* charBuffer = buffer.data();
    ani_size bytesWritten = 0;
    if (env->String_GetUTF8(strAni, charBuffer, strSize + 1, &bytesWritten) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8 failed");
        return false;
    }
    charBuffer[bytesWritten] = '\0';
    content = std::string(charBuffer);
    return true;
}

static void EtsStartTrace(ani_env* env, ani_string nameAni, ani_int taskIdAni)
{
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsStartTrace name parsing failed");
        return;
    }
    StartAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name.c_str(), taskIdAni, "", "");
}

static void EtsFinishTrace(ani_env* env, ani_string nameAni, ani_int taskIdAni)
{
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsFinishTrace name parsing failed");
        return;
    }
    FinishAsyncTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name.c_str(), taskIdAni);
}

static void EtsTraceByValueV8(ani_env* env, ani_string nameAni, ani_long countAni)
{
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsTraceByValueV8 name parsing failed");
        return;
    }
    CountTraceEx(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name.c_str(), countAni);
}

static void EtsTraceByValueV19(ani_env* env, ani_enum_item levelAni, ani_string nameAni, ani_long countAni)
{
    ani_int level = 0;
    if (env->EnumItem_GetValue_Int(levelAni, &level) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsTraceByValueV19 level parsing failed");
        return;
    }
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsTraceByValueV19 name parsing failed");
        return;
    }
    CountTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(), countAni);
}

static void EtsStartSyncTrace(ani_env* env, ani_enum_item levelAni, ani_string nameAni, ani_object customArgsAni)
{
    ani_int level = 0;
    if (env->EnumItem_GetValue_Int(levelAni, &level) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsStartSyncTrace level parsing failed");
        return;
    }
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsStartSyncTrace name parsing failed");
        return;
    }
    ani_boolean isUndefined = true;
    if (env->Reference_IsUndefined(static_cast<ani_ref>(customArgsAni), &isUndefined) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsStartSyncTrace get ref undefined failed");
        return;
    }
    std::string customArgs = "";
    if (!isUndefined && !GetAniStringValue(env, static_cast<ani_string>(customArgsAni), customArgs)) {
        HILOG_ERROR(LOG_CORE, "EtsStartSyncTrace customArgs parsing failed");
        return;
    }
    StartTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(), customArgs.c_str());
}

static void EtsFinishSyncTrace(ani_env* env, ani_enum_item levelAni)
{
    ani_int level = 0;
    if (env->EnumItem_GetValue_Int(levelAni, &level) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsFinishSyncTrace level parsing failed");
        return;
    }
    FinishTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP);
}

static void EtsStartAsyncTrace(ani_env* env, ani_enum_item levelAni, ani_string nameAni, ani_int taskIdAni,
    ani_string customCategoryAni, ani_object customArgsAni)
{
    ani_int level = 0;
    if (env->EnumItem_GetValue_Int(levelAni, &level) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsStartAsyncTrace level parsing failed");
        return;
    }
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsStartAsyncTrace name parsing failed");
        return;
    }
    std::string customCategory = "";
    if (!GetAniStringValue(env, customCategoryAni, customCategory)) {
        HILOG_ERROR(LOG_CORE, "EtsStartAsyncTrace customCategory parsing failed");
        return;
    }
    ani_boolean isUndefined = true;
    if (env->Reference_IsUndefined(static_cast<ani_ref>(customArgsAni), &isUndefined) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsStartAsyncTrace get ref undefined failed");
        return;
    }
    std::string customArgs = "";
    if (!isUndefined) {
        if (!GetAniStringValue(env, static_cast<ani_string>(customArgsAni), customArgs)) {
            HILOG_ERROR(LOG_CORE, "EtsStartAsyncTrace customArgs parsing failed");
            return;
        }
    }
    StartAsyncTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(),
        taskIdAni, customCategory.c_str(), customArgs.c_str());
}

static void EtsFinishAsyncTrace(ani_env* env, ani_enum_item levelAni, ani_string nameAni, ani_int taskIdAni)
{
    ani_int level = 0;
    if (env->EnumItem_GetValue_Int(levelAni, &level) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EtsFinishAsyncTrace level parsing failed");
        return;
    }
    std::string name = "";
    if (!GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "EtsFinishAsyncTrace name parsing failed");
        return;
    }
    FinishAsyncTraceEx(static_cast<HiTraceOutputLevel>(level), HITRACE_TAG_APP, name.c_str(), taskIdAni);
}

static ani_boolean EtsIsTraceEnabled(ani_env* env)
{
    return static_cast<ani_boolean>(IsTagEnabled(HITRACE_TAG_APP));
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm* vm, uint32_t* result)
{
    ani_env* env;
    if (vm->GetEnv(ANI_VERSION_1, &env) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "GetEnv failed");
        return ANI_ERROR;
    }

    ani_namespace ns;
    if (env->FindNamespace(NAMESPACE_HITRACEMETER, &ns) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "FindNamespace %{public}s failed", NAMESPACE_HITRACEMETER);
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"startTrace", nullptr, reinterpret_cast<void *>(EtsStartTrace)},
        ani_native_function {"finishTrace", nullptr, reinterpret_cast<void *>(EtsFinishTrace)},
        ani_native_function {"traceByValue", "C{std.core.String}l:", reinterpret_cast<void *>(EtsTraceByValueV8)},
        ani_native_function {"traceByValue",
            "C{@ohos.hiTraceMeter.hiTraceMeter.HiTraceOutputLevel}C{std.core.String}l:",
            reinterpret_cast<void *>(EtsTraceByValueV19)},
        ani_native_function {"startSyncTrace", nullptr, reinterpret_cast<void *>(EtsStartSyncTrace)},
        ani_native_function {"finishSyncTrace", nullptr, reinterpret_cast<void *>(EtsFinishSyncTrace)},
        ani_native_function {"startAsyncTrace", nullptr, reinterpret_cast<void *>(EtsStartAsyncTrace)},
        ani_native_function {"finishAsyncTrace", nullptr, reinterpret_cast<void *>(EtsFinishAsyncTrace)},
        ani_native_function {"isTraceEnabled", nullptr, reinterpret_cast<void *>(EtsIsTraceEnabled)},
    };

    if (env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size()) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "%{public}s bind native function failed", NAMESPACE_HITRACEMETER);
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
