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

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

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
} // namespace

HitraceDumpPipe::HitraceDumpPipe(bool isParent)
{
    isParent_ = isParent;
    if (isParent_) {
        taskSubmitFd_ = open(TRACE_TASK_SUBMIT_PIPE, O_WRONLY);
        if (taskSubmitFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: parent open %{public}s failed, errno(%{public}d)",
                TRACE_TASK_SUBMIT_PIPE, errno);
        }
        syncRetFd_ = open(TRACE_SYNC_RETURN_PIPE, O_RDONLY | O_NONBLOCK);
        if (syncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: parent open %{public}s failed, errno(%{public}d)",
                TRACE_SYNC_RETURN_PIPE, errno);
        }
        asyncRetFd_ = open(TRACE_ASYNC_RETURN_PIPE, O_RDONLY | O_NONBLOCK);
        if (asyncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: parent open %{public}s failed, errno(%{public}d)",
                TRACE_TASK_SUBMIT_PIPE, errno);
        }
    } else {
        taskSubmitFd_ = open(TRACE_TASK_SUBMIT_PIPE, O_RDONLY | O_NONBLOCK);
        if (taskSubmitFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: child open %{public}s failed, errno(%{public}d)",
                TRACE_TASK_SUBMIT_PIPE, errno);
        }
        syncRetFd_ = open(TRACE_SYNC_RETURN_PIPE, O_WRONLY);
        if (syncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: child open %{public}s failed, errno(%{public}d)",
                TRACE_SYNC_RETURN_PIPE, errno);
        }
        asyncRetFd_ = open(TRACE_ASYNC_RETURN_PIPE, O_WRONLY);
        if (asyncRetFd_ < 0) {
            HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: child open %{public}s failed, errno(%{public}d)",
                TRACE_TASK_SUBMIT_PIPE, errno);
        }
    }
}

HitraceDumpPipe::~HitraceDumpPipe()
{
    if (taskSubmitFd_ != -1) {
        close(taskSubmitFd_);
    }
    if (syncRetFd_ != -1) {
        close(syncRetFd_);
    }
    if (asyncRetFd_ != -1) {
        close(asyncRetFd_);
    }
}

bool HitraceDumpPipe::InitTraceDumpPipe()
{
    if (mkfifo(TRACE_TASK_SUBMIT_PIPE, 0666) < 0) {
        HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: create %{public}s failed, errno(%{public}d)",
            TRACE_TASK_SUBMIT_PIPE, errno);
        return false;
    }
    if (mkfifo(TRACE_SYNC_RETURN_PIPE, 0666) < 0) {
        HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: create %{public}s failed, errno(%{public}d)",
            TRACE_SYNC_RETURN_PIPE, errno);
        return false;
    }
    if (mkfifo(TRACE_ASYNC_RETURN_PIPE, 0666) < 0) {
        HILOG_ERROR(LOG_CORE, "HitraceDumpPipe: create %{public}s failed, errno(%{public}d)",
            TRACE_ASYNC_RETURN_PIPE, errno);
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

bool HitraceDumpPipe::SubmitTraceDumpTask(const TraceDumpTask& task)
{
    if (!isParent_) {
        HILOG_ERROR(LOG_CORE, "SubmitTraceDumpTask: child process can not submit trace dump task.");
        return false;
    }
    if (taskSubmitFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "SubmitTraceDumpTask: submit pipe fd is illegel.");
        return false;
    }
    ssize_t ret = write(taskSubmitFd_, &task, sizeof(task));
    if (ret < 0) {
        HILOG_ERROR(LOG_CORE, "SubmitTraceDumpTask: write pipe failed.");
        return false;
    }
    HILOG_INFO(LOG_CORE, "SubmitTraceDumpTask: task submitted, task id: %{public}llu.", task.time);
    return true;
}

