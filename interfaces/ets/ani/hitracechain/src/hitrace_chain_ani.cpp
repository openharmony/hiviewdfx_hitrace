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
#include "hilog/log.h"
#include "hitraceid.h"
#include "hitracechain.h"
#include "hitrace_chain_ani.h"
#include "hitrace_chain_ani_util.h"
#include "hitrace_chain_ani_parameter_name.h"

using namespace OHOS::HiviewDFX;
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "HitraceChainAni"

constexpr uint64_t DEFAULT_CHAIN_ID = 0;
constexpr uint64_t DEFAULT_SPAN_ID = 0;
constexpr uint64_t DEFAULT_PARENT_SPAN_ID = 0;
constexpr int DEFAULT_FLAGS = 0;

ani_object HiTraceChainAni::Begin(ani_env* env, ani_string nameAni, ani_object flagAni)
{
    std::string name;
    ani_object val {};
    if (!HiTraceChainAniUtil::GetAniStringValue(env, nameAni, name)) {
        HILOG_ERROR(LOG_CORE, "Begin name parsing failed");
        return val;
    }
    int flag = HiTraceFlag::HITRACE_FLAG_DEFAULT;
    ani_boolean isUndefined = true;
    if (env->Reference_IsUndefined(static_cast<ani_ref>(flagAni), &isUndefined) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Begin get ref undefined failed");
        return val;
    }
    if (!isUndefined &&
        env->Object_CallMethodByName_Int(flagAni, "toInt", ":i", &flag) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Begin flag parsing failed");
        return val;
    }
    HiTraceId traceId = HiTraceChain::Begin(name, flag);
    val = HiTraceChainAni::CreateHitraceIdAni(env, traceId);
    return val;
};


