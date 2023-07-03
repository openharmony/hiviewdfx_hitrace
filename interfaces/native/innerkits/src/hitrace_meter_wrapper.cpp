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

void StartTraceCwrapper(const char *value)
{
    StartTrace(HITRACE_TAG_APP, value);
}

void FinishTraceCwrapper(void)
{
    FinishTrace(HITRACE_TAG_APP);
}

void StartAsyncTraceCwrapper(const char *value, int32_t taskId)
{
    StartAsyncTrace(HITRACE_TAG_APP, value, taskId);
}

void FinishAsyncTraceCwrapper(const char *value, int32_t taskId)
{
    FinishAsyncTrace(HITRACE_TAG_APP, value, taskId);
}

void CountTraceCwrapper(const char *value, int64_t count)
{
    CountTrace(HITRACE_TAG_APP, value, count);
}

#ifdef __cplusplus
}
#endif