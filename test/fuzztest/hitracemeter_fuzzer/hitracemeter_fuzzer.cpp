/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "common_define.h"
#include "common_utils.h"
#include "hitracemeter_fuzzer.h"
#include "hitrace_fuzztest_common.h"
#include "hitrace_meter.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
void HitraceMeterTest(const uint8_t* data, size_t size)
{
    int32_t taskId = 0;
    if (size < sizeof(taskId)) {
        return;
    }
    StreamToValueInfo(data, taskId);
    std::string name(reinterpret_cast<const char*>(data), size - sizeof(taskId));
    std::string customArgs = name;
    std::string customCategory = name;
    StartTraceEx(HITRACE_LEVEL_INFO, HITRACE_TAG_APP, name.c_str(), customArgs.c_str());
    FinishTraceEx(HITRACE_LEVEL_INFO, HITRACE_TAG_APP);
    StartAsyncTraceEx(HITRACE_LEVEL_INFO, HITRACE_TAG_APP, name.c_str(), taskId,
                      customCategory.c_str(), customArgs.c_str());
    FinishAsyncTraceEx(HITRACE_LEVEL_INFO, HITRACE_TAG_APP, name.c_str(), taskId);
    CountTraceEx(HITRACE_LEVEL_INFO, HITRACE_TAG_APP, name.c_str(), taskId);
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }
    std::string value = std::to_string(HITRACE_TAG_APP);
    bool ret = OHOS::HiviewDFX::Hitrace::SetPropertyInner(TRACE_TAG_ENABLE_FLAGS, value);
    if (!ret) {
        return 0;
    }
    /* Run your code on data */
    OHOS::HiviewDFX::Hitrace::HitraceMeterTest(data, size);
    return 0;
}
