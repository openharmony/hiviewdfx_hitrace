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

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "singleton.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
/**
 * @brief BufferBlock is a block of memory that can be used to store trace data.
 * @note The current trace collection service already ensures that memory reading and writing are serial,
 *       so there is no need to add lock protection.
 */
struct BufferBlock {
    // cpu index
    int cpu;
    // data buffer
    std::vector<uint8_t> data;
    // used bytes
    size_t usedBytes = 0;
    // constructor
    BufferBlock(int cpuIdx, size_t size) : cpu(cpuIdx), data(size) {}
    // free bytes
    size_t FreeBytes() const;
    // append data
    bool Append(const uint8_t* src, size_t size);
};

using BufferBlockPtr = std::shared_ptr<BufferBlock>;
using BufferList = std::list<BufferBlockPtr>;

constexpr size_t DEFAULT_BLOCK_SZ = 10 * 1024 * 1024; // 10 MB
constexpr size_t DEFAULT_MAX_TOTAL_SZ = 300 * 1024 * 1024; // 300 MB

class TraceBufferManager : public Singleton<TraceBufferManager> {
    DECLARE_SINGLETON(TraceBufferManager);
public:
    BufferBlockPtr AllocateBlock(const uint64_t taskId, const int cpu);
    void ReleaseTaskBlocks(const uint64_t taskId);
    BufferList GetTaskBuffers(const uint64_t taskId);
    size_t GetTaskTotalUsedBytes(const uint64_t taskId);
    size_t GetCurrentTotalSize();
    size_t GetBlockSize() const;

private:
    bool TryAllocateMemorySpace(uint64_t taskId);

private:
    size_t maxTotalSz_;
    size_t blockSz_;
    std::atomic_size_t curTotalSz_;
    mutable std::shared_mutex globalMutex_;
    std::map<uint64_t, BufferList> taskBuffers_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_BUFFER_MANAGER_H