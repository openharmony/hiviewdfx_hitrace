/*
* Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SMART_FD_H
#define SMART_FD_H

#include <cstdint>
#include <cstdio>
#include <unistd.h>

namespace OHOS {
namespace HiviewDFX {
class SmartFd {
public:
    SmartFd() = default;

    explicit SmartFd(int fd) : fd_(fd)
    {
#ifndef is_host
        if (fd_ >= 0) {
            fdsan_exchange_owner_tag(fd_, 0, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, DFX_HITRACE_FDSAN_DOMAIN));
        }
#endif
    }

    ~SmartFd()
    {
        Reset();
    }

    SmartFd(const SmartFd&) = delete;

    SmartFd &operator=(const SmartFd&) = delete;

    SmartFd(SmartFd&& rhs) noexcept
    {
        fd_ = rhs.fd_;
        rhs.fd_ = -1;
    }

    SmartFd& operator=(SmartFd&& rhs) noexcept
    {
        if (this != &rhs) {
            Reset();
            fd_ = rhs.fd_;
            rhs.fd_ = -1;
        }
        return *this;
    }

    explicit operator bool() const
    {
        return fd_ >= 0;
    }

    int GetFd() const
    {
        return fd_;
    }

    void Reset()
    {
        if (fd_ < 0) {
            return;
        }
#ifndef is_host
        fdsan_close_with_tag(fd_, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, DFX_HITRACE_FDSAN_DOMAIN));
#else
        close(fd_);
#endif
        fd_ = -1;
    }

private:
    int fd_{-1};
#ifndef is_host
    static constexpr uint64_t DFX_HITRACE_FDSAN_DOMAIN = 0xD002D33;
#endif
};
}
}
#endif // SMART_FD_H