static ani_status SetChainId(ani_env* env, ani_class cls, HiTraceId& traceId, ani_object& traceIdAni)
{
    uint64_t chainId = traceId.GetChainId();
    ani_object chainIdObj {};
    if (!HiTraceChainAniUtil::CreateAniBigInt(env, chainId, chainIdObj)) {
        HILOG_ERROR(LOG_CORE, "SetChainId CreateAniBigInt failed");
        return ANI_ERROR;
    }
    if (env->Object_SetPropertyByName_Ref(traceIdAni, "chainId", static_cast<ani_ref>(chainIdObj)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set chainId failed");
        return ANI_ERROR;
    }
    return ANI_OK;
}

static ani_status SetSpanId(ani_env* env, ani_class cls, HiTraceId& traceId, ani_object& traceIdAni)
{
    uint64_t spanId = traceId.GetSpanId();
    ani_object spanIdObj {};
    if (!HiTraceChainAniUtil::CreateAniInt(env, spanId, spanIdObj)) {
        HILOG_ERROR(LOG_CORE, "SetSpanId CreateAniInt failed");
        return ANI_ERROR;
    }
    if (env->Object_SetPropertyByName_Ref(traceIdAni, "spanId", static_cast<ani_ref>(spanIdObj)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set spanId failed");
        return ANI_ERROR;
    }
    return ANI_OK;
}

static ani_status SetParentSpanId(ani_env* env, ani_class cls, HiTraceId& traceId, ani_object& traceIdAni)
{
    uint64_t parentSpanId = traceId.GetParentSpanId();
    ani_object parentSpanIdObj {};
    if (!HiTraceChainAniUtil::CreateAniInt(env, parentSpanId, parentSpanIdObj)) {
        HILOG_ERROR(LOG_CORE, "SetParentSpanId CreateAniInt failed");
        return ANI_ERROR;
    }
    if (env->Object_SetPropertyByName_Ref(traceIdAni, "parentSpanId",
        static_cast<ani_ref>(parentSpanIdObj)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set parentSpanId failed");
        return ANI_ERROR;
    }
    return ANI_OK;
}

static ani_status SetFlags(ani_env* env, ani_class cls, HiTraceId& traceId, ani_object& traceIdAni)
{
    int flags = traceId.GetFlags();
    ani_object flagsObj {};
    if (!HiTraceChainAniUtil::CreateAniInt(env, static_cast<uint64_t>(flags), flagsObj)) {
        HILOG_ERROR(LOG_CORE, "SetFlags CreateAniInt failed");
        return ANI_ERROR;
    }
    if (env->Object_SetPropertyByName_Ref(traceIdAni, "flags", static_cast<ani_ref>(flagsObj)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set flags failed");
        return ANI_ERROR;
    }
    return ANI_OK;
}

ani_object HiTraceChainAni::CreateHitraceIdAni(ani_env* env, HiTraceId& traceId)
{
    ani_object traceIdAni {};
    ani_class cls {};
    if (env->FindClass(CLASS_NAME_HITRACEID, &cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_HITRACEID);
        return traceIdAni;
    }
    ani_method ctor {};
    if (env->Class_FindMethod(cls, FUNC_NAME_CTOR, nullptr, &ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID);
        return traceIdAni;
    }
    if (env->Object_New(cls, ctor, &traceIdAni) != ANI_OK ||
        SetChainId(env, cls, traceId, traceIdAni) != ANI_OK ||
        SetSpanId(env, cls, traceId, traceIdAni) != ANI_OK ||
        SetParentSpanId(env, cls, traceId, traceIdAni) != ANI_OK ||
        SetFlags(env, cls, traceId, traceIdAni) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_HITRACEID);
    }
    return traceIdAni;
}

static bool GetIntProp(ani_env* env, ani_object traceIdAni, const char* prop, ani_int& value)
{
    ani_ref propRef {};
    if (env->Object_GetPropertyByName_Ref(traceIdAni, prop, &propRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get %{public}s ref failed", prop);
        return false;
    }
    ani_boolean isUndefined = true;
    if (env->Reference_IsUndefined(propRef, &isUndefined) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "%{public}s get ref undefined failed", prop);
        return false;
    }
    if (!isUndefined &&
        env->Object_CallMethodByName_Int(static_cast<ani_object>(propRef), "toInt", ":i", &value) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "%{public}s parsing failed", prop);
        return false;
    }
    return true;
}

static bool ParseHiTraceId(ani_env* env, ani_object traceIdAni, HiTraceId& traceId)
{
    ani_ref chainIdRef {};
    if (env->Object_GetPropertyByName_Ref(traceIdAni, "chainId", &chainIdRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get chainId ref failed");
        return false;
    }
    ani_long chainId = DEFAULT_CHAIN_ID;
    if (!HiTraceChainAniUtil::GetAniBigIntValue(env, static_cast<ani_object>(chainIdRef), chainId)) {
        HILOG_ERROR(LOG_CORE, "chainId parsing failed");
        return false;
    }
    traceId.SetChainId(static_cast<uint64_t>(chainId));

    ani_int spanId = DEFAULT_SPAN_ID;
    if (!GetIntProp(env, traceIdAni, "spanId", spanId)) {
        HILOG_ERROR(LOG_CORE, "get spanId failed");
        return false;
    }
    traceId.SetSpanId(static_cast<uint64_t>(spanId));

    ani_int parentSpanId = DEFAULT_PARENT_SPAN_ID;
    if (!GetIntProp(env, traceIdAni, "parentSpanId", parentSpanId)) {
        HILOG_ERROR(LOG_CORE, "get parentSpanId failed");
        return false;
    }
    traceId.SetParentSpanId(static_cast<uint64_t>(parentSpanId));

    ani_int flags = DEFAULT_FLAGS;
    if (!GetIntProp(env, traceIdAni, "flags", flags)) {
        HILOG_ERROR(LOG_CORE, "get flags failed");
        return false;
    }
    traceId.SetFlags(static_cast<int>(flags));
    return true;
}

void HiTraceChainAni::End(ani_env* env, ani_object traceIdAni)
{
    HiTraceId traceId;
    if (!ParseHiTraceId(env, traceIdAni, traceId)) {
        HILOG_ERROR(LOG_CORE, "End ParseHiTraceId failed");
        return;
    }
    HiTraceChain::End(traceId);
}

void HiTraceChainAni::SetId(ani_env* env, ani_object traceIdAni)
{
    HiTraceId traceId;
    if (!ParseHiTraceId(env, traceIdAni, traceId)) {
        HILOG_ERROR(LOG_CORE, "SetId ParseHiTraceId failed");
        return;
    }
    HiTraceChain::SetId(traceId);
}

ani_object HiTraceChainAni::GetId(ani_env* env)
{
    HiTraceId traceId = HiTraceChain::GetId();
    return HiTraceChainAni::CreateHitraceIdAni(env, traceId);
}

void HiTraceChainAni::ClearId(ani_env* env)
{
    HiTraceChain::ClearId();
}

ani_object HiTraceChainAni::CreateSpan(ani_env* env)
{
    HiTraceId traceId = HiTraceChain::CreateSpan();
    return HiTraceChainAni::CreateHitraceIdAni(env, traceId);
}

void HiTraceChainAni::Tracepoint(ani_env* env, ani_enum_item modeAni, ani_enum_item typeAni,
    ani_object traceIdAni, ani_object msgAni)
{
    ani_int mode = 0;
    if (env->EnumItem_GetValue_Int(modeAni, &mode) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Tracepoint mode parsing failed");
        return;
    }
    ani_int type = 0;
    if (env->EnumItem_GetValue_Int(typeAni, &type) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Tracepoint type parsing failed");
        return;
    }
    HiTraceId traceId;
    if (!ParseHiTraceId(env, traceIdAni, traceId)) {
        HILOG_ERROR(LOG_CORE, "Tracepoint ParseHiTraceId failed");
        return;
    }
    std::string msg = "";
    ani_boolean isUndefined = true;
    if (env->Reference_IsUndefined(static_cast<ani_ref>(msgAni), &isUndefined) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Tracepoint get ref undefined failed");
        return;
    }
    if (!isUndefined && !HiTraceChainAniUtil::GetAniStringValue(env, static_cast<ani_string>(msgAni), msg)) {
        HILOG_ERROR(LOG_CORE, "Tracepoint msg parsing failed");
        return;
    }
    HiTraceChain::Tracepoint(static_cast<HiTraceCommunicationMode>(mode), static_cast<HiTraceTracepointType>(type),
        traceId, "%s", msg.c_str());
}

ani_boolean HiTraceChainAni::IsValid(ani_env* env, ani_object traceIdAni)
{
    bool isValid = false;
    HiTraceId traceId;
    if (!ParseHiTraceId(env, traceIdAni, traceId)) {
        HILOG_ERROR(LOG_CORE, "IsValid ParseHiTraceId failed");
        return static_cast<ani_boolean>(isValid);
    }
    isValid = traceId.IsValid();
    return static_cast<ani_boolean>(isValid);
}

ani_boolean HiTraceChainAni::IsFlagEnabled(ani_env* env, ani_object traceIdAni, ani_enum_item flagAni)
{
    bool isFlagEnabled = false;
    HiTraceId traceId;
    if (!ParseHiTraceId(env, traceIdAni, traceId)) {
        HILOG_ERROR(LOG_CORE, "IsFlagEnabled ParseHiTraceId failed");
        return static_cast<ani_boolean>(isFlagEnabled);
    }
    ani_int flag = 0;
    if (env->EnumItem_GetValue_Int(flagAni, &flag) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "IsFlagEnabled flag pasing failed");
        return static_cast<ani_boolean>(isFlagEnabled);
    }
    isFlagEnabled = traceId.IsFlagEnabled(static_cast<HiTraceFlag>(flag));
    return static_cast<ani_boolean>(isFlagEnabled);
}

void HiTraceChainAni::EnableFlag(ani_env* env, ani_object traceIdAni, ani_enum_item flagAni)
{
    HiTraceId traceId;
    if (!ParseHiTraceId(env, traceIdAni, traceId)) {
        HILOG_ERROR(LOG_CORE, "EnableFlag ParseHiTraceId failed");
        return;
    }
    ani_int flag = 0;
    if (env->EnumItem_GetValue_Int(flagAni, &flag) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "EnableFlag flag pasing failed");
        return;
    }
    traceId.EnableFlag(static_cast<HiTraceFlag>(flag));
    ani_object flagsObj {};
    if (!HiTraceChainAniUtil::CreateAniInt(env, traceId.GetFlags(), flagsObj)) {
        HILOG_ERROR(LOG_CORE, "EnableFlag CreateAniInt failed");
        return;
    }
    ani_status status = env->Object_SetPropertyByName_Ref(traceIdAni, "flags", static_cast<ani_ref>(flagsObj));
    if (status != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set flags failed");
    }
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm* vm, uint32_t* result)
{
    ani_env* env;
    if (vm->GetEnv(ANI_VERSION_1, &env) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "GetEnv failed");
        return ANI_ERROR;
    }
    ani_namespace ns {};
    if (env->FindNamespace(NAMESPACE_HITRACECHAIN, &ns) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "FindNamespace %{public}s failed", NAMESPACE_HITRACECHAIN);
        return ANI_ERROR;
    }
    std::array methods = {
        ani_native_function {"begin", nullptr, reinterpret_cast<void *>(HiTraceChainAni::Begin)},
        ani_native_function {"end", nullptr, reinterpret_cast<void *>(HiTraceChainAni::End)},
        ani_native_function {"setId", nullptr, reinterpret_cast<void *>(HiTraceChainAni::SetId)},
        ani_native_function {"getId", nullptr, reinterpret_cast<void *>(HiTraceChainAni::GetId)},
        ani_native_function {"clearId", nullptr, reinterpret_cast<void *>(HiTraceChainAni::ClearId)},
        ani_native_function {"createSpan", nullptr, reinterpret_cast<void *>(HiTraceChainAni::CreateSpan)},
        ani_native_function {"tracepoint", nullptr, reinterpret_cast<void *>(HiTraceChainAni::Tracepoint)},
        ani_native_function {"isValid", nullptr, reinterpret_cast<void *>(HiTraceChainAni::IsValid)},
        ani_native_function {"isFlagEnabled", nullptr, reinterpret_cast<void *>(HiTraceChainAni::IsFlagEnabled)},
        ani_native_function {"enableFlag", nullptr, reinterpret_cast<void *>(HiTraceChainAni::EnableFlag)},
    };
    if (env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size()) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "%{public}s bind native function failed", NAMESPACE_HITRACECHAIN);
        return ANI_ERROR;
    };
    *result = ANI_VERSION_1;
    return ANI_OK;
}
