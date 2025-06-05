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
#include <functional>

#include "hitrace_dump.h"
#include "hitrace_fuzztest_common.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
constexpr int MAX_DURATION = 30;
constexpr int MAX_FILE_SIZE = 1024 * 1024;
}
void HitraceDumpCmdModeTest(const uint8_t* data, size_t size)
{
    uint64_t traceTags = 0;
    if (size < sizeof(traceTags)) {
        return;
    }
    StreamToValueInfo(data, traceTags);
    std::string hitraceTags = " sched freq idle disk";
    GenerateTagStr(traceTags, hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)OpenTrace(hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)RecordTraceOn();
    sleep(1);
    (void)RecordTraceOff();
    (void)CloseTrace();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
}

void HitraceDumpCacheTest(const uint8_t* data, size_t size)
{
    uint64_t traceTags = 0;
    if (size < sizeof(traceTags)) {
        return;
    }
    StreamToValueInfo(data, traceTags);
    std::vector<std::string> hitraceTags = { "sched", "freq", "idle", "disk" };
    GenerateTagVec(traceTags, hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)OpenTrace(hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    sleep(1);
    (void)CacheTraceOn();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    sleep(1);
    (void)DumpTrace();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    sleep(1);
    (void)CacheTraceOff();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    sleep(1);
    (void)DumpTrace();
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
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
    std::vector<std::string> hitraceTags = { "sched", "freq", "idle", "disk" };
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

void HitraceDumpTest(const uint8_t* data, size_t size)
{
    bool enable = 0;
    if (size < sizeof(enable)) {
        return;
    }
    StreamToValueInfo(data, enable);
    SetTraceStatus(enable);
}

void HitraceDumpAsyncTest(const uint8_t* data, size_t size)
{
    int maxDuration = 0;
    uint64_t utTraceEndTime = 0;
    int64_t fileSizeLimit = 0;
    if (size < sizeof(maxDuration) + sizeof(utTraceEndTime) + sizeof(fileSizeLimit)) {
        return;
    }
    StreamToValueInfo(data, maxDuration);
    StreamToValueInfo(data, utTraceEndTime);
    StreamToValueInfo(data, fileSizeLimit);

    maxDuration = maxDuration > MAX_DURATION ? MAX_DURATION : maxDuration;
    fileSizeLimit = fileSizeLimit > MAX_FILE_SIZE ? MAX_FILE_SIZE : fileSizeLimit;

    std::vector<std::string> hitraceTags = { "sched", "freq", "idle", "disk" };
    std::cout << "trace mode : " << GetTraceMode() << std::endl;
    (void)OpenTrace(hitraceTags);
    std::cout << "trace mode : " << GetTraceMode() << std::endl;

    // 定义异步回调函数
    std::function<void(TraceRetInfo)> asyncCallback = [](TraceRetInfo traceInfo) {
        std::cout << "Async dump completed with error code: " << static_cast<int>(traceInfo.errorCode) << std::endl;
        for (const auto& file : traceInfo.outputFiles) {
            std::cout << "Output file: " << file << std::endl;
        }
    };

    (void)DumpTraceAsync(maxDuration, utTraceEndTime, fileSizeLimit, asyncCallback);
    sleep(1);
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
    OHOS::HiviewDFX::Hitrace::HitraceDumpCacheTest(data, size);
    OHOS::HiviewDFX::Hitrace::HitraceDumpServiceModeTest(data, size);
    OHOS::HiviewDFX::Hitrace::HitraceDumpTest(data, size);
    OHOS::HiviewDFX::Hitrace::HitraceDumpAsyncTest(data, size);
    return 0;
}
