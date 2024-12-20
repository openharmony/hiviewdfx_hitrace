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

void OH_HiTrace_StartTrace(const char *name)
{
    StartTraceCwrapper(HITRACE_TAG_APP, name);
}

void OH_HiTrace_FinishTrace(void)
{
    FinishTraceCwrapper(HITRACE_TAG_APP);
}

void OH_HiTrace_StartAsyncTrace(const char *name, int32_t taskId)
{
    StartAsyncTraceCwrapper(HITRACE_TAG_APP, name, taskId);
}

void OH_HiTrace_FinishAsyncTrace(const char *name, int32_t taskId)
{
    FinishAsyncTraceCwrapper(HITRACE_TAG_APP, name, taskId);
}

void OH_HiTrace_CountTrace(const char *name, int64_t count)
{
    CountTraceCwrapper(HITRACE_TAG_APP, name, count);
}