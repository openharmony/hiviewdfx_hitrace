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

#include "trace_dump_pipe.h"

#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common_define.h"
#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceDumpPipe"
#endif
namespace {
const char TRACE_TASK_SUBMIT_PIPE[] = "/data/log/hitrace/trace_task";
const char TRACE_SYNC_RETURN_PIPE[] = "/data/log/hitrace/trace_sync_return";
const char TRACE_ASYNC_RETURN_PIPE[] = "/data/log/hitrace/trace_async_return";
const int READ_PIPE_TICK = 50;
const mode_t PIPE_FILE_MODE = 0666;
} // namespace

HitraceDumpPipe::HitraceDumpPipe(bool isParent)
{
    isParent_ = isParent;
    if (isParent_) {
        taskSubmitFd_ = UniqueFd(open(TRACE_TASK_SUBMIT_PIPE, O_WRONLY));
        if (taskSubmitFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: parent open %{public}s failed, errno(%{public}d)",
                TRACE_TASK_SUBMIT_PIPE, errno);
        }
        syncRetFd_ = UniqueFd(open(TRACE_SYNC_RETURN_PIPE, O_RDONLY | O_NONBLOCK));
        if (syncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: parent open %{public}s failed, errno(%{public}d)",
                TRACE_SYNC_RETURN_PIPE, errno);
        }
        asyncRetFd_ = UniqueFd(open(TRACE_ASYNC_RETURN_PIPE, O_RDONLY | O_NONBLOCK));
        if (asyncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: parent open %{public}s failed, errno(%{public}d)",
                TRACE_ASYNC_RETURN_PIPE, errno);
        }
    } else {
        taskSubmitFd_ = UniqueFd(open(TRACE_TASK_SUBMIT_PIPE, O_RDONLY | O_NONBLOCK));
        if (taskSubmitFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: child open %{public}s failed, errno(%{public}d)",
                TRACE_TASK_SUBMIT_PIPE, errno);
        }
        syncRetFd_ = UniqueFd(open(TRACE_SYNC_RETURN_PIPE, O_WRONLY));
        if (syncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: child open %{public}s failed, errno(%{public}d)",
                TRACE_SYNC_RETURN_PIPE, errno);
        }
        asyncRetFd_ = UniqueFd(open(TRACE_ASYNC_RETURN_PIPE, O_WRONLY));
        if (asyncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: child open %{public}s failed, errno(%{public}d)",
                TRACE_ASYNC_RETURN_PIPE, errno);
        }
    }
}

bool HitraceDumpPipe::InitTraceDumpPipe()
{
    if (mkfifo(TRACE_TASK_SUBMIT_PIPE, PIPE_FILE_MODE) < 0) {
        HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: create %{public}s failed, errno(%{public}d)",
            TRACE_TASK_SUBMIT_PIPE, errno);
        return false;
    }
    if (mkfifo(TRACE_SYNC_RETURN_PIPE, PIPE_FILE_MODE) < 0) {
        HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: create %{public}s failed, errno(%{public}d)",
            TRACE_SYNC_RETURN_PIPE, errno);
        unlink(TRACE_TASK_SUBMIT_PIPE);
        return false;
    }
    if (mkfifo(TRACE_ASYNC_RETURN_PIPE, PIPE_FILE_MODE) < 0) {
        HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: create %{public}s failed, errno(%{public}d)",
            TRACE_ASYNC_RETURN_PIPE, errno);
        unlink(TRACE_TASK_SUBMIT_PIPE);
        unlink(TRACE_SYNC_RETURN_PIPE);
        return false;
    }
    return true;
}

void HitraceDumpPipe::ClearTraceDumpPipe()
{
    unlink(TRACE_TASK_SUBMIT_PIPE);
    unlink(TRACE_SYNC_RETURN_PIPE);
    unlink(TRACE_ASYNC_RETURN_PIPE);
}

bool HitraceDumpPipe::CheckProcessRole(bool shouldBeParent, const char* operation) const
{
    if (isParent_ != shouldBeParent) {
        HILOG_ERROR(LOG_CORE, "%{public}s: %{public}s process can not perform this operation.",
            operation, shouldBeParent ? "child" : "parent");
        return false;
    }
    return true;
}