bool HitraceDumpPipe::ReadSyncDumpRet(const int timeout, TraceDumpTask& task)
{
    if (!isParent_) {
        HILOG_ERROR(LOG_CORE, "ReadSyncDumpRet: child process can not read sync pipe return.");
        return false;
    }
    if (syncRetFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "ReadSyncDumpRet: sync return pipe fd is illegel.");
        return false;
    }

    int msCnt = 0;
    while (msCnt <= timeout * 1000) {
        ssize_t readSize = read(syncRetFd_, &task, sizeof(task));
        if (readSize > 0) {
            HILOG_INFO(LOG_CORE, "ReadSyncDumpRet: read task done, task id: %{public}llu.", task.time);
            return true;
        }
        usleep(50 * 1000); // 50 ms
        msCnt += 50;
    }

    HILOG_INFO(LOG_CORE, "ReadSyncDumpRet: read task timeout.");
    return false;
}

bool HitraceDumpPipe::ReadAsyncDumpRet(const int timeout, TraceDumpTask& task)
{
    if (!isParent_) {
        HILOG_ERROR(LOG_CORE, "ReadSyncDumpRet: child process can not read async pipe return.");
        return false;
    }
    if (asyncRetFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "ReadSyncDumpRet: async return pipe fd is illegel.");
        return false;
    }
    int msCnt = 0;
    while (msCnt <= timeout * 1000) {
        ssize_t readSize = read(asyncRetFd_, &task, sizeof(task));
        if (readSize > 0) {
            HILOG_INFO(LOG_CORE, "ReadAsyncDumpRet: read task done, task id: %{public}llu.", task.time);
            return true;
        }
        usleep(50 * 1000); // 50 ms
        msCnt += 50;
    }
    HILOG_INFO(LOG_CORE, "ReadAsyncDumpRet: read task timeout.");
    return false;
}

bool HitraceDumpPipe::ReadTraceTask(const int timeoutMs, TraceDumpTask& task)
{
    if (isParent_) {
        HILOG_ERROR(LOG_CORE, "ReadTraceTask: parent process can not read submit pipe.");
        return false;
    }
    if (taskSubmitFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "ReadTraceTask: submit pipe fd is illegel.");
        return false;
    }
    int msCnt = 0;
    while (msCnt <= timeoutMs) {
        ssize_t readSize = read(taskSubmitFd_, &task, sizeof(task));
        if (readSize > 0) {
            HILOG_INFO(LOG_CORE, "ReadTraceTask: read task done, task id: %{public}llu.", task.time);
            return true;
        }
        usleep(50 * 1000); // 50 ms
        msCnt += 50;
    }
    HILOG_INFO(LOG_CORE, "ReadTraceTask: read task timeout.");
    return false;
}

bool HitraceDumpPipe::WriteSyncReturn(TraceDumpTask& task)
{
    if (isParent_) {
        HILOG_ERROR(LOG_CORE, "WriteSyncReturn: parent process can not write sync return pipe.");
        return false;
    }
    if (syncRetFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "WriteSyncReturn: sync return pipe fd is illegel.");
        return false;
    }
    ssize_t ret = write(syncRetFd_, &task, sizeof(task));
    if (ret < 0) {
        HILOG_ERROR(LOG_CORE, "WriteSyncReturn: write pipe failed.");
        return false;
    }
    HILOG_INFO(LOG_CORE, "WriteSyncReturn: task submitted, task id: %{public}llu.", task.time);
    return true;
}

bool HitraceDumpPipe::WriteAsyncReturn(TraceDumpTask& task)
{
    if (isParent_) {
        HILOG_ERROR(LOG_CORE, "WriteAsyncReturn: parent process can not write async return pipe.");
        return false;
    }
    if (asyncRetFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "WriteAsyncReturn: async return pipe fd is illegel.");
        return false;
    }
    ssize_t ret = write(asyncRetFd_, &task, sizeof(task));
    if (ret < 0) {
        HILOG_ERROR(LOG_CORE, "WriteAsyncReturn: write pipe failed.");
        return false;
    }
    HILOG_INFO(LOG_CORE, "WriteAsyncReturn: task submitted, task id: %{public}llu.", task.time);
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS