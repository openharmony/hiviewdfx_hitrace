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
    HiTraceId traceId;
    std::string name;
    ani_object val = {};
    if (HiTraceChainAniUtil::GetAniStringValue(env, nameAni, name) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get name failed.");
        return val;
    }
    int flag = HiTraceFlag::HITRACE_FLAG_DEFAULT;
    if (!HiTraceChainAniUtil::IsRefUndefined(env, static_cast<ani_ref>(flagAni))) {
        flag = HiTraceChainAniUtil::GetAniIntValue(env, flagAni);
    }
    traceId = HiTraceChain::Begin(name, flag);
    val = HiTraceChainAni::CreateHitraceIdAni(env, traceId);
    return val;
};


static ani_status SetChainId(ani_env* env, HiTraceId& traceId, ani_object& hiTrace_object, ani_class cls)
{
    ani_method chainIdSetter;
    if (env->Class_FindMethod(cls, "<set>chainId", nullptr, &chainIdSetter) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID_CHAINID);
        return ANI_ERROR;
    }
    uint64_t chainId = traceId.GetChainId();
    ani_object bigint_ctor = HiTraceChainAniUtil::CreateAniBigInt(env, chainId);
    if (env->Object_CallMethod_Void(hiTrace_object, chainIdSetter, bigint_ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", CLASS_NAME_HITRACEID_CHAINID);
        return ANI_ERROR;
    }
    return ANI_OK;
}

static ani_status SetSpanId(ani_env* env, HiTraceId& traceId, ani_object& hiTrace_object, ani_class cls)
{
    ani_method parentSpanIdSetter;
    if (env->Class_FindMethod(cls, "<set>parentSpanId", nullptr, &parentSpanIdSetter) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID_PARENTSSPANID);
        return ANI_ERROR;
    }
    uint64_t parentSpanId = traceId.GetParentSpanId();
    ani_object parentSpanId_ctor = HiTraceChainAniUtil::CreateAniInt(env, parentSpanId);
    if (env->Object_CallMethod_Void(hiTrace_object, parentSpanIdSetter, parentSpanId_ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", CLASS_NAME_HITRACEID_PARENTSSPANID);
        return ANI_ERROR;
    }
    return ANI_OK;
}

static ani_status SetParentSpanId(ani_env* env, HiTraceId& traceId, ani_object& hiTrace_object, ani_class cls)
{
    ani_method spanIdSetter;
    if (env->Class_FindMethod(cls, "<set>spanId", nullptr, &spanIdSetter) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID_SPANID);
        return ANI_ERROR;
    }
    uint64_t spanId = traceId.GetSpanId();
    ani_object spanId_ctor = HiTraceChainAniUtil::CreateAniInt(env, spanId);
    if (env->Object_CallMethod_Void(hiTrace_object, spanIdSetter, spanId_ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", CLASS_NAME_HITRACEID_SPANID);
        return ANI_ERROR;
    }
    return ANI_OK;
}

static ani_status SetFlags(ani_env* env, HiTraceId& traceId, ani_object& hiTrace_object, ani_class cls)
{
    ani_method flagsSetter;
    if (env->Class_FindMethod(cls, "<set>flags", nullptr, &flagsSetter) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID_FLAGS);
        return ANI_ERROR;
    }
    int flags = traceId.GetFlags();
    ani_object flags_ctor = HiTraceChainAniUtil::CreateAniInt(env, static_cast<uint64_t>(flags));
    if (env->Object_CallMethod_Void(hiTrace_object, flagsSetter, flags_ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "call method %{public}s failed", CLASS_NAME_HITRACEID_FLAGS);
        return ANI_ERROR;
    }
    return ANI_OK;
}

