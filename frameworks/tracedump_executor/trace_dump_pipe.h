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

#ifndef HITRACE_DUMP_PIPE_H
#define HITRACE_DUMP_PIPE_H

#include "smart_fd.h"
#include "hitrace_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
class HitraceDumpPipe {
public:
    HitraceDumpPipe() = delete;
    explicit HitraceDumpPipe(bool isParent);
    ~HitraceDumpPipe() = default;

    static bool InitTraceDumpPipe();
    static void ClearTraceDumpPipe();

    // parent side
    bool SubmitTraceDumpTask(TraceDumpTask& task);
    bool ReadSyncDumpRet(const int timeout, TraceDumpTask& task);
    bool ReadAsyncDumpRet(const int timeout, TraceDumpTask& task);

    // child side
    bool ReadTraceTask(const int timeoutMs, TraceDumpTask& task);
    bool WriteSyncReturn(TraceDumpTask& task);
    bool WriteAsyncReturn(TraceDumpTask& task);

private:
    void InitPipeFd();
    bool CheckProcessRole(bool shouldBeParent, const char* operation) const;
    bool CheckFdValidity(const int fd, const char* operation, const char* pipeName) const;
    bool WriteToPipe(const int fd, TraceDumpTask& task, const char* operation);
    bool ReadFromPipe(const int fd, TraceDumpTask& task, const int timeoutMs, const char* operation);
    bool AddFdToEpoll(const int fd);

    bool isParent_ = false;
    SmartFd taskSubmitFd_;
    SmartFd syncRetFd_;
    SmartFd asyncRetFd_;
    SmartFd epollFd_;
    bool epollInitialized_ = false;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif