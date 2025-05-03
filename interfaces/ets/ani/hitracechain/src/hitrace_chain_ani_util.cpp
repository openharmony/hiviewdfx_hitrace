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

bool HiTraceChainAniUtil::IsRefUndefined(ani_env *env, ani_ref ref)
{
    ani_boolean isUndefined = false;
    env->Reference_IsUndefined(ref, &isUndefined);
    return isUndefined;
}

ani_status HiTraceChainAniUtil::ParseStringValue(ani_env *env, ani_ref aniStrRef, std::string &name)
{
    ani_size strSize = 0;
    if (ANI_OK != env->String_GetUTF8Size(static_cast<ani_string>(aniStrRef), &strSize)) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8Size failed");
        return ANI_ERROR;
    }
    std::vector<char> buffer(strSize + 1);
    char* utf8Buffer = buffer.data();
    ani_size bytesWritten = 0;
    if (ANI_OK != env->String_GetUTF8(static_cast<ani_string>(aniStrRef), utf8Buffer, strSize + 1, &bytesWritten)) {
        HILOG_ERROR(LOG_CORE, "String_GetUTF8 failed");
        return ANI_ERROR;
    }
    utf8Buffer[bytesWritten] = '\0';
    name = std::string(utf8Buffer);
    return ANI_OK;
}

int64_t HiTraceChainAniUtil::ParseBigintValue(ani_env *env, ani_ref elementRef)
{
    ani_long longNum = static_cast<ani_long>(0);
    ani_class cls {};
    if (ANI_OK != env->FindClass(CLASS_NAME_BIGINT.c_str(), &cls)) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_BIGINT.c_str());
        return static_cast<int64_t>(longNum);
    }
    ani_method getLongMethod {};
    if (ANI_OK != env->Class_FindMethod(cls, FUNC_NAME_GETLONG.c_str(), ":J", &getLongMethod)) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", FUNC_NAME_GETLONG.c_str());
        return static_cast<int64_t>(longNum);
    }
    if (ANI_OK != env->Object_CallMethod_Long(static_cast<ani_object>(elementRef), getLongMethod, &longNum)) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", FUNC_NAME_GETLONG.c_str());
        return static_cast<int64_t>(longNum);
    }
    return static_cast<int64_t>(longNum);
}

int64_t HiTraceChainAniUtil::ParseNumberValueInt64(ani_env *env, ani_ref elementRef)
{
    ani_double doubleVal = 0;
    ani_class cls {};
    if (ANI_OK != env->FindClass(CLASS_NAME_DOUBLE.c_str(), &cls)) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_DOUBLE.c_str());
        return static_cast<int64_t>(doubleVal);
    }
    ani_method unboxedMethod {};
    if (ANI_OK != env->Class_FindMethod(cls, FUNC_NAME_UNBOXED.c_str(), ":D", &unboxedMethod)) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", FUNC_NAME_UNBOXED.c_str());
        return static_cast<int64_t>(doubleVal);
    }
    if (ANI_OK != env->Object_CallMethod_Double(static_cast<ani_object>(elementRef), unboxedMethod, &doubleVal)) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", FUNC_NAME_UNBOXED.c_str());
        return static_cast<int64_t>(doubleVal);
    }
    return static_cast<int64_t>(doubleVal);
}

int32_t HiTraceChainAniUtil::ParseNumberValueInt32(ani_env *env, ani_ref elementRef)
{
    ani_double doubleVal = 0;
    ani_class cls {};
    if (ANI_OK != env->FindClass(CLASS_NAME_DOUBLE.c_str(), &cls)) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_DOUBLE.c_str());
        return static_cast<int32_t>(doubleVal);
    }
    ani_method unboxedMethod {};
    if (ANI_OK != env->Class_FindMethod(cls, FUNC_NAME_UNBOXED.c_str(), ":D", &unboxedMethod)) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", FUNC_NAME_UNBOXED.c_str());
        return static_cast<int32_t>(doubleVal);
    }
    if (ANI_OK != env->Object_CallMethod_Double(static_cast<ani_object>(elementRef), unboxedMethod, &doubleVal)) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", FUNC_NAME_UNBOXED.c_str());
        return static_cast<int32_t>(doubleVal);
    }
    return static_cast<int32_t>(doubleVal);
}

ani_status HiTraceChainAniUtil::EnumGetValueInt32(ani_env *env, ani_enum_item enumItem, int32_t &value)
{
    ani_int aniInt {};
    ani_status result = env->EnumItem_GetValue_Int(enumItem, &aniInt);
    if (ANI_OK != result) {
        HILOG_ERROR(LOG_CORE, "get Int32 %{public}d failed", result);
        return result;
    }
    value = static_cast<int32_t>(aniInt);
    return result;
}

ani_object HiTraceChainAniUtil::CreateBigInt(ani_env *env, uint64_t traceId)
{
    ani_object bigintCtor = nullptr;
    ani_class bigIntCls;
    if (ANI_OK != env->FindClass(CLASS_NAME_BIGINT.c_str(), &bigIntCls)) {
        HILOG_ERROR(LOG_CORE, "find Class %{public}s failed", CLASS_NAME_HITRACEID.c_str());
        return bigintCtor;
    }
    ani_method createLongMethod;
    if (ANI_OK != env->Class_FindMethod(bigIntCls, "<ctor>", "J:V", &createLongMethod)) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID.c_str());
        return bigintCtor;
    }
    ani_long longNum = static_cast<ani_long>(traceId);
    if (ANI_OK != env->Object_New(bigIntCls, createLongMethod, &bigintCtor, longNum)) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_HITRACEID.c_str());
        return bigintCtor;
    }
    return bigintCtor;
}

ani_object HiTraceChainAniUtil::createDoubleUint64(ani_env *env, uint64_t number)
{
    ani_object personInfoObj = nullptr;
    ani_class personCls;
    if (ANI_OK != env->FindClass(CLASS_NAME_DOUBLE.c_str(), &personCls)) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_DOUBLE.c_str());
        return personInfoObj;
    }
    ani_method personInfoCtor;
    env->Class_FindMethod(personCls, "<ctor>", "D:V", &personInfoCtor);
    env->Object_New(personCls, personInfoCtor, &personInfoObj, static_cast<ani_double>(number));
    return personInfoObj;
}

ani_object HiTraceChainAniUtil::createDoubleInt(ani_env *env, int number)
{
    ani_object personInfoObj = nullptr;
    ani_class personCls;
    if (ANI_OK != env->FindClass(CLASS_NAME_DOUBLE.c_str(), &personCls)) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_DOUBLE.c_str());
        return personInfoObj;
    }
    ani_method personInfoCtor;
    env->Class_FindMethod(personCls, "<ctor>", "D:V", &personInfoCtor);
    env->Object_New(personCls, personInfoCtor, &personInfoObj, static_cast<ani_double>(number));
    return personInfoObj;
}
