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

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
struct BufferBlock {
    int cpu;
    std::vector<uint8_t> data;
    size_t usedBytes = 0;
    BufferBlock(int cpuIdx, size_t size) : cpu(cpuIdx), data(size) {}

    // 获取空闲空间
    size_t FreeBytes() const;

    // 追加数据到缓冲区
    bool Append(const uint8_t* src, size_t size);
};
using task_id_type = uint64_t; // 可根据实际情况调整任务ID类型
using BufferBlockPtr = std::shared_ptr<BufferBlock>;
using BufferList = std::list<BufferBlockPtr>;

class TraceBufferManager {
public:
    TraceBufferManager();
    // 构造函数：初始化总内存上限和块大小（默认10MB）
    TraceBufferManager(size_t maxTotalSz, size_t blockSz = 10 * 1024 * 1024)
        : maxTotalSz_(maxTotalSz),
          blockSz_(blockSz),
          curTotalSz_(0) {}

    static TraceBufferManager& GetInstance()
    {
        static TraceBufferManager instance(300 * 1024 * 1024); // 300 MB
        return instance;
    }

    // 为指定任务分配一个内存块（线程安全）
    BufferBlockPtr AllocateBlock(task_id_type taskId, int cpu);
    // 释放指定任务的所有内存块（线程安全）
    void ReleaseTaskBlocks(task_id_type taskId);
    // 获取指定任务的内存块列表（线程安全）
    BufferList GetTaskBuffers(task_id_type taskId);
    // 获取指定任务的总实际使用内存（线程安全）
    size_t GetTaskTotalUsedBytes(task_id_type taskId);
    // 获取当前已使用内存总量（线程安全）
    size_t GetCurrentTotalSize();

    size_t GetBlockSize() const;

private:
    size_t maxTotalSz_;  // 总内存上限
    size_t blockSz_;      // 每个内存块的大小
    size_t curTotalSz_; // 当前已分配内存总量

    mutable std::mutex mutex_;
    std::unordered_map<task_id_type, BufferList> taskBuffers_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_BUFFER_MANAGER_H