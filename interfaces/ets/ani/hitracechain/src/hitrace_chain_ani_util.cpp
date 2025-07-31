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
#include "hitraceid.h"
#include "hitrace_chain_ani_util.h"
#include "hitrace_chain_ani_parameter_name.h"

using namespace OHOS::HiviewDFX;
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "HitraceUtilAni"

bool HiTraceChainAniUtil::GetAniStringValue(ani_env* env, ani_string strAni, std::string& content)
{
    ani_size strSize = 0;
    if (env->String_GetUTF8Size(strAni, &strSize) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8Size failed");
        return false;
    }
    std::vector<char> buffer(strSize + 1);
    char* utf8Buffer = buffer.data();
    ani_size bytesWritten = 0;
    if (env->String_GetUTF8(strAni, utf8Buffer, strSize + 1, &bytesWritten) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8 failed");
        return false;
    }
    utf8Buffer[bytesWritten] = '\0';
    content = std::string(utf8Buffer);
    return true;
}

bool HiTraceChainAniUtil::GetAniBigIntValue(ani_env* env, ani_object bigIntObj, ani_long& value)
{
    ani_class cls {};
    if (env->FindClass(CLASS_NAME_BIGINT, &cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_BIGINT);
        return false;
    }
    ani_method getLongMethod {};
    if (env->Class_FindMethod(cls, FUNC_NAME_GETLONG, ":l", &getLongMethod) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", FUNC_NAME_GETLONG);
        return false;
    }
    if (env->Object_CallMethod_Long(bigIntObj, getLongMethod, &value) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", FUNC_NAME_GETLONG);
        return false;
    }
    return true;
}

bool HiTraceChainAniUtil::CreateAniBigInt(ani_env* env, uint64_t value, ani_object& bigIntObj)
{
    ani_class bigIntCls {};
    if (env->FindClass(CLASS_NAME_BIGINT, &bigIntCls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find Class %{public}s failed", CLASS_NAME_BIGINT);
        return false;
    }
    ani_method createLongMethod {};
    if (env->Class_FindMethod(bigIntCls, FUNC_NAME_CTOR, "l:", &createLongMethod) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s constructor failed", CLASS_NAME_BIGINT);
        return false;
    }
    ani_long longNum = static_cast<ani_long>(value);
    if (env->Object_New(bigIntCls, createLongMethod, &bigIntObj, longNum) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_BIGINT);
        return false;
    }
    return true;
}

bool HiTraceChainAniUtil::CreateAniInt(ani_env* env, uint64_t value, ani_object& intObj)
{
    ani_class intCls {};
    if (env->FindClass(CLASS_NAME_INT, &intCls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_INT);
        return false;
    }
    ani_method createIntMethod {};
    if (env->Class_FindMethod(intCls, FUNC_NAME_CTOR, "i:", &createIntMethod) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s constructor failed", CLASS_NAME_INT);
        return false;
    }
    ani_int intNum = static_cast<ani_int>(value);
    if (env->Object_New(intCls, createIntMethod, &intObj, intNum) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_INT);
        return false;
    }
    return true;
}