ani_object HiTraceChainAni::CreateHitraceIdAni(ani_env* env, HiTraceId& traceId)
{
    ani_object hiTrace_object = nullptr;
    ani_class cls;
    if (env->FindClass(CLASS_NAME_HITRACEID, &cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_HITRACEID);
        return hiTrace_object;
    }
    ani_method ctor;
    if (env->Class_FindMethod(cls, "<ctor>", nullptr, &ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method %{public}s failed", CLASS_NAME_HITRACEID);
        return hiTrace_object;
    }
    if (env->Object_New(cls, ctor, &hiTrace_object) != ANI_OK ||
        SetChainId(env, traceId, hiTrace_object, cls) != ANI_OK ||
        SetSpanId(env, traceId, hiTrace_object, cls) != ANI_OK ||
        SetParentSpanId(env, traceId, hiTrace_object, cls) != ANI_OK ||
        SetFlags(env, traceId, hiTrace_object, cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object %{public}s failed", CLASS_NAME_HITRACEID);
    }
    return hiTrace_object;
}

static void ParseHiTraceId(ani_env* env, ani_object HiTraceIdAni, HiTraceId& traceId)
{
    ani_ref chainIdRef {};
    if (env->Object_GetPropertyByName_Ref(HiTraceIdAni, "chainId", &chainIdRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get chainId %{public}s failed", CLASS_NAME_HITRACEID);
        return;
    }
    uint64_t chainId = DEFAULT_CHAIN_ID;
    chainId = static_cast<uint64_t>(HiTraceChainAniUtil::GetAniBigIntValue(env, chainIdRef));
    traceId.SetChainId(chainId);

    ani_ref spanIdRef {};
    if (env->Object_GetPropertyByName_Ref(HiTraceIdAni, "spanId", &spanIdRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get spanId %{public}s failed", CLASS_NAME_HITRACEID);
        return;
    }
    uint64_t spanId = DEFAULT_SPAN_ID;
    if (!HiTraceChainAniUtil::IsRefUndefined(env, spanIdRef)) {
        spanId = static_cast<uint64_t>(HiTraceChainAniUtil::GetAniIntValue(env, spanIdRef));
        traceId.SetSpanId(spanId);
    }

    ani_ref parentSpanIdRef {};
    if (env->Object_GetPropertyByName_Ref(HiTraceIdAni, "parentSpanId", &parentSpanIdRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get parentSpanId %{public}s failed", CLASS_NAME_HITRACEID);
        return;
    }
    uint64_t parentSpanId = DEFAULT_PARENT_SPAN_ID;
    if (!HiTraceChainAniUtil::IsRefUndefined(env, parentSpanIdRef)) {
        parentSpanId = static_cast<uint64_t>(HiTraceChainAniUtil::GetAniIntValue(env, parentSpanIdRef));
        traceId.SetParentSpanId(parentSpanId);
    }

    ani_ref flagsRef {};
    if (env->Object_GetPropertyByName_Ref(HiTraceIdAni, "flags", &flagsRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "get flags %{public}s failed", CLASS_NAME_HITRACEID);
        return;
    }
    int flags = DEFAULT_FLAGS;
    if (!HiTraceChainAniUtil::IsRefUndefined(env, flagsRef)) {
        flags = HiTraceChainAniUtil::GetAniIntValue(env, flagsRef);
        traceId.SetFlags(flags);
    }
}

void HiTraceChainAni::End(ani_env* env, ani_object HiTraceIdAni)
{
    HiTraceId traceId;
    ParseHiTraceId(env, HiTraceIdAni, traceId);
    HiTraceChain::End(traceId);
}

void HiTraceChainAni::SetId(ani_env* env, ani_object HiTraceIdAni)
{
    HiTraceId traceId;
    ParseHiTraceId(env, HiTraceIdAni, traceId);
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

void HiTraceChainAni::Tracepoint(ani_env* env, ani_enum_item modeAni,
    ani_enum_item typeAni, ani_object HiTraceIdAni, ani_string msgAni)
{
    int communicationModeInt = 0;
    if (HiTraceChainAniUtil::AniEnumToInt(env, modeAni, communicationModeInt) != ANI_OK) {
        return;
    }
    HiTraceCommunicationMode communicationMode = HiTraceCommunicationMode(communicationModeInt);
    int tracePointTypeInt = 0;
    if (HiTraceChainAniUtil::AniEnumToInt(env, typeAni, tracePointTypeInt) != ANI_OK) {
        return;
    }
    HiTraceTracepointType tracePointType = HiTraceTracepointType(tracePointTypeInt);
    HiTraceId traceId;
    ParseHiTraceId(env, HiTraceIdAni, traceId);
    std::string description = "";
    if (!HiTraceChainAniUtil::IsRefUndefined(env, static_cast<ani_ref>(msgAni))) {
        if (HiTraceChainAniUtil::GetAniStringValue(env, msgAni, description) != ANI_OK) {
            HILOG_ERROR(LOG_CORE, "get description failed.");
            return;
        }
    }
    HiTraceChain::Tracepoint(communicationMode, tracePointType, traceId, "%s", description.c_str());
}

ani_boolean HiTraceChainAni::IsValid(ani_env* env, ani_object HiTraceIdAni)
{
    bool isValid = false;
    HiTraceId traceId;
    ParseHiTraceId(env, HiTraceIdAni, traceId);
    isValid = traceId.IsValid();
    return static_cast<ani_boolean>(isValid);
}

ani_boolean HiTraceChainAni::IsFlagEnabled(ani_env* env, ani_object HiTraceIdAni,
    ani_enum_item flagAni)
{
    bool isFalgEnabled = false;
    HiTraceId traceId;
    ParseHiTraceId(env, HiTraceIdAni, traceId);
    int traceFlagInt = 0;
    if (HiTraceChainAniUtil::AniEnumToInt(env, flagAni, traceFlagInt) != ANI_OK) {
        return isFalgEnabled;
    }
    HiTraceFlag traceFlag = HiTraceFlag(traceFlagInt);
    isFalgEnabled = traceId.IsFlagEnabled(traceFlag);
    return static_cast<ani_boolean>(isFalgEnabled);
}

void HiTraceChainAni::EnableFlag(ani_env* env, ani_object HiTraceIdAni,
    ani_enum_item flagAni)
{
    HiTraceId traceId;
    ParseHiTraceId(env, HiTraceIdAni, traceId);
    int traceFlagInt = 0;
    if (HiTraceChainAniUtil::AniEnumToInt(env, flagAni, traceFlagInt) != ANI_OK) {
        return;
    }
    HiTraceFlag traceFlag = HiTraceFlag(traceFlagInt);
    traceId.EnableFlag(traceFlag);
    ani_object flagsObj = HiTraceChainAniUtil::CreateAniInt(env, traceId.GetFlags());
    ani_status status = env->Object_SetPropertyByName_Ref(HiTraceIdAni, "flags", static_cast<ani_ref>(flagsObj));
    if (status != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create object failed");
    }
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm* vm, uint32_t* result)
{
    ani_env* env;
    if (vm->GetEnv(ANI_VERSION_1, &env) != ANI_OK) {
        return ANI_ERROR;
    }
    ani_namespace ns {};
    if (env->FindNamespace(CLASS_NAME_HITRACECHAIN, &ns) != ANI_OK) {
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
        return ANI_ERROR;
    };
    *result = ANI_VERSION_1;
    return ANI_OK;
}
