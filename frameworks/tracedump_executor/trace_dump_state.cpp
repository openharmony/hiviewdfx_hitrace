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
    DumpState expected = DumpState::IDLE;
    return state_.compare_exchange_strong(expected, DumpState::RUNNING,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

void TraceDumpState::EndLoopDumpSelf()
{
    state_.store(DumpState::IDLE, std::memory_order_release);
    stateCondition_.notify_all();
}

void TraceDumpState::EndLoopDump()
{
    state_.store(DumpState::STOPPING, std::memory_order_release);
    std::unique_lock<std::mutex> lock(conditionMutex_);
    stateCondition_.wait_for(lock, std::chrono::milliseconds(WAIT_TIMEOUT_MS),
        [this] { return state_.load(std::memory_order_acquire) == DumpState::IDLE; });
}

bool TraceDumpState::IsLoopDumpRunning() const
{
    auto state = state_.load(std::memory_order_acquire);
    return state == DumpState::RUNNING || state == DumpState::INTERRUPT;
}

bool TraceDumpState::InterruptCache()
{
    DumpState expected = DumpState::RUNNING;
    return state_.compare_exchange_strong(expected, DumpState::INTERRUPT,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

bool TraceDumpState::ContinueCache()
{
    DumpState expected = DumpState::INTERRUPT;
    return state_.compare_exchange_strong(expected, DumpState::RUNNING,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

bool TraceDumpState::IsInterruptCache() const
{
    return state_.load(std::memory_order_acquire) == DumpState::INTERRUPT;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS