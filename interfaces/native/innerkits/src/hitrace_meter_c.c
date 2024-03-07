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

#include "hitrace_meter_c.h"
#include "hitrace_meter_wrapper.h"

void OH_HiTrace_StartTrace_C(uint64_t tag, const char* value)
{
    StartTraceCwrapper(tag, value);
}

void OH_HiTrace_FinishTrace_C(uint64_t tag)
{
    FinishTraceCwrapper(tag);
}

void OH_HiTrace_StartAsyncTrace_C(uint64_t tag, const char* value, int32_t taskId)
{
    StartAsyncTraceCwrapper(tag, value, taskId);
}

void OH_HiTrace_FinishAsyncTrace_C(uint64_t tag, const char* value, int32_t taskId)
{
    FinishAsyncTraceCwrapper(tag, value, taskId);
}

void OH_HiTrace_CountTrace_C(uint64_t tag, const char* value, int32_t count)
{
    CountTraceCwrapper(tag, value, count);
}