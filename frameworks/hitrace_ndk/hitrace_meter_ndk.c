/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "hitrace_meter_wrapper.h"

#ifndef HITRACE_TAG_APP

#define HITRACE_TAG_APP (1ULL << 62)

#endif

typedef HiTraceOutputLevel HiTrace_Output_Level;

void OH_HiTrace_StartTrace(const char *name)
{
    StartTraceExCwrapper(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name, "");
}

void OH_HiTrace_FinishTrace(void)
{
    FinishTraceExCwrapper(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP);
}

void OH_HiTrace_StartAsyncTrace(const char *name, int32_t taskId)
{
    StartAsyncTraceExCwrapper(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name, taskId, "", "");
}

void OH_HiTrace_FinishAsyncTrace(const char *name, int32_t taskId)
{
    FinishAsyncTraceExCwrapper(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name, taskId);
}

void OH_HiTrace_CountTrace(const char *name, int64_t count)
{
    CountTraceExCwrapper(HITRACE_LEVEL_COMMERCIAL, HITRACE_TAG_APP, name, count);
}

void OH_HiTrace_StartTraceEx(HiTrace_Output_Level level, const char *name, const char *customArgs)
{
    StartTraceExCwrapper(level, HITRACE_TAG_APP, name, customArgs);
}

void OH_HiTrace_FinishTraceEx(HiTrace_Output_Level level)
{
    FinishTraceExCwrapper(level, HITRACE_TAG_APP);
}

void OH_HiTrace_StartAsyncTraceEx(HiTrace_Output_Level level, const char *name, int32_t taskId,
    const char *customCategory, const char *customArgs)
{
    StartAsyncTraceExCwrapper(level, HITRACE_TAG_APP, name, taskId, customCategory, customArgs);
}

void OH_HiTrace_FinishAsyncTraceEx(HiTrace_Output_Level level, const char *name, int32_t taskId)
{
    FinishAsyncTraceExCwrapper(level, HITRACE_TAG_APP, name, taskId);
}

void OH_HiTrace_CountTraceEx(HiTrace_Output_Level level, const char *name, int64_t count)
{
    CountTraceExCwrapper(level, HITRACE_TAG_APP, name, count);
}

bool OH_HiTrace_IsTraceEnabled(void)
{
    return IsTagEnabledCwrapper(HITRACE_TAG_APP);
}