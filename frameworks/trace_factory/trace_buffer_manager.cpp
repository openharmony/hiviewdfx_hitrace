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

#include "trace_buffer_manager.h"

#include <cinttypes>
#include <memory>
#include <mutex>

#include "hilog/log.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceBufferManager"
#endif
} // namespace

size_t BufferBlock::FreeBytes() const
{
    return data.size() - usedBytes;
}

bool BufferBlock::Append(const uint8_t* src, size_t size)
{
    if (FreeBytes() < size) {
        HILOG_ERROR(LOG_CORE, "Append : cannot append more data");
        return false;
    }
    if (memcpy_s(data.data() + usedBytes, FreeBytes(), src, size) != EOK) {
        HILOG_ERROR(LOG_CORE, "Append : memcpy_s failed");
        return false;
    }
    usedBytes += size;
    return true;
}

TraceBufferManager::TraceBufferManager()
{
    maxTotalSz_ = DEFAULT_MAX_TOTAL_SZ;
    blockSz_ = DEFAULT_BLOCK_SZ;
    curTotalSz_.store(0, std::memory_order_relaxed);
}

TraceBufferManager::~TraceBufferManager() {}

bool TraceBufferManager::TryAllocateMemorySpace(uint64_t taskId)
{
    size_t cur = curTotalSz_.load(std::memory_order_relaxed);
    size_t desiredSz = cur + blockSz_;

    if (desiredSz > maxTotalSz_) {
        HILOG_ERROR(LOG_CORE, "TryAllocateMemorySpace : taskid(%{public}" PRIu64 ") cannot allocate more blocks",
            taskId);
        return false;
    }

    while (!curTotalSz_.compare_exchange_weak(cur, desiredSz, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        desiredSz = cur + blockSz_;
        if (desiredSz > maxTotalSz_) {
            HILOG_ERROR(LOG_CORE, "TryAllocateMemorySpace : taskid(%{public}" PRIu64 ") cannot allocate more blocks",
                taskId);
            return false;
        }
    }

    return true;
}

BufferBlockPtr TraceBufferManager::AllocateBlock(const uint64_t taskId, const int cpu)
{
    if (curTotalSz_.load(std::memory_order_relaxed) + blockSz_ > maxTotalSz_) {
        HILOG_ERROR(LOG_CORE, "AllocateBlock : taskid(%{public}" PRIu64 ") cannot allocate more blocks", taskId);
        return nullptr;
    }
    auto buffer = std::make_shared<BufferBlock>(cpu, blockSz_);
    if (!TryAllocateMemorySpace(taskId)) {
        return nullptr;
    }
    {
        std::unique_lock<std::shared_mutex> globalWriteLock(globalMutex_);
        taskBuffers_[taskId].push_back(buffer);
    }
    return buffer;
}

void TraceBufferManager::ReleaseTaskBlocks(const uint64_t taskId)
{
    std::unique_lock<std::shared_mutex> globalWriteLock(globalMutex_);
    if (auto it = taskBuffers_.find(taskId); it != taskBuffers_.end()) {
        size_t released = it->second.size() * blockSz_;
        taskBuffers_.erase(it);
        globalWriteLock.unlock();
        curTotalSz_.fetch_sub(released, std::memory_order_relaxed);
    }
}

BufferList TraceBufferManager::GetTaskBuffers(const uint64_t taskId)
{
    std::shared_lock<std::shared_mutex> globalReadLock(globalMutex_);
    if (auto it = taskBuffers_.find(taskId); it != taskBuffers_.end()) {
        return it->second;
    }
    HILOG_WARN(LOG_CORE, "GetTaskBuffers : taskid(%{public}" PRIu64 ") not found.", taskId);
    return {};
}

size_t TraceBufferManager::GetTaskTotalUsedBytes(const uint64_t taskId)
{
    size_t totalUsed = 0;
    std::shared_lock<std::shared_mutex> globalReadLock(globalMutex_);
    if (auto it = taskBuffers_.find(taskId); it != taskBuffers_.end()) {
        for (auto& bufBlock : it->second) {
            totalUsed += bufBlock->usedBytes;
        }
    }
    return totalUsed;
}

size_t TraceBufferManager::GetCurrentTotalSize()
{
    std::shared_lock<std::shared_mutex> globalReadLock(globalMutex_);
    return curTotalSz_.load(std::memory_order_relaxed);
}

size_t TraceBufferManager::GetBlockSize() const
{
    return blockSz_;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS