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

#ifndef HITRACE_STOPPABLE_THREAD_HELPER_H
#define HITRACE_STOPPABLE_THREAD_HELPER_H
#include <condition_variable>
#include <functional>

namespace OHOS {
namespace HiviewDFX {
class StoppableThreadHelper {
public:
    StoppableThreadHelper() = default;
    ~StoppableThreadHelper();
    StoppableThreadHelper(const StoppableThreadHelper&) = delete;
    StoppableThreadHelper& operator=(const StoppableThreadHelper&) = delete;
    StoppableThreadHelper(StoppableThreadHelper&&) = delete;
    StoppableThreadHelper& operator=(StoppableThreadHelper&&) = delete;
    bool StopSubThread();
    bool StartSubThread(const std::function<bool()>& task, uint32_t intervalInSecond, const std::string& threadName);
private:
    bool isSubThreadRunning_{false};
    bool shouldSubThreadExit_{false};
    std::mutex mtx_;
    std::condition_variable cvTask_;
    std::condition_variable cvFinished_;
};
}
}

#endif //HITRACE_STOPPABLE_THREAD_HELPER_H