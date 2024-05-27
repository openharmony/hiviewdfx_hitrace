/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef HITRACEMETER_IMPL_H
#define HITRACEMETER_IMPL_H

#include <cstdint>
namespace OHOS {
namespace CJSystemapi {

constexpr uint64_t HITRACE_TAG_APP = (1ULL << 62); // App tag

class HiTraceMeterImpl {
public:
    static void HiTraceStartAsyncTrace(const char*  name, int taskId);
    static void HiTraceFinishAsyncTrace(const char* name, int taskId);
    static void HiTraceCountTrace(const char* name, int count);
};

} // CJSystemapi
} // OHOS

#endif