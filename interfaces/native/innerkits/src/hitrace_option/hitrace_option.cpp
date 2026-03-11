/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#include "hitrace_option.h"

#include <cstring>
#include <sstream>

#include "hitrace_util.h"
#include "hilog/log.h"
#include "parameters.h"
#include "hitrace_option_util.h"
#include "trace_context.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceOption"
#endif

constexpr auto TELEMETRY_APP_PARAM = "debug.hitrace.telemetry.app";
constexpr auto SET_NO_FILTER_EVENT = "no_filter_events";

int32_t SetFilterAppName(const std::vector<std::string>& apps)
{
    auto appsSize = apps.size();
    constexpr auto maxAppNameLen = 10;
    if (appsSize > maxAppNameLen) {
        return HITRACE_SET_PARAM_ERROR;
    }
    std::string appNames;
    for (size_t i = 0; i < appsSize; i++) {
        appNames.append(apps[i]);
        if (i != apps.size() - 1) {
            appNames.append("\t");
        }
    }
    bool ret = OHOS::system::SetParameter(TELEMETRY_APP_PARAM, appNames);
    HILOG_INFO(LOG_CORE, "SetTelemetryAppName %{public}s ret=%{public}d", appNames.c_str(), ret);
    return ret ? HITRACE_NO_ERROR : HITRACE_SET_PARAM_ERROR;
}

int32_t AddFilterPid(const pid_t pid)
{
    int32_t ret = HITRACE_WRITE_FILE_ERROR;
    auto filterContext = TraceContextManager::GetInstance().GetTraceFilterContext(true);
    if (filterContext && filterContext->AddFilterPids({ std::to_string(pid) })) {
        ret = HITRACE_NO_ERROR;
    }
    return ret;
}

void FilterAppTrace(const char* app, pid_t pid)
{
    if (app == nullptr) {
        HILOG_INFO(LOG_CORE, "FilterAppTrace: app is null");
        return;
    }
    HILOG_INFO(LOG_CORE, "FilterAppTrace %{public}s %{public}d", app, pid);
    std::string paramApp = OHOS::system::GetParameter(TELEMETRY_APP_PARAM, "");
    auto pos = paramApp.find(app);
    while (pos != std::string::npos) {
        size_t left = pos;
        size_t right = pos + strlen(app);
        if ((left > 0 && paramApp.at(left - 1) != '\t') ||
            (right < paramApp.size() && paramApp.at(right) != '\t')) {
            pos = paramApp.find(app, right);
            continue;
        }
        AddFilterPid(pid);
        return;
    }
}

int32_t AddNoFilterEvents(const std::vector<std::string>& events)
{
    std::stringstream ss(" ");
    for (const auto& event : events) {
        ss << event << " ";
    }
    return AppendTracePoint(SET_NO_FILTER_EVENT, ss.str()) ? HITRACE_NO_ERROR : HITRACE_WRITE_FILE_ERROR;
}

int32_t ClearNoFilterEvents()
{
    return ClearTracePoint(SET_NO_FILTER_EVENT) ? HITRACE_NO_ERROR : HITRACE_WRITE_FILE_ERROR;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
