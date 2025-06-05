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

#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
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
        return false;
    }
    if (memcpy_s(data.data() + usedBytes, size, src, size) != EOK) {
        return false;
    }
    usedBytes += size;
    return true;
}

BufferBlockPtr TraceBufferManager::AllocateBlock(task_id_type taskId, int cpu)
{
    std::lock_guard<std::mutex> lck(mutex_);

    if (curTotalSz_ + blockSz_ > maxTotalSz_) {
        return nullptr;
    }

    auto buffer = std::make_shared<BufferBlock>(cpu, blockSz_);
    taskBuffers_[taskId].push_back(buffer);
    curTotalSz_ += blockSz_;
    return buffer;
}

void TraceBufferManager::ReleaseTaskBlocks(task_id_type taskId)
{
    std::lock_guard<std::mutex> lck(mutex_);
    if (auto it = taskBuffers_.find(taskId); it != taskBuffers_.end()) {
        size_t released = it->second.size() * blockSz_;
        curTotalSz_ -= released;

        taskBuffers_.erase(it);
    }
}

BufferList TraceBufferManager::GetTaskBuffers(task_id_type taskId)
{
    std::lock_guard<std::mutex> lck(mutex_);
    if (auto it = taskBuffers_.find(taskId); it != taskBuffers_.end()) {
        return it->second;
    }
    return {};
}

size_t TraceBufferManager::GetTaskTotalUsedBytes(task_id_type taskId)
{
    size_t totalUsed = 0;
    std::lock_guard<std::mutex> lck(mutex_);
    if (auto it = taskBuffers_.find(taskId); it != taskBuffers_.end()) {
        for (auto& bufBlock : it->second) {
            totalUsed += bufBlock->usedBytes;
        }
    }
    return totalUsed;
}

size_t TraceBufferManager::GetCurrentTotalSize()
{
    std::lock_guard<std::mutex> lck(mutex_);
    return curTotalSz_;
}

size_t TraceBufferManager::GetBlockSize() const
{
    return blockSz_;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS