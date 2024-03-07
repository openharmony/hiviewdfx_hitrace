/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#ifndef HITRACE_METER_H
#define HITRACE_METER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void OH_HiTrace_StartTrace_C(uint64_t tag, const char* value);
void OH_HiTrace_FinishTrace_C(uint64_t tag);
void OH_HiTrace_StartAsyncTrace_C(uint64_t tag, const char* value, int32_t taskId);
void OH_HiTrace_FinishAsyncTrace_C(uint64_t tag, const char* value, int32_t taskId);
void OH_HiTrace_CountTrace_C(uint64_t tag, const char* value, int32_t count);

#ifdef __cplusplus
}
#endif
#endif /* HITRACE_METER_H */