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

#ifndef HITRACEEVENT_LISTENER_H
#define HITRACEEVENT_LISTENER_H

#include "hisysevent_listener.h"

namespace OHOS {
namespace HiviewDFX {

struct TraceSysEventParams {
    std::string opt;
    std::string caller;
    std::string tags;
    std::string clockType;
    int errorCode;
    std::string errorMessage;
};

class HitraceEventListener : public HiSysEventListener {
public:
    explicit HitraceEventListener() : HiSysEventListener() {}
    virtual ~HitraceEventListener() {}

public:
    void SetEvent(const TraceSysEventParams& traceEventParams);
    bool CheckKeywordInReasons();
    bool IsTraceSysEventParamsEqual(const TraceSysEventParams& traceEventParams);
    void OnEvent(std::shared_ptr<HiSysEventRecord> sysEvent);
    void OnServiceDied();

private:
    TraceSysEventParams traceSysEventParams;
    bool allFindFlag = false;
    std::mutex setFlagMutex;
    std::condition_variable keyWordCheckCondition;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
