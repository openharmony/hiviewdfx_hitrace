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

#include "hitrace_util.h"

#include <thread>
#include <sys/prctl.h>

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {

bool StoppableThreadHelper::StartSubThread(const std::function<bool()>& task, uint32_t intervalInSecond,
    const std::string& threadName)
{
    std::unique_lock<std::mutex> lock(mtx_);
    if (!task || isSubThreadRunning_) {
        return false;
    }
    shouldSubThreadExit_ = false;
    isSubThreadRunning_ = true;
    std::thread([task, intervalInSecond, threadName, this]() {
        HILOG_INFO(LOG_CORE, "StopAbleThreadUtil: %{public}s thread start.", threadName.c_str());
        prctl(PR_SET_NAME, threadName.c_str());
        while (task()) {
            std::unique_lock<std::mutex> lock(mtx_);
            auto shouldExited = cvTask_.wait_for(lock, std::chrono::seconds(intervalInSecond), [this] {
                return shouldSubThreadExit_;
            });
            if (shouldExited) {
                break;
            }
        }
        std::unique_lock<std::mutex> lock(mtx_);
        isSubThreadRunning_ = false;
        cvFinished_.notify_one();
        HILOG_INFO(LOG_CORE, "StopAbleThreadUtil: %{public}s thread exited.", threadName.c_str());
    }).detach();
    return true;
}

bool StoppableThreadHelper::StopSubThread()
{
    std::unique_lock<std::mutex> lock(mtx_);
    if (isSubThreadRunning_) {
        shouldSubThreadExit_ = true;
        cvTask_.notify_one();
        cvFinished_.wait(lock,
            [this] {
                return !isSubThreadRunning_;
            });
        return true;
    }
    return false;
}

StoppableThreadHelper::~StoppableThreadHelper()
{
    StopSubThread();
}
}
}