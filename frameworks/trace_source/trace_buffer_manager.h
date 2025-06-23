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
    size_t FreeBytes() const;
    bool Append(const uint8_t* src, size_t size);
};

using BufferBlockPtr = std::shared_ptr<BufferBlock>;
using BufferList = std::list<BufferBlockPtr>;

constexpr size_t DEFAULT_MAX_TOTAL_SZ = 300 * 1024 * 1024; // 300 MB
constexpr size_t DEFAULT_BLOCK_SZ = 10 * 1024 * 1024; // 10 MB

class TraceBufferManager final {
public:
    static TraceBufferManager& GetInstance()
    {
        static TraceBufferManager instance(DEFAULT_MAX_TOTAL_SZ);
        return instance;
    }

    BufferBlockPtr AllocateBlock(const uint64_t taskId, const int cpu);
    void ReleaseTaskBlocks(const uint64_t taskId);
    BufferList GetTaskBuffers(const uint64_t taskId);
    size_t GetTaskTotalUsedBytes(const uint64_t taskId);
    size_t GetCurrentTotalSize();
    size_t GetBlockSize() const;

    TraceBufferManager(const TraceBufferManager&) = delete;
    TraceBufferManager(TraceBufferManager&&) = delete;
    TraceBufferManager& operator=(const TraceBufferManager&) = delete;
    TraceBufferManager& operator=(TraceBufferManager&&) = delete;

private:
    TraceBufferManager();
    explicit TraceBufferManager(size_t maxTotalSz, size_t blockSz = DEFAULT_BLOCK_SZ)
        : maxTotalSz_(maxTotalSz), blockSz_(blockSz), curTotalSz_(0) {}

private:
    size_t maxTotalSz_;
    size_t blockSz_;
    size_t curTotalSz_;

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, BufferList> taskBuffers_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_BUFFER_MANAGER_H