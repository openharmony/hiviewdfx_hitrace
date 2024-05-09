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

#include "hitracemeter_ffi.h"
#include "hitracemeter_impl.h"

using namespace OHOS::CJSystemapi;

extern "C" {
    void FfiOHOSHiTraceStartAsyncTrace(const char* name, int taskId)
    {
        HiTraceMeterImpl::HiTraceStartAsyncTrace(name, taskId);
    }

    void FfiOHOSHiTraceFinishAsyncTrace(const char* name, int taskId)
    {
        HiTraceMeterImpl::HiTraceFinishAsyncTrace(name, taskId);
    }

    void FfiOHOSHiTraceCountTrace(const char* name, int count)
    {
        HiTraceMeterImpl::HiTraceCountTrace(name, count);
    }
}