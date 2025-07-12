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

bool HiTraceChainAniUtil::IsRefUndefined(ani_env* env, ani_ref ref)
{
    ani_boolean isUndefined = false;
    env->Reference_IsUndefined(ref, &isUndefined);
    return isUndefined;
}

ani_status HiTraceChainAniUtil::GetAniStringValue(ani_env* env, ani_ref aniStrRef, std::string& name)
{
    ani_size strSize = 0;
    if (env->String_GetUTF8Size(static_cast<ani_string>(aniStrRef), &strSize) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8Size failed");
        return ANI_ERROR;
    }
    std::vector<char> buffer(strSize + 1);
    char* utf8Buffer = buffer.data();
    ani_size bytesWritten = 0;
    if (env->String_GetUTF8(static_cast<ani_string>(aniStrRef), utf8Buffer, strSize + 1, &bytesWritten) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8 failed");
        return ANI_ERROR;
    }
    utf8Buffer[bytesWritten] = '\0';
    name = std::string(utf8Buffer);
    return ANI_OK;
}

ani_long HiTraceChainAniUtil::GetAniBigIntValue(ani_env* env, ani_ref elementRef)
{
    ani_long aniLong = 0;
    ani_class cls {};
    if (env->FindClass(CLASS_NAME_BIGINT, &cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_BIGINT);
        return aniLong;
    }
    ani_method getLongMethod {};
    if (env->Class_FindMethod(cls, FUNC_NAME_GETLONG, ":l", &getLongMethod) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", FUNC_NAME_GETLONG);
        return aniLong;
    }
    if (env->Object_CallMethod_Long(static_cast<ani_object>(elementRef), getLongMethod, &aniLong) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", FUNC_NAME_GETLONG);
    }
    return aniLong;
}

ani_int HiTraceChainAniUtil::GetAniIntValue(ani_env* env, ani_ref elementRef)
{
    ani_int aniInt = 0;
    ani_object intObj = static_cast<ani_object>(elementRef);
    if (env->Object_CallMethodByName_Int(intObj, FUNC_NAME_UNBOXED, ":i", &aniInt) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", FUNC_NAME_UNBOXED);
    }
    return aniInt;
}

ani_status HiTraceChainAniUtil::AniEnumToInt(ani_env* env, ani_enum_item enumItem, int& value)
{
    ani_int aniInt {};
    ani_status result = env->EnumItem_GetValue_Int(enumItem, &aniInt);
    if (result != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get Int32 %{public}d failed", result);
        return result;
    }
    value = static_cast<int>(aniInt);
    return result;
}

ani_object HiTraceChainAniUtil::CreateAniBigInt(ani_env* env, uint64_t value)
{
    ani_object bigIntObj = nullptr;
    ani_class bigIntCls;
    if (env->FindClass(CLASS_NAME_BIGINT, &bigIntCls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find Class %{public}s failed", CLASS_NAME_BIGINT);
        return bigIntObj;
    }
    ani_method createLongMethod;
    if (env->Class_FindMethod(bigIntCls, "<ctor>", "l:", &createLongMethod) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s constructor failed", CLASS_NAME_BIGINT);
        return bigIntObj;
    }
    ani_long longNum = static_cast<ani_long>(value);
    if (env->Object_New(bigIntCls, createLongMethod, &bigIntObj, longNum) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_BIGINT);
    }
    return bigIntObj;
}

ani_object HiTraceChainAniUtil::CreateAniInt(ani_env* env, uint64_t value)
{
    ani_object intObj = nullptr;
    ani_class intCls;
    if (env->FindClass(CLASS_NAME_INT, &intCls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_INT);
        return intObj;
    }
    ani_method createIntMethod;
    if (env->Class_FindMethod(intCls, "<ctor>", "i:", &createIntMethod) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s constructor failed", CLASS_NAME_INT);
        return intObj;
    }
    ani_int intNum = static_cast<ani_int>(value);
    if (env->Object_New(intCls, createIntMethod, &intObj, intNum) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_INT);
    }
    return intObj;
}
