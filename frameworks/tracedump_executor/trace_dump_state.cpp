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

#include "trace_dump_state.h"

#include <chrono>
#include <thread>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
constexpr int WAIT_TIMEOUT_MS = 5000;
}

TraceDumpState::TraceDumpState() {}

TraceDumpState::~TraceDumpState() {}

bool TraceDumpState::StartLoopDump()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (state_.load() != DumpState::IDLE) {
        return false;
    }

    state_.store(DumpState::RUNNING);
    return true;
}

void TraceDumpState::EndLoopDumpSelf()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.store(DumpState::IDLE);
    }
    stateCondition_.notify_all();
}

void TraceDumpState::EndLoopDump()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_.load() == DumpState::RUNNING) {
            state_.store(DumpState::STOPPING);
        }
    }
    std::unique_lock<std::mutex> lock(stateMutex_);
    stateCondition_.wait_for(lock, std::chrono::milliseconds(WAIT_TIMEOUT_MS),
        [this] { return state_.load() == DumpState::IDLE; });
}

bool TraceDumpState::IsLoopDumpRunning() const
{
    return state_.load() == DumpState::RUNNING;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS