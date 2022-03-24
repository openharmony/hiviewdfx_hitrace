/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef HIVIEWDFX_HITRACE_INNER_H
#define HIVIEWDFX_HITRACE_INNER_H

#include "hitrace/hitracec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HITRACE_INFO_FAIL (-1)
#define HITRACE_INFO_ALL_VALID (0)
#define HITRACE_INFO_ALL_VALID_EXCEPT_SPAN (1)

void HiTraceTracepointInner(HiTraceCommunicationMode mode, HiTraceTracepointType type, const HiTraceIdStruct* pId,
    const char* fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif // HIVIEWDFX_HITRACE_INNER_H
