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

#include "hitrace_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <vector>

#include "hitrace_fuzztest_common.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
void HitraceCommonTest(const uint8_t* data, size_t size)
{
    int bufferSz = 0;
    uint32_t duration = 0;
    uint64_t traceTags = 0;
    if (size < sizeof(bufferSz) + sizeof(duration) + sizeof(traceTags)) {
        return;
    }
    StreamToValueInfo(data, bufferSz);
    bufferSz += 256; // 256 : min trace buffer size
    StreamToValueInfo(data, duration);
    StreamToValueInfo(data, traceTags);
    duration = duration > 10 ? 10 : duration; // 10 : max duration
    std::string hitraceCmd = "hitrace -b " + std::to_string(bufferSz) + " -t " + std::to_string(duration);
    std::string hitraceTags = " sched freq binder idle disk";
    GenerateTagStr(traceTags, hitraceTags);
    std::string outputCmd = " -o /data/local/tmp/HitraceCommonTest_fuzz.ftrace";
    std::cout << hitraceCmd << hitraceTags << outputCmd << std::endl;
    system((hitraceCmd + hitraceTags + outputCmd).c_str());
}

void HitraceRecordTest(const uint8_t* data, size_t size)
{
    int bufferSz = 0;
    bool isRaw = false;
    int recordCmdRand = 0;
    uint64_t traceTags = 0;
    if (size < sizeof(bufferSz) + sizeof(isRaw) + sizeof(recordCmdRand) + sizeof(traceTags)) {
        return;
    }
    StreamToValueInfo(data, bufferSz);
    bufferSz += 256; // 256 : min trace buffer size
    StreamToValueInfo(data, isRaw);
    StreamToValueInfo(data, recordCmdRand);
    StreamToValueInfo(data, traceTags);
    const std::vector<std::string> recordCmdList = {
        "hitrace --trace_begin",
        "hitrace --trace_dump",
        "hitrace --trace_finish",
        "hitrace --trace_finish_nodump"
    };
    std::string hitraceBuffer = " -b " + std::to_string(bufferSz);
    std::string hitraceTags = " sched freq binder idle disk";
    GenerateTagStr(traceTags, hitraceTags);
    std::string outputCmd = " -o /data/local/tmp/HitraceCommonTest_fuzz.ftrace";
    std::string testCmd = recordCmdList[recordCmdRand % recordCmdList.size()] + hitraceBuffer + hitraceTags + outputCmd;
    std::cout << testCmd << std::endl;
    system(testCmd.c_str());
}

void HitracesSnapshotTest(const uint8_t* data, size_t size)
{
    int snapshotCmdRand = 0;
    if (size < sizeof(snapshotCmdRand)) {
        return;
    }
    StreamToValueInfo(data, snapshotCmdRand);
    const std::vector<std::string> snapshotCmdList = {
        "hitrace --start_bgsrv",
        "hitrace --dump_bgsrv",
        "hitrace --stop_bgsrv"
    };
    system(snapshotCmdList[snapshotCmdRand % snapshotCmdList.size()].c_str());
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
    OHOS::HiviewDFX::Hitrace::HitraceCommonTest(data, size);
    OHOS::HiviewDFX::Hitrace::HitraceRecordTest(data, size);
    OHOS::HiviewDFX::Hitrace::HitracesSnapshotTest(data, size);
    return 0;
}