bool HitraceDumpPipe::CheckFdValidity(const int fd, const char* operation, const char* pipeName) const
{
    if (fd < 0) {
        HILOG_ERROR(LOG_CORE, "%{public}s: %{public}s fd is illegal.", operation, pipeName);
        return false;
    }
    return true;
}

bool HitraceDumpPipe::WriteToPipe(const int fd, const TraceDumpTask& task, const char* operation)
{
    ssize_t ret = TEMP_FAILURE_RETRY(write(fd, &task, sizeof(task)));
    if (ret < 0) {
        HILOG_ERROR(LOG_CORE, "%{public}s: write pipe failed.", operation);
        return false;
    }
    HILOG_INFO(LOG_CORE, "%{public}s: task submitted, task id: %{public}" PRIu64 ".", operation, task.time);
    return true;
}

bool HitraceDumpPipe::ReadFromPipe(const int fd, TraceDumpTask& task, const int timeoutMs, const char* operation)
{
    fd_set readfds;
    struct timeval tv;
    int ret;
    const int waitStepMs = READ_PIPE_TICK;
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        int remain = timeoutMs - elapsed;
        if (remain <= 0) {
            break;
        }
        int waitMs = (remain < waitStepMs) ? remain : waitStepMs;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        tv.tv_sec = waitMs / S_TO_MS;
        tv.tv_usec = (waitMs % S_TO_MS) * S_TO_MS;
        ret = select(fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret > 0 && FD_ISSET(fd, &readfds)) {
            ssize_t readSize = TEMP_FAILURE_RETRY(read(fd, &task, sizeof(task)));
            if (readSize > 0) {
                HILOG_INFO(LOG_CORE, "%{public}s: read task done, task id: %{public}" PRIu64 ".", operation, task.time);
                return true;
            }
        } else if (ret < 0) {
            HILOG_ERROR(LOG_CORE, "%{public}s: select error, errno: %{public}d", operation, errno);
            return false;
        }
    }
    HILOG_INFO(LOG_CORE, "%{public}s: read task timeout.", operation);
    return false;
}

bool HitraceDumpPipe::SubmitTraceDumpTask(const TraceDumpTask& task)
{
    const char* operation = "SubmitTraceDumpTask";
    if (!CheckProcessRole(true, operation) || !CheckFdValidity(taskSubmitFd_, operation, "submit pipe")) {
        return false;
    }
    return WriteToPipe(taskSubmitFd_, task, operation);
}

bool HitraceDumpPipe::ReadSyncDumpRet(const int timeout, TraceDumpTask& task)
{
    const char* operation = "ReadSyncDumpRet";
    if (!CheckProcessRole(true, operation) || !CheckFdValidity(syncRetFd_, operation, "sync return pipe")) {
        return false;
    }
    return ReadFromPipe(syncRetFd_, task, timeout * S_TO_MS, operation);
}

bool HitraceDumpPipe::ReadAsyncDumpRet(const int timeout, TraceDumpTask& task)
{
    const char* operation = "ReadAsyncDumpRet";
    if (!CheckProcessRole(true, operation) || !CheckFdValidity(asyncRetFd_, operation, "async return pipe")) {
        return false;
    }
    return ReadFromPipe(asyncRetFd_, task, timeout * S_TO_MS, operation);
}

bool HitraceDumpPipe::ReadTraceTask(const int timeoutMs, TraceDumpTask& task)
{
    const char* operation = "ReadTraceTask";
    if (!CheckProcessRole(false, operation) || !CheckFdValidity(taskSubmitFd_, operation, "submit pipe")) {
        return false;
    }
    return ReadFromPipe(taskSubmitFd_, task, timeoutMs, operation);
}

bool HitraceDumpPipe::WriteSyncReturn(const TraceDumpTask& task)
{
    const char* operation = "WriteSyncReturn";
    if (!CheckProcessRole(false, operation) || !CheckFdValidity(syncRetFd_, operation, "sync return pipe")) {
        return false;
    }
    return WriteToPipe(syncRetFd_, task, operation);
}

bool HitraceDumpPipe::WriteAsyncReturn(const TraceDumpTask& task)
{
    const char* operation = "WriteAsyncReturn";
    if (!CheckProcessRole(false, operation) || !CheckFdValidity(asyncRetFd_, operation, "async return pipe")) {
        return false;
    }
    return WriteToPipe(asyncRetFd_, task, operation);
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS