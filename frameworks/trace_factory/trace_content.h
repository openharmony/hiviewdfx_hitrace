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

#ifndef TRACE_CONTENT_H
#define TRACE_CONTENT_H

#include <cinttypes>
#include <string>

#include "hitrace_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
constexpr int ALIGNMENT_COEFFICIENT = 4;
constexpr uint16_t MAGIC_NUMBER = 57161;
constexpr uint8_t FILE_RAW_TRACE = 0;
constexpr uint16_t VERSION_NUMBER = 1;

struct alignas(ALIGNMENT_COEFFICIENT) TraceFileHeader {
    uint16_t magicNumber {MAGIC_NUMBER};
    uint8_t fileType {FILE_RAW_TRACE};
    uint16_t versionNumber {VERSION_NUMBER};
    uint32_t reserved {0};
};

enum ContentType : uint8_t {
    CONTENT_TYPE_DEFAULT = 0,
    CONTENT_TYPE_EVENTS_FORMAT = 1,
    CONTENT_TYPE_CMDLINES  = 2,
    CONTENT_TYPE_TGIDS = 3,
    CONTENT_TYPE_CPU_RAW = 4,
    CONTENT_TYPE_HEADER_PAGE = 30,
    CONTENT_TYPE_PRINTK_FORMATS = 31,
    CONTENT_TYPE_KALLSYMS = 32,
    CONTENT_TYPE_BASE_INFO = 33
};

struct alignas(ALIGNMENT_COEFFICIENT) TraceFileContentHeader {
    uint8_t type = CONTENT_TYPE_DEFAULT;
    uint32_t length = 0;
};

struct PageHeader {
    uint64_t timestamp = 0;
    uint64_t size = 0;
    uint8_t overwrite = 0;
    uint8_t *startPos = nullptr;
    uint8_t *endPos = nullptr;
};

class ITraceContent {
public:
    ITraceContent(const int fd, const std::string& tracefsPath, const std::string& traceFilePath, const bool ishm);
    virtual ~ITraceContent();
    virtual bool WriteTraceContent() = 0;
    bool WriteTraceData(const uint8_t contentType);
    void DoWriteTraceData(const uint8_t* buffer, const int bytes, ssize_t& writeLen);
    bool DoWriteTraceContentHeader(TraceFileContentHeader& contentHeader, const uint8_t contentType);
    void UpdateTraceContentHeader(TraceFileContentHeader& contentHeader, const uint32_t writeLen);
    bool IsFileExist();
    bool CheckPage(uint8_t* page);
    std::string GetTraceFilePath() const { return traceFilePath_; }
    static int GetCurrentFileSize();
    static void ResetCurrentFileSize();
    void WriteProcessLists(ssize_t& writeLen);

private:
    std::string ReadProcessName(const std::string& pid);
    bool AppendToBuffer(const std::string& data, int& bytes, ssize_t& writeLen);

protected:
    int traceFileFd_ = -1;
    int traceSourceFd_ = -1;
    std::string tracefsPath_;
    std::string traceFilePath_;
    bool isHm_;
};

class ITraceFileHdrContent : public ITraceContent {
public:
    ITraceFileHdrContent(const int fd,
        const std::string& tracefsPath, const std::string& traceFilePath, const bool ishm)
        : ITraceContent(fd, tracefsPath, traceFilePath, ishm) {}
    bool WriteTraceContent() override = 0;
    bool InitTraceFileHdr(TraceFileHeader& fileHdr);
};

class TraceFileHdrLinux : public ITraceFileHdrContent {
public:
    TraceFileHdrLinux(const int fd, const std::string& tracefsPath, const std::string& traceFilePath)
        : ITraceFileHdrContent(fd, tracefsPath, traceFilePath, false) {}
    bool WriteTraceContent() override;
};

class TraceBaseInfoContent : public ITraceContent {
public:
    TraceBaseInfoContent(const int fd,
        const std::string& tracefsPath, const std::string& traceFilePath, const bool ishm)
        : ITraceContent(fd, tracefsPath, traceFilePath, ishm) {}
    bool WriteTraceContent() override;

private:
    ssize_t WriteKeyValue(const std::string& key, const std::string& value);
    ssize_t WriteUnixTimeMs();
    ssize_t WriteBootTimeMs();
    ssize_t WriteKernelVersion();
};

class TraceFileHdrHM : public ITraceFileHdrContent {
public:
    TraceFileHdrHM(const int fd, const std::string& tracefsPath, const std::string& traceFilePath)
        : ITraceFileHdrContent(fd, tracefsPath, traceFilePath, true) {}
    bool WriteTraceContent() override;
};

