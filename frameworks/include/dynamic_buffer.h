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
};

class DynamicBuffer {
public:

    /**
     * traceRootPath, eg: /sys/kernel/debug/tracing/;
     * cpuNums, Number of CPU cores.
    */
    DynamicBuffer(const std::string& traceRootPath, int cpuNums)
        : traceRootPath_(traceRootPath), cpuNums_(cpuNums) {}

    /**
     * Evaluate the set values of each buffer based on the load balancing algorithm,
     * and the results are stored.
    */
    std::vector<int> CalculateBufferSize();
private:
    bool GetPerCpuStatsInfo(const size_t cpuIndex, TraceStatsInfo& traceStats);
    std::vector<double> CalculateTraceLoad();
    std::string traceRootPath_;
    int cpuNums_ = 0;
    double totalCpusLoad_ = 0.0;
    int maxAverage_ = 0;
};
} // Hitrace
} // HiviewDFX
} // OHOS

#endif // OHOS_HIVIEWDFX_HITRACE_DYNAMIC_BUFFER_H