/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#ifndef INTERFACES_INNERKITS_NATIVE_HITRACE_METER_H
#define INTERFACES_INNERKITS_NATIVE_HITRACE_METER_H

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

constexpr uint64_t BYTRACE_TAG_NEVER = 0; // This tag is never enabled.
constexpr uint64_t BYTRACE_TAG_ALWAYS = (1ULL << 0); // This tag is always enabled.
constexpr uint64_t BYTRACE_TAG_OHOS = (1ULL << 30); // OHOS generic tag.
constexpr uint64_t BYTRACE_TAG_ABILITY_MANAGER = (1ULL << 31); // Ability Manager tag.
constexpr uint64_t BYTRACE_TAG_ZCAMERA = (1ULL << 32); // Camera module tag.
constexpr uint64_t BYTRACE_TAG_ZMEDIA = (1ULL << 33); // Media module tag.
constexpr uint64_t BYTRACE_TAG_ZIMAGE = (1ULL << 34); // Image module tag.
constexpr uint64_t BYTRACE_TAG_ZAUDIO = (1ULL << 35); // Audio module tag.
constexpr uint64_t BYTRACE_TAG_DISTRIBUTEDDATA = (1ULL << 36); // Distributeddata manager module tag.
constexpr uint64_t BYTRACE_TAG_MDFS = (1ULL << 37); // Mobile distributed file system tag.
constexpr uint64_t BYTRACE_TAG_GRAPHIC_AGP = (1ULL << 38); // Graphic module tag.
constexpr uint64_t BYTRACE_TAG_ACE = (1ULL << 39); // ACE development framework tag.
constexpr uint64_t BYTRACE_TAG_NOTIFICATION = (1ULL << 40); // Notification module tag.
constexpr uint64_t BYTRACE_TAG_MISC = (1ULL << 41); // Notification module tag.
constexpr uint64_t BYTRACE_TAG_MULTIMODALINPUT = (1ULL << 42); // Multi modal module tag.
constexpr uint64_t BYTRACE_TAG_SENSORS = (1ULL << 43); // Sensors mudule tag.
constexpr uint64_t BYTRACE_TAG_MSDP = (1ULL << 44); // Multimodal Sensor Data Platform module tag.
constexpr uint64_t BYTRACE_TAG_DSOFTBUS = (1ULL << 45); // Distributed Softbus tag.
constexpr uint64_t BYTRACE_TAG_RPC = (1ULL << 46); // RPC and IPC tag.
constexpr uint64_t BYTRACE_TAG_ARK = (1ULL << 47); // ARK tag.
constexpr uint64_t BYTRACE_TAG_WINDOW_MANAGER = (1ULL << 48); // window manager tag.
constexpr uint64_t BYTRACE_TAG_APP = (1ULL << 62); // App tag.

constexpr uint64_t BYTRACE_TAG_LAST = BYTRACE_TAG_APP;
constexpr uint64_t BYTRACE_TAG_NOT_READY = (1ULL << 63); // Reserved for initialization.
constexpr uint64_t BYTRACE_TAG_VALID_MASK = ((BYTRACE_TAG_LAST - 1) | BYTRACE_TAG_LAST);

#ifndef BYTRACE_TAG
#define BYTRACE_TAG BYTRACE_TAG_NEVER
#elif BYTRACE_TAG > BYTRACE_TAG_VALID_MASK
#error BYTRACE_TAG must be defined to be one of the tags defined in hitrace_meter.h
#elif BYTRACE_TAG < BYTRACE_TAG_OHOS
#error BYTRACE_TAG must be defined to be one of the tags defined in hitrace_meter.h
#endif

#define RELEASE_LEVEL 0X01
#define DEBUG_LEVEL 0X02

#ifndef TRACE_LEVEL
#define TRACE_LEVEL RELEASE
#endif

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define BYTRACE_NAME(TAG, fmt, ...) ByTraceScoped TOKENPASTE2(tracer, __LINE__)(TAG, fmt, ##__VA_ARGS__)
#define BYTRACE(TAG) BYTRACE_NAME(TAG, __func__)

/**
 * Update trace label when your process has started.
 */
void UpdateTraceLabel();

/**
 * Track the beginning of a context.
 */
void StartTrace(uint64_t label, const std::string& value, float limit = -1);
void StartTraceDebug(uint64_t label, const std::string& value, float limit = -1);
/**
 * Track the end of a context.
 */
void FinishTrace(uint64_t label);
void FinishTraceDebug(uint64_t label);
/**
 * Track the beginning of an asynchronous event.
 */
void StartAsyncTrace(uint64_t label, const std::string& value, int32_t taskId, float limit = -1);
void StartAsyncTraceDebug(uint64_t label, const std::string& value, int32_t taskId, float limit = -1);

/**
 * Track the end of an asynchronous event.
 */
void FinishAsyncTrace(uint64_t label, const std::string& value, int32_t taskId);
void FinishAsyncTraceDebug(uint64_t label, const std::string& value, int32_t taskId);

/**
 * Track the middle of a context. Match the previous function of StartTrace before it.
 */
void MiddleTrace(uint64_t label, const std::string& beforeValue, const std::string& afterValue);
void MiddleTraceDebug(uint64_t label, const std::string& beforeValue, const std::string& afterValue);

/**
 * Track the 64-bit integer counter value.
 */
void CountTrace(uint64_t label, const std::string& name, int64_t count);
void CountTraceDebug(uint64_t label, const std::string& name, int64_t count);

class ByTraceScoped {
public:
    inline ByTraceScoped(uint64_t tag, const std::string &value) : mTag(tag)
    {
        StartTrace(mTag, value);
    }

    inline ~ByTraceScoped()
    {
        FinishTrace(mTag);
    }
private:
    uint64_t mTag;
};
#ifdef __cplusplus
}
#endif
#endif // INTERFACES_INNERKITS_NATIVE_HITRACE_METER_H
