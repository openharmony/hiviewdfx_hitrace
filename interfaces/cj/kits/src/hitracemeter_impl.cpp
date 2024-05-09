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

#include "hitracemeter_impl.h"
#include "hitrace_meter.h"
#include <string>

namespace OHOS {
namespace CJSystemapi {

void HiTraceMeterImpl::HiTraceStartAsyncTrace(const char* name, int taskId)
{
    StartAsyncTrace(HITRACE_TAG_APP, std::string(name), taskId);
}

void HiTraceMeterImpl::HiTraceFinishAsyncTrace(const char* name, int taskId)
{
    FinishAsyncTrace(HITRACE_TAG_APP, std::string(name), taskId);
}

void HiTraceMeterImpl::HiTraceCountTrace(const char* name, int count)
{
    CountTrace(HITRACE_TAG_APP, std::string(name), count);
}

} // CJSystemapi
} // OHOS