class TraceEventFmtContent : public ITraceContent {
public:
    TraceEventFmtContent(const int fd, const std::string& tracefsPath, const std::string& traceFilePath,
        const bool ishm);
    bool WriteTraceContent() override;
};

class TraceCmdLinesContent : public ITraceContent {
public:
    TraceCmdLinesContent(const int fd, const std::string& tracefsPath, const std::string& traceFilePath,
        const bool ishm);
    bool WriteTraceContent() override;
};

class TraceTgidsContent : public ITraceContent {
public:
    TraceTgidsContent(const int fd, const std::string& tracefsPath, const std::string& traceFilePath,
        const bool ishm);
    bool WriteTraceContent() override;
};

class ITraceCpuRawContent : public ITraceContent {
public:
    ITraceCpuRawContent(const int fd, const std::string& tracefsPath, const std::string& traceFilePath,
        const bool ishm, const TraceDumpRequest& request)
        : ITraceContent(fd, tracefsPath, traceFilePath, ishm), request_(request) {}
    bool WriteTraceContent() override = 0;

    bool WriteTracePipeRawData(const std::string& srcPath, const int cpuIdx);
    void ReadTracePipeRawLoop(const int srcFd,
        int& bytes, bool& endFlag, int& pageChkFailedTime, bool& printFirstPageTime);
    bool IsWriteFileOverflow(const int outputFileSize, const ssize_t writeLen, const int fileSizeThreshold);

    TraceErrorCode GetDumpStatus() { return dumpStatus_; }
    uint64_t GetFirstPageTimeStamp() { return firstPageTimeStamp_; }
    uint64_t GetLastPageTimeStamp() { return lastPageTimeStamp_; }
    bool IsOverFlow();

protected:
    TraceDumpRequest request_;
    TraceErrorCode dumpStatus_ = TraceErrorCode::UNSET;
    uint64_t firstPageTimeStamp_ = std::numeric_limits<uint64_t>::max();
    uint64_t lastPageTimeStamp_ = 0;
    bool isOverFlow_ = false;
};

class TraceCpuRawLinux : public ITraceCpuRawContent {
public:
    TraceCpuRawLinux(const int fd,
        const std::string& tracefsPath, const std::string& traceFilePath, const TraceDumpRequest& request)
        : ITraceCpuRawContent(fd, tracefsPath, traceFilePath, false, request) {}
    bool WriteTraceContent() override;
};

class TraceCpuRawHM : public ITraceCpuRawContent {
public:
    TraceCpuRawHM(const int fd,
        const std::string& tracefsPath, const std::string& traceFilePath, const TraceDumpRequest& request)
        : ITraceCpuRawContent(fd, tracefsPath, traceFilePath, true, request) {}
    bool WriteTraceContent() override;
};

class ITraceHeaderPageContent : public ITraceContent {
public:
    ITraceHeaderPageContent(const int fd,
        const std::string& tracefsPath, const std::string& traceFilePath, const bool ishm)
        : ITraceContent(fd, tracefsPath, traceFilePath, ishm) {}
    bool WriteTraceContent() override = 0;
};

class TraceHeaderPageLinux : public ITraceHeaderPageContent {
public:
    TraceHeaderPageLinux(const int fd, const std::string& tracefsPath, const std::string& traceFilePath);
    bool WriteTraceContent() override;
};

class TraceHeaderPageHM : public ITraceHeaderPageContent {
public:
    TraceHeaderPageHM(const int fd, const std::string& tracefsPath, const std::string& traceFilePath)
        : ITraceHeaderPageContent(fd, tracefsPath, traceFilePath, true) {}
    bool WriteTraceContent() override;
};

class ITracePrintkFmtContent : public ITraceContent {
public:
    ITracePrintkFmtContent(const int fd,
        const std::string& tracefsPath, const std::string& traceFilePath, const bool ishm)
        : ITraceContent(fd, tracefsPath, traceFilePath, ishm) {}
    bool WriteTraceContent() override = 0;
};

class TracePrintkFmtLinux : public ITracePrintkFmtContent {
public:
    TracePrintkFmtLinux(const int fd, const std::string& tracefsPath, const std::string& traceFilePath);
    bool WriteTraceContent() override;
};

class TracePrintkFmtHM : public ITracePrintkFmtContent {
public:
    TracePrintkFmtHM(const int fd, const std::string& tracefsPath, const std::string& traceFilePath)
        : ITracePrintkFmtContent(fd, tracefsPath, traceFilePath, true) {}
    bool WriteTraceContent() override;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_CONTENT_H