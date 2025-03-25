/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef HITRACE_METER_WRAPPER_H
#define HITRACE_METER_WRAPPER_H

#include <stdint.h>

#include "hitrace/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

void StartTraceCwrapper(uint64_t tag, const char *value);

void FinishTraceCwrapper(uint64_t tag);

void StartAsyncTraceCwrapper(uint64_t tag, const char *value, int32_t taskId);

void FinishAsyncTraceCwrapper(uint64_t tag, const char *value, int32_t taskId);

void CountTraceCwrapper(uint64_t tag, const char *value, int64_t count);

struct HiTraceIdStruct;
void StartTraceChainPoint(const struct HiTraceIdStruct* hiTraceId, const char *value);

#ifdef __cplusplus
}
#endif
#endif /* HITRACE_METER_WRAPPER_H */
