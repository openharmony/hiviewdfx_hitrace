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

#include "hitrace_meter_wrapper.h"
#include "hitrace_meter.h"

#ifdef __cplusplus
extern "C" {
#endif

void StartTraceCwrapper(uint64_t tag, const char *value)
{
    StartTrace(tag, value);
}

void FinishTraceCwrapper(uint64_t tag)
{
    FinishTrace(tag);
}

void StartAsyncTraceCwrapper(uint64_t tag, const char *value, int32_t taskId)
{
    StartAsyncTrace(tag, value, taskId);
}

void FinishAsyncTraceCwrapper(uint64_t tag, const char *value, int32_t taskId)
{
    FinishAsyncTrace(tag, value, taskId);
}

void CountTraceCwrapper(uint64_t tag, const char *value, int64_t count)
{
    CountTrace(tag, value, count);
}

void StartTraceChainPoint(const struct HiTraceIdStruct* hiTraceId, const char *value)
{
    StartTraceChain(HITRACE_TAG_OHOS, hiTraceId, value);
}

#ifdef __cplusplus
}
#endif