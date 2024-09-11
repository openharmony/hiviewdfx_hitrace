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

#include "hitracedump_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "hitrace_dump.h"
#include "hitrace_fuzztest_common.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
void HitraceDumpCmdModeTest(const uint8_t* data, size_t size)
{
    uint64_t traceTags = 0;
    if (size < sizeof(traceTags)) {
        return;
    }
    StreamToValueInfo(data, traceTags);
    std::string hitraceTags = " sched freq binder idle disk";
    GenerateTagStr(traceTags, hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)OpenTrace(hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)DumpTraceOn();
    sleep(1);
    (void)DumpTraceOff();
    (void)CloseTrace();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
}

void HitraceDumpServiceModeTest(const uint8_t* data, size_t size)
{
    int duration = 0;
    uint64_t happenTime = 0;
    uint64_t traceTags = 0;
    if (size < sizeof(traceTags) + sizeof(duration) + sizeof(happenTime)) {
        return;
    }
    StreamToValueInfo(data, duration);
    StreamToValueInfo(data, happenTime);
    StreamToValueInfo(data, traceTags);
    duration = duration > 30 ? 30 : duration; // 30 : max duration
    std::vector<std::string> hitraceTags = { "sched", "freq", "binder", "idle", "disk" };
    GenerateTagVec(traceTags, hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)OpenTrace(hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    sleep(1);
    (void)DumpTrace();
    sleep(1);
    (void)DumpTrace(duration, happenTime);
    (void)CloseTrace();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
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

    /* Run your code on data */
    OHOS::HiviewDFX::Hitrace::HitraceDumpCmdModeTest(data, size);
    OHOS::HiviewDFX::Hitrace::HitraceDumpServiceModeTest(data, size);
    return 0;
}
