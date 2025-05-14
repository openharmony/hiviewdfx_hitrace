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

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
// 为指定任务分配一个内存块（线程安全）
bool TraceBufferManager::AllocateBlock(task_id_type taskId)
{
    std::lock_guard<std::mutex> lck(mutex_);

    // 检查剩余内存是否足够
    if (curTotalSz_ + blockSz_ > maxTotalSz_) {
        return false;
    }

    auto& blocks = taskBuffers_[taskId];
    blocks.push_back(std::make_unique<uint8_t[]>(blockSz_));
    curTotalSz_ += blockSz_;
    return true;
}

// 释放指定任务的所有内存块（线程安全）
void TraceBufferManager::ReleaseTaskBlocks(task_id_type taskId)
{
    std::lock_guard<std::mutex> lck(mutex_);
    auto it = taskBuffers_.find(taskId);
    if (it != taskBuffers_.end()) {
        curTotalSz_ -= it->second.size() * blockSz_;
        taskBuffers_.erase(it);
    }
}

// 获取指定任务的内存块列表（线程安全）
std::vector<std::pair<uint8_t*, size_t>> TraceBufferManager::GetTaskBuffers(task_id_type taskId)
{
    std::lock_guard<std::mutex> lck(mutex_);
    std::vector<std::pair<uint8_t*, size_t>> result;

    auto it = taskBuffers_.find(taskId);
    if (it != taskBuffers_.end()) {
        for (auto& block : it->second) {
            result.emplace_back(block.get(), blockSz_);
        }
    }
    return result;
}

// 获取当前已使用内存总量（线程安全）
size_t TraceBufferManager::GetCurrentTotalSize() const
{
    std::lock_guard<std::mutex> lck(mutex_);
    return curTotalSz_;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS