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

#ifndef HITRACE_DUMP_STATE_H
#define HITRACE_DUMP_STATE_H

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "singleton.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
enum class DumpState {
    IDLE,
    RUNNING,
    INTERRUPT,
    STOPPING
};

class TraceDumpState : public Singleton<TraceDumpState> {
    DECLARE_SINGLETON(TraceDumpState);

public:
    bool StartLoopDump();
    void EndLoopDumpSelf();
    void EndLoopDump();
    bool IsLoopDumpRunning() const;

    bool InterruptCache();
    bool ContinueCache();
    bool IsInterruptCache() const;

    void EndAsyncReadWrite();
    bool IsAsyncReadContinue() const;
    bool IsAsyncWriteContinue() const;

private:
    std::atomic<DumpState> state_{DumpState::IDLE};
    mutable std::mutex conditionMutex_;
    std::condition_variable stateCondition_;

    std::atomic<bool> asyncReadFlag_{true};
    std::atomic<bool> asyncWriteFlag_{true};
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // HITRACE_DUMP_STATE_H