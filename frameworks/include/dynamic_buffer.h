/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_HIVIEWDFX_HITRACE_DYNAMIC_BUFFER_H
#define OHOS_HIVIEWDFX_HITRACE_DYNAMIC_BUFFER_H

#include <vector>
#include <string>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

struct TraceStatsInfo {
    double oldTs = 0;
    double nowTs = 0;
    int bytes = 0;
    int averageTrace = 0;
    double freq = 0;
};

class DynamicBuffer {
public:

    /**
     * traceRootPath, eg: /sys/kernel/debug/tracing/;
     * cpuNums, Number of CPU cores.
    */
    DynamicBuffer(std::string& traceRootPath, int cpuNums)
        : traceRootPath(traceRootPath), cpuNums(cpuNums) {}

    /**
     * Evaluate the set values of each buffer based on the load balancing algorithm,
     * and the results are stored.
    */
    void CalculateBufferSize(std::vector<int>& result);

private:
    bool GetPerCpuStatsInfo(const size_t cpuIndex, TraceStatsInfo& traceStats);
    void UpdateTraceLoad();

    std::vector<TraceStatsInfo> allTraceStats;
    std::string traceRootPath;
    int cpuNums = 0;
    double totalCpusLoad = 0.0;
    int totalAverage = 0;
    int maxAverage = 0;
};


} // Hitrace
} // HiviewDFX
} // OHOS

#endif // OHOS_HIVIEWDFX_HITRACE_DYNAMIC_BUFFER_H