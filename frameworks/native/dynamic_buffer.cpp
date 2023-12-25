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

#include "dynamic_buffer.h"
#include "common_utils.h"

#include <fstream>
#include <cmath>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

namespace {
constexpr int EXPANSION_SIZE = 1024 * 6; // 6M
constexpr int LOW_THRESHOLD = 400 * 1024; // 400kb
constexpr int BASE_SIZE = 12 * 1024; // 12M
constexpr int EXPONENT = 2;
constexpr int PAGE_KB = 4;
} // namespace

bool DynamicBuffer::GetPerCpuStatsInfo(const size_t cpuIndex, TraceStatsInfo& traceStats)
{
    std::string statsPath = traceRootPath + "per_cpu/cpu" + std::to_string(cpuIndex) + "/stats";
    std::string standardizedPath = CanonicalizeSpecPath(statsPath.c_str());
    std::ifstream inFile;
    inFile.open(standardizedPath.c_str(), std::ios::in);
    if (!inFile.is_open()) {
        return false;
    }

    std::string line;
    const size_t oldTsPos = 17;
    const size_t nowTsPos = 8;
    const size_t bytesPos = 7;
    while (std::getline(inFile, line)) {
        if ((line.find("oldest event ts: ")) != std::string::npos) {
            traceStats.oldTs = std::stod(line.substr(oldTsPos));
        }
        if ((line.find("now ts: ")) != std::string::npos) {
            traceStats.nowTs = std::stod(line.substr(nowTsPos));
        }
        if ((line.find("bytes: ")) != std::string::npos) {
            traceStats.bytes = std::stod(line.substr(bytesPos));
        }
    }
    inFile.close();
    return true;
}

void DynamicBuffer::UpdateTraceLoad()
{
    if (!allTraceStats.empty()) {
        allTraceStats.clear();
    }
    totalCpusLoad = 0.0;
    totalAverage = 0;
    maxAverage = 0;
    for (int i = 0; i < cpuNums; i++) {
        TraceStatsInfo traceStats = {};
        if (!GetPerCpuStatsInfo(i, traceStats)) {
            HILOG_ERROR(LOG_CORE, "GetPerCpuStatsInfo failed.");
            return;
        }
        int duration = floor(traceStats.nowTs - traceStats.oldTs);
        if (duration == 0) {
            HILOG_ERROR(LOG_CORE, "nowTs:%{public}lf, oldTs:%{public}lf", traceStats.nowTs, traceStats.oldTs);
            return;
        }
        traceStats.averageTrace = traceStats.bytes / duration;
        totalAverage += traceStats.averageTrace;
        if (maxAverage < traceStats.averageTrace) {
            maxAverage = traceStats.averageTrace;
        }
        traceStats.freq = pow(traceStats.averageTrace, EXPONENT);
        totalCpusLoad += traceStats.freq;
        allTraceStats.push_back(traceStats);
    }
}

void DynamicBuffer::CalculateBufferSize(std::vector<int>& result)
{
    UpdateTraceLoad();
    if (static_cast<int>(allTraceStats.size()) != cpuNums) {
        return;
    }
    HILOG_DEBUG(LOG_CORE, "hitrace: average = %{public}d.", totalAverage / cpuNums);

    int totalBonus = 0;
    if (maxAverage > LOW_THRESHOLD) {
        totalBonus = EXPANSION_SIZE * cpuNums;
    }

    for (int i = 0; i < cpuNums; i++) {
        int newSize = BASE_SIZE + floor((allTraceStats[i].freq / totalCpusLoad) * totalBonus);
        newSize = newSize / PAGE_KB * PAGE_KB;
        result.push_back(newSize);
    }
}

} // Hitrace
} // HiviewDFX
} // OHOS