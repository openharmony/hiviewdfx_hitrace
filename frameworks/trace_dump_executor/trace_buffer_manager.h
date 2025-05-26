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

#ifndef TRACE_BUFFER_MANAGER_H
#define TRACE_BUFFER_MANAGER_H

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
class TraceBufferManager {
public:
    using task_id_type = uint64_t; // 可根据实际情况调整任务ID类型

    // 构造函数：初始化总内存上限和块大小（默认50MB）
    explicit TraceBufferManager(size_t maxTotalSz, size_t blockSz = 50 * 1024 * 1024)
        : maxTotalSz_(maxTotalSz),
          blockSz_(blockSz),
          curTotalSz_(0) {}

    // 为指定任务分配一个内存块（线程安全）
    bool AllocateBlock(task_id_type taskId);
    // 释放指定任务的所有内存块（线程安全）
    void ReleaseTaskBlocks(task_id_type taskId);
    // 获取指定任务的内存块列表（线程安全）
    std::vector<std::pair<uint8_t*, size_t>> GetTaskBuffers(task_id_type taskId);
    // 获取当前已使用内存总量（线程安全）
    size_t GetCurrentTotalSize() const;

private:
    size_t maxTotalSz_;  // 总内存上限
    size_t blockSz_;      // 每个内存块的大小
    size_t curTotalSz_; // 当前已分配内存总量

    std::mutex mutex_;
    std::unordered_map<task_id_type, std::vector<std::unique_ptr<uint8_t[]>>> taskBuffers_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_BUFFER_MANAGER_H