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
#include "hitrace_meter.h"
#include "hilog/log.h"
#include "hilog/log_cpp.h"

using namespace OHOS::HiviewDFX;
const std::string CLASS_NAME_HITRACEMETER = "L@ohos/hitracemeter/HiTraceMeter;";
constexpr size_t MIN_SIZE = 1;
constexpr size_t MAX_SIZE = 1024;

static bool ANIStringToStdString(ani_env *env, ani_string aniStr, std::string &content)
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

static void StartTraceMeter([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object,
    ani_string name, ani_double taskId)
{
    std::string nameStr;
    if (!ANIStringToStdString(env, name, nameStr)) {
        return;
    }
    if (nameStr == "null" || nameStr == "undefined") {
        return;
    }

    int32_t taskIdTemp = static_cast<int32_t>(taskId);
    StartAsyncTrace(HITRACE_TAG_APP, nameStr, taskIdTemp);
}

static void FinishTraceMeter([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object,
    ani_string name, ani_double taskId)
{
    std::string nameStr;
    if (!ANIStringToStdString(env, name, nameStr)) {
        return;
    }
    if (nameStr == "null" || nameStr == "undefined") {
        return;
    }

    int32_t taskIdTemp = static_cast<int32_t>(taskId);
    FinishAsyncTrace(HITRACE_TAG_APP, nameStr, taskIdTemp);
}

static ani_status HiTraceMeterAniInit(ani_env *env)
{
    ani_class cls;
    if (ANI_OK != env->FindClass(CLASS_NAME_HITRACEMETER.c_str(), &cls)) {
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"startTrace", nullptr, reinterpret_cast<void *>(StartTraceMeter) },
        ani_native_function {"finishTrace", nullptr, reinterpret_cast<void *>(FinishTraceMeter) },
    };

    if (ANI_OK != env->Class_BindNativeMethods(cls, methods.data(), methods.size())) {
        return ANI_ERROR;
    };

    return ANI_OK;
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        return ANI_ERROR;
    }

    auto status = HiTraceMeterAniInit(env);
    if (status != ANI_OK) {
        return status;
    }
    *result = ANI_VERSION_1;
    return ANI_OK;
}
