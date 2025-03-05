/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#include "hitrace_meter.h"
#include "hitrace_meter_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

void StartTraceCwrapper(uint64_t tag, const char* name)
{
    StartTraceEx(HITRACE_LEVEL_INFO, tag, name, "");
}

void FinishTraceCwrapper(uint64_t tag)
{
    FinishTraceEx(HITRACE_LEVEL_INFO, tag);
}

void StartAsyncTraceCwrapper(uint64_t tag, const char* name, int32_t taskId)
{
    StartAsyncTraceEx(HITRACE_LEVEL_INFO, tag, name, taskId, "", "");
}

void FinishAsyncTraceCwrapper(uint64_t tag, const char* name, int32_t taskId)
{
    FinishAsyncTraceEx(HITRACE_LEVEL_INFO, tag, name, taskId);
}

void CountTraceCwrapper(uint64_t tag, const char* name, int64_t count)
{
    CountTraceEx(HITRACE_LEVEL_INFO, tag, name, count);
}

void StartTraceChainPoint(const struct HiTraceIdStruct* hiTraceId, const char* value)
{
    StartTraceChain(HITRACE_TAG_OHOS, hiTraceId, value);
}

void StartTraceExCwrapper(HiTraceOutputLevel level, uint64_t tag, const char* name, const char* customArgs)
{
    StartTraceEx(level, tag, name, customArgs);
}

void FinishTraceExCwrapper(HiTraceOutputLevel level, uint64_t tag)
{
    FinishTraceEx(level, tag);
}

void StartAsyncTraceExCwrapper(HiTraceOutputLevel level, uint64_t tag, const char* name, int32_t taskId,
    const char* customCategory, const char* customArgs)
{
    StartAsyncTraceEx(level, tag, name, taskId, customCategory, customArgs);
}

void FinishAsyncTraceExCwrapper(HiTraceOutputLevel level, uint64_t tag, const char* name, int32_t taskId)
{
    FinishAsyncTraceEx(level, tag, name, taskId);
}

void CountTraceExCwrapper(HiTraceOutputLevel level, uint64_t tag, const char* name, int64_t count)
{
    CountTraceEx(level, tag, name, count);
}

bool IsTagEnabledCwrapper(uint64_t tag)
{
    return IsTagEnabled(tag);
}

#ifdef __cplusplus
}
#endif