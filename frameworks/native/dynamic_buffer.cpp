/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include <fstream>
#include <cmath>

#include "common_define.h"
#include "common_utils.h"
#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceDynamicBuf"
#endif
namespace {
constexpr int EXPANSION_SIZE = 1024 * 6; // 6M
constexpr int LOW_THRESHOLD = 400 * 1024; // 400kb
constexpr int BASE_SIZE = 12 * 1024; // 12M
constexpr int EXPONENT = 2;
constexpr int PAGE_KB = 4;
} // namespace

bool DynamicBuffer::GetPerCpuStatsInfo(const size_t cpuIndex, TraceStatsInfo& traceStats)
{
    std::string statsPath = traceRootPath_ + "per_cpu/cpu" + std::to_string(cpuIndex) + "/stats";
    std::string standardizedPath = CanonicalizeSpecPath(statsPath.c_str());
    std::ifstream inFile;
    inFile.open(standardizedPath.c_str(), std::ios::in);
    if (!inFile.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(inFile, line)) {
        if (line.find("oldest event ts: ") != std::string::npos) {
            constexpr size_t oldTsPos = 17;
            if (!StringToDouble(line.substr(oldTsPos), traceStats.oldTs)) {
                inFile.close();
                return false;
            }
        }
        if (line.find("now ts: ") != std::string::npos) {
            constexpr size_t nowTsPos = 8;
            if (!StringToDouble(line.substr(nowTsPos), traceStats.nowTs)) {
                inFile.close();
                return false;
            }
        }
        if (line.find("bytes: ") != std::string::npos) {
            constexpr size_t bytesPos = 7;
            if (!StringToInt(line.substr(bytesPos), traceStats.bytes)) {
                inFile.close();
                return false;
            }
        }
    }
    inFile.close();
    return true;
}

std::vector<double> DynamicBuffer::CalculateTraceLoad()
{
    std::vector<double> traceLoad;
    int totalAverage = 0;
    for (int i = 0; i < cpuNums_; i++) {
        TraceStatsInfo traceStats = {};
        if (!GetPerCpuStatsInfo(i, traceStats)) {
            HILOG_ERROR(LOG_CORE, "GetPerCpuStatsInfo failed.");
            return traceLoad;
        }
        int duration = floor(traceStats.nowTs - traceStats.oldTs);
        if (duration == 0) {
            HILOG_ERROR(LOG_CORE, "nowTs:%{public}lf, oldTs:%{public}lf", traceStats.nowTs, traceStats.oldTs);
            traceLoad.push_back(0);
            continue;
        }
        auto averageTrace = traceStats.bytes / duration;
        totalAverage += averageTrace;
        if (maxAverage_ < averageTrace) {
            maxAverage_ = averageTrace;
        }
        auto freq = pow(averageTrace, EXPONENT);
        totalCpusLoad_ += freq;
        traceLoad.push_back(freq);
    }
    HILOG_DEBUG(LOG_CORE, "hitrace: average = %{public}d.", totalAverage / cpuNums_);
    return traceLoad;
}

std::vector<int> DynamicBuffer::CalculateBufferSize()
{
    std::vector<int> result;
    auto allCpuFrequencies = CalculateTraceLoad();
    if (static_cast<int>(allCpuFrequencies.size()) != cpuNums_) {
        return result;
    }
    if (maxAverage_ <= LOW_THRESHOLD) {
        result.insert(result.begin(), cpuNums_, BASE_SIZE);
        return result;
    }
    auto totalBonus =  EXPANSION_SIZE * cpuNums_;
    for (double cpuFrequency : allCpuFrequencies) {
        int newSize = BASE_SIZE + static_cast<int32_t>(floor((cpuFrequency / totalCpusLoad_) * totalBonus));
        newSize = newSize / PAGE_KB * PAGE_KB;
        result.push_back(newSize);
    }
    return result;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS