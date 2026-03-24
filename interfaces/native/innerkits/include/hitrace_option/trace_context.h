/*
* Copyright (C) 2026 Huawei Device Co., Ltd.
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

#ifndef HITRACE_TRACE_FILTER_CONTEXT_H
#define HITRACE_TRACE_FILTER_CONTEXT_H
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

class TraceFilterContext {
public:
    TraceFilterContext();
    ~TraceFilterContext();
    bool AddFilterPids(const std::vector<std::string>& filterPids);
    void FilterTraceContent();
    void TraverseSavedCmdLine(const std::function<void(const std::string& savedCmdLine)>& handler) const;
    void TraverseFilterPid(const std::function<void(const std::string& pid)>& handler) const;
    void TraverseTGidsContent(
        const std::function<void(const std::pair<std::string, std::string>& tgid)>& handler) const;
private:
    void FilterTGidsContent();
    void FilterSavedCmdLine();
    std::vector<std::string> filterCmdLines_;
    std::vector<std::pair<std::string, std::string>> filterTGidsContent_;
    std::set<std::string> filterPids_;
    pid_t initPid_ = -1;
    pid_t standInTid_ = -1;
    struct StandInThreadContext* standInThreadContext_;
};

class TraceContextManager {
public:
    static TraceContextManager& GetInstance();
    std::shared_ptr<TraceFilterContext> GetTraceFilterContext(bool createIfNotExisted = false);
    void ReleaseContext();
private:
    std::shared_ptr<TraceFilterContext> traceFilterContext_ = nullptr;
};
}
}
}
#endif //HITRACE_TRACE_FILTER_CONTEXT_H