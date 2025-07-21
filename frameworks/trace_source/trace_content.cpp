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

#include "trace_content.h"

#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <hilog/log.h>
#include <string>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "securec.h"
#include "trace_file_utils.h"
#include "trace_json_parser.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceSource"
#endif
namespace {
constexpr int KB_PER_MB = 1024;
constexpr int JUDGE_FILE_EXIST = 10;  // Check whether the trace file exists every 10 times.
constexpr int BUFFER_SIZE = 256 * PAGE_SIZE; // 1M
constexpr uint8_t HM_FILE_RAW_TRACE = 1;

static int g_writeFileLimit = 0;
static int g_outputFileSize = 0;

uint8_t g_buffer[BUFFER_SIZE] = { 0 };

static void PreWriteAllTraceEventsFormat(const int fd, const std::string& tracefsPath)
{
    const TraceJsonParser& TraceJsonParser = TraceJsonParser::Instance();
    const std::map<std::string, TraceTag>& allTags = TraceJsonParser.GetAllTagInfos();
    std::vector<std::string> traceFormats = TraceJsonParser.GetBaseFmtPath();
    for (auto& tag : allTags) {
        for (auto& fmt : tag.second.formatPath) {
            traceFormats.emplace_back(fmt);
        }
    }
    for (auto& traceFmt : traceFormats) {
        std::string srcPath = tracefsPath + traceFmt;
        if (access(srcPath.c_str(), R_OK) != -1) {
            WriteEventFile(srcPath, fd);
        }
    }
}

static int IsCurrentTracePageValid(const uint64_t pageTraceTime, const uint64_t traceStartTime,
    const uint64_t traceEndTime)
{
    if (pageTraceTime > traceEndTime) { // need stop read, but should keep current page.
        HILOG_INFO(LOG_CORE,
            "Current pageTraceTime(%{public}" PRIu64 ") is larger than traceEndTime(%{public}" PRIu64 ")",
            pageTraceTime, traceEndTime);
        return -1;
    }
    if (pageTraceTime < traceStartTime) { // skip current 4K data caused of target trace range is not reached.
        return 0; // just skip
    }
    return 1; // hit.
}
}

ITraceContent::ITraceContent(const int fd,
                             const std::string& tracefsPath,
                             const std::string& traceFilePath,
                             const bool ishm)
    : traceFileFd_(fd), tracefsPath_(tracefsPath), traceFilePath_(traceFilePath), isHm_(ishm)
{
    traceSourceFd_ = -1;
}

ITraceContent::~ITraceContent()
{
    if (traceSourceFd_ != -1) {
        close(traceSourceFd_);
    }
}

bool ITraceContent::WriteTraceData(const uint8_t contentType)
{
    if (traceSourceFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "WriteTraceData: trace source fd is illegal.");
        return false;
    }
    if (!IsFileExist()) {
        HILOG_ERROR(LOG_CORE, "WriteTraceData: trace file (%{public}s) not found.", traceFilePath_.c_str());
        return false;
    }
    TraceFileContentHeader contentHeader;
    if (!DoWriteTraceContentHeader(contentHeader, contentType)) {
        return false;
    }
    ssize_t writeLen = 0;
    int pageChkFailedTime = 0;
    while (true) {
        int bytes = 0;
        bool endFlag = false;
        /* Write 1M at a time */
        while (bytes <= (BUFFER_SIZE - static_cast<int>(PAGE_SIZE))) {
            ssize_t readBytes = TEMP_FAILURE_RETRY(read(traceSourceFd_, g_buffer + bytes, PAGE_SIZE));
            if (readBytes <= 0) {
                endFlag = true;
                HILOG_DEBUG(LOG_CORE, "WriteTraceData: read raw trace done, size(%{public}zd), err(%{public}s).",
                    readBytes, strerror(errno));
                break;
            }
            if (!CheckPage(g_buffer + bytes)) {
                pageChkFailedTime++;
            }
            bytes += readBytes;
            if (pageChkFailedTime >= 2) { // 2 : check failed times threshold
                endFlag = true;
                break;
            }
        }
        DoWriteTraceData(g_buffer, bytes, writeLen);
        if (endFlag) {
            break;
        }
    }
    UpdateTraceContentHeader(contentHeader, static_cast<uint32_t>(writeLen));
    HILOG_INFO(LOG_CORE, "WriteTraceData end, type: %{public}d, byte: %{public}zd. g_writeFileLimit: %{public}d",
        contentType, writeLen, g_writeFileLimit);
    return true;
}

void ITraceContent::DoWriteTraceData(const uint8_t* buffer, const int bytes, ssize_t& writeLen)
{
    if (buffer == nullptr) {
        HILOG_ERROR(LOG_CORE, "DoWriteTraceData: buffer is nullptr!");
        return;
    }
    ssize_t writeRet = TEMP_FAILURE_RETRY(write(traceFileFd_, buffer, bytes));
    if (writeRet < 0) {
        HILOG_ERROR(LOG_CORE, "DoWriteTraceData: write failed, err(%{public}s)", strerror(errno));
    } else {
        if (writeRet != static_cast<ssize_t>(bytes)) {
            HILOG_WARN(LOG_CORE, "DoWriteTraceData: not write all done, writeLen(%{public}zd), FullLen(%{public}d)",
                writeRet, bytes);
        }
        writeLen += writeRet;
    }
}

bool ITraceContent::DoWriteTraceContentHeader(TraceFileContentHeader& contentHeader, const uint8_t contentType)
{
    contentHeader.type = contentType;
    ssize_t writeRet = TEMP_FAILURE_RETRY(write(traceFileFd_, reinterpret_cast<char *>(&contentHeader),
        sizeof(contentHeader)));
    if (writeRet <= 0) {
        HILOG_ERROR(LOG_CORE, "DoWriteTraceContentHeader: failed to write content header, err(%{public}s)",
            strerror(errno));
        return false;
    }
    return true;
}

void ITraceContent::UpdateTraceContentHeader(struct TraceFileContentHeader& contentHeader, const uint32_t writeLen)
{
    contentHeader.length = writeLen;
    uint32_t offset = contentHeader.length + sizeof(contentHeader);
    off_t pos = lseek(traceFileFd_, 0, SEEK_CUR);
    lseek(traceFileFd_, pos - offset, SEEK_SET);
    write(traceFileFd_, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    lseek(traceFileFd_, pos, SEEK_SET);
    g_outputFileSize += static_cast<int>(offset);
}

bool ITraceContent::IsFileExist()
{
    // 每10次数据段写入，检查一次文件是否存在
    g_writeFileLimit++;
    if (g_writeFileLimit > JUDGE_FILE_EXIST) {
        g_writeFileLimit = 0;
        if (access(traceFilePath_.c_str(), F_OK) != 0) {
            HILOG_WARN(LOG_CORE, "IsFileExist access file:%{public}s failed, errno: %{public}d.",
                traceFilePath_.c_str(), errno);
            return false;
        }
    }
    return true;
}

bool ITraceContent::CheckPage(uint8_t* page)
{
    if (isHm_) {
        return true;
    }
    const int pageThreshold = PAGE_SIZE / 2; // pageThreshold = 2kB
    PageHeader *pageHeader = reinterpret_cast<PageHeader*>(&page);
    if (pageHeader->size < static_cast<uint64_t>(pageThreshold)) {
        return false;
    }
    return true;
}

int ITraceContent::GetCurrentFileSize()
{
    return g_outputFileSize;
}

void ITraceContent::ResetCurrentFileSize()
{
    g_outputFileSize = 0;
}

void ITraceContent::WriteProcessLists(ssize_t& writeLen)
{
    std::string result;
    DIR* procDir = opendir("/proc");
    if (procDir == nullptr) {
        HILOG_ERROR(LOG_CORE, "WriteProcessLists: open /proc failed, errno: %{public}d.", errno);
        return;
    }
    int bytes = 0;
    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type != DT_DIR || std::string(entry->d_name) == "." || std::string(entry->d_name) == "..") {
            continue;
        }
        std::string dirName(entry->d_name);
        if (!std::all_of(dirName.begin(), dirName.end(), ::isdigit)) {
            continue;
        }
        std::string statusPath = CanonicalizeSpecPath(std::string("/proc/" + dirName + "/status").c_str());
        std::ifstream statusFile(statusPath);
        if (!statusFile.is_open()) {
            continue;
        }
        std::string line;
        std::string processName;
        while (std::getline(statusFile, line)) {
            if (line.find("Name:") != std::string::npos) {
                processName = line.substr(line.find(":") + 1);
                break;
            }
        }
        if (processName.empty()) {
            continue;
        }
        result = dirName + " " + processName + "\n";
        if (memcpy_s(g_buffer + bytes, BUFFER_SIZE - bytes, result.c_str(), result.length()) != EOK) {
            HILOG_ERROR(LOG_CORE, "WriteProcessLists: failed to memcpy result to g_buffer.");
            continue;
        }
        bytes += static_cast<int>(result.length());
        if (bytes <= BUFFER_SIZE - static_cast<int>(PAGE_SIZE)) {
            continue;
        }
        DoWriteTraceData(g_buffer, bytes, writeLen);
        bytes = 0;
    }
    if (bytes > 0) {
        DoWriteTraceData(g_buffer, bytes, writeLen);
    }
    closedir(procDir);
}

bool ITraceFileHdrContent::InitTraceFileHdr(TraceFileHeader& fileHdr)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        fileHdr.reserved |= 0;
    } else if (sizeof(void*) == sizeof(uint32_t)) {
        fileHdr.reserved |= 1;
    }
    HILOG_INFO(LOG_CORE, "InitTraceFileHdr: reserved with arch word info is %{public}d.", fileHdr.reserved);
    const int maxCpuNums = 24;
    int cpuNums = GetCpuProcessors();
    if (cpuNums > maxCpuNums || cpuNums <= 0) {
        HILOG_ERROR(LOG_CORE, "InitTraceFileHdr error: cpu_number is %{public}d.", cpuNums);
        return false;
    }
    fileHdr.reserved |= (static_cast<uint64_t>(cpuNums) << 1);
    HILOG_INFO(LOG_CORE, "InitTraceFileHdr: reserved with cpu number info is %{public}d.", fileHdr.reserved);
    return true;
}

bool TraceFileHdrLinux::WriteTraceContent()
{
    TraceFileHeader header;
    if (!InitTraceFileHdr(header)) {
        return false;
    }
    ssize_t writeRet = TEMP_FAILURE_RETRY(write(traceFileFd_, reinterpret_cast<char*>(&header), sizeof(header)));
    if (writeRet < 0) {
        HILOG_ERROR(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
            strerror(errno), sizeof(header));
        return false;
    }
    return true;
}

bool TraceFileHdrHM::WriteTraceContent()
{
    TraceFileHeader header;
    if (!InitTraceFileHdr(header)) {
        return false;
    }
    header.fileType = HM_FILE_RAW_TRACE;
    ssize_t writeRet = TEMP_FAILURE_RETRY(write(traceFileFd_, reinterpret_cast<char*>(&header), sizeof(header)));
    if (writeRet < 0) {
        HILOG_ERROR(LOG_CORE, "Failed to write trace file header, errno: %{public}s, headerLen: %{public}zu.",
            strerror(errno), sizeof(header));
        return false;
    }
    return true;
}

bool TraceBaseInfoContent::WriteTraceContent()
{
    struct TraceFileContentHeader contentHeader;
    contentHeader.type = CONTENT_TYPE_BASE_INFO;
    ssize_t writeRet = write(traceFileFd_, reinterpret_cast<char *>(&contentHeader), sizeof(contentHeader));
    if (writeRet < 0) {
        HILOG_WARN(LOG_CORE, "Write BaseInfo contentHeader failed, errno: %{public}d.", errno);
        return false;
    }
    auto writeLen = WriteKernelVersion();
    writeLen += WriteUnixTimeMs();
    writeLen += WriteBootTimeMs();
    UpdateTraceContentHeader(contentHeader, writeLen);
    return true;
}

ssize_t TraceBaseInfoContent::WriteKeyValue(const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty()) {
        HILOG_ERROR(LOG_CORE, "WriteKeyValue: key or value is empty.");
        return 0;
    }
    std::string keyValue = key + ": " + value + "\n";
    ssize_t writeRet = write(traceFileFd_, keyValue.data(), keyValue.size());
    if (writeRet < 0) {
        HILOG_WARN(LOG_CORE, "Write Key %{public}s Value %{public}s failed, errno: %{public}d.",
            key.c_str(), value.c_str(), errno);
        return 0;
    } else {
        return writeRet;
    }
}

ssize_t TraceBaseInfoContent::WriteKernelVersion()
{
    std::string kernelVersion = GetKernelVersion();
    HILOG_INFO(LOG_CORE, "WriteKernelVersion : current version %{public}s", kernelVersion.c_str());
    return WriteKeyValue("KERNEL_VERSION", kernelVersion);
}

ssize_t TraceBaseInfoContent::WriteUnixTimeMs()
{
    uint64_t unixTimeMs = GetCurUnixTimeMs();
    HILOG_INFO(LOG_CORE, "WriteUnixTimeMs : current time %{public}" PRIu64, unixTimeMs);
    std::string timeStr = std::to_string(unixTimeMs);
    return WriteKeyValue("UNIX_TIME_MS", timeStr);
}

ssize_t TraceBaseInfoContent::WriteBootTimeMs()
{
    uint64_t bootTimeMs = GetCurBootTime() / MS_TO_NS;
    HILOG_INFO(LOG_CORE, "WriteBootTimeMs : current time %{public}" PRIu64, bootTimeMs);
    std::string timeStr = std::to_string(bootTimeMs);
    return WriteKeyValue("BOOT_TIME_MS", timeStr);
}

TraceEventFmtContent::TraceEventFmtContent(const int fd,
                                           const std::string& tracefsPath,
                                           const std::string& traceFilePath,
                                           const bool ishm)
    : ITraceContent(fd, tracefsPath, traceFilePath, ishm)
{
    const std::string savedEventsFormatPath = TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT;
    bool hasPreWrotten = true;
    if (access(savedEventsFormatPath.c_str(), F_OK) != -1) {
        traceSourceFd_ = open(savedEventsFormatPath.c_str(), O_RDONLY | O_NONBLOCK);
    } else {
        traceSourceFd_ = open(savedEventsFormatPath.c_str(),
            O_CREAT | O_RDWR | O_TRUNC | O_NONBLOCK, 0644); // 0644:-rw-r--r--
        hasPreWrotten = false;
    }
    if (traceSourceFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceEventFmtContent: open %{public}s failed.", savedEventsFormatPath.c_str());
    }
    if (!hasPreWrotten && traceFileFd_ >= 0) {
        PreWriteAllTraceEventsFormat(traceSourceFd_, tracefsPath_);
        (void)lseek(traceFileFd_, 0, SEEK_SET);
    }
}

bool TraceEventFmtContent::WriteTraceContent()
{
    return WriteTraceData(CONTENT_TYPE_EVENTS_FORMAT);
}

TraceCmdLinesContent::TraceCmdLinesContent(const int fd,
                                           const std::string& tracefsPath,
                                           const std::string& traceFilePath,
                                           const bool ishm)
    : ITraceContent(fd, tracefsPath, traceFilePath, ishm)
{
    const std::string cmdlinesPath = tracefsPath_ + "saved_cmdlines";
    traceSourceFd_ = open(cmdlinesPath.c_str(), O_RDONLY | O_NONBLOCK);
    if (traceSourceFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceCmdLinesContent: open %{public}s failed.", cmdlinesPath.c_str());
    }
}

bool TraceCmdLinesContent::WriteTraceContent()
{
    return WriteTraceData(CONTENT_TYPE_CMDLINES);
}

TraceTgidsContent::TraceTgidsContent(const int fd, const std::string& tracefsPath, const std::string& traceFilePath,
                                     const bool ishm)
    : ITraceContent(fd, tracefsPath, traceFilePath, ishm)
{
    const std::string tgidsPath = tracefsPath_ + "saved_tgids";
    traceSourceFd_ = open(tgidsPath.c_str(), O_RDONLY | O_NONBLOCK);
    if (traceSourceFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceTgidsContent: open %{public}s failed.", tgidsPath.c_str());
    }
}

bool TraceTgidsContent::WriteTraceContent()
{
    return WriteTraceData(CONTENT_TYPE_TGIDS);
}

bool ITraceCpuRawContent::WriteTracePipeRawData(const std::string& srcPath, const int cpuIdx)
{
    if (!IsFileExist()) {
        HILOG_ERROR(LOG_CORE, "WriteTracePipeRawData: trace file (%{public}s) not found.", traceFilePath_.c_str());
        return false;
    }
    std::string path = CanonicalizeSpecPath(srcPath.c_str());
    auto rawTraceFd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (rawTraceFd < 0) {
        HILOG_ERROR(LOG_CORE, "WriteTracePipeRawData: open %{public}s failed.", srcPath.c_str());
        return false;
    }
    struct TraceFileContentHeader rawtraceHdr;
    if (!DoWriteTraceContentHeader(rawtraceHdr, CONTENT_TYPE_CPU_RAW + cpuIdx)) {
        close(rawTraceFd);
        return false;
    }
    ssize_t writeLen = 0;
    int pageChkFailedTime = 0;
    bool printFirstPageTime = false; // attention: update first page time in every WriteTracePipeRawData calling.
    while (true) {
        int bytes = 0;
        bool endFlag = false;
        ReadTracePipeRawLoop(rawTraceFd, bytes, endFlag, pageChkFailedTime, printFirstPageTime);
        DoWriteTraceData(g_buffer, bytes, writeLen);
        if (IsWriteFileOverflow(g_outputFileSize, writeLen,
            request_.fileSize != 0 ? request_.fileSize : DEFAULT_FILE_SIZE * KB_PER_MB)) {
            isOverFlow_ = true;
            break;
        }
        if (endFlag) {
            break;
        }
    }
    UpdateTraceContentHeader(rawtraceHdr, static_cast<uint32_t>(writeLen));
    if (writeLen > 0) {
        dumpStatus_ = TraceErrorCode::SUCCESS;
    } else if (dumpStatus_ == TraceErrorCode::UNSET) {
        dumpStatus_ = TraceErrorCode::OUT_OF_TIME;
    }
    HILOG_INFO(LOG_CORE, "WriteTracePipeRawData end, path: %{public}s, byte: %{public}zd. g_writeFileLimit: %{public}d",
        srcPath.c_str(), writeLen, g_writeFileLimit);
    close(rawTraceFd);
    return true;
}

void ITraceCpuRawContent::ReadTracePipeRawLoop(const int srcFd,
    int& bytes, bool& endFlag, int& pageChkFailedTime, bool& printFirstPageTime)
{
    while (bytes <= (BUFFER_SIZE - static_cast<int>(PAGE_SIZE))) {
        ssize_t readBytes = TEMP_FAILURE_RETRY(read(srcFd, g_buffer + bytes, PAGE_SIZE));
        if (readBytes <= 0) {
            endFlag = true;
            HILOG_DEBUG(LOG_CORE, "ReadTracePipeRawLoop: read raw trace done, size(%{public}zd), err(%{public}s).",
                readBytes, strerror(errno));
            break;
        }
        uint64_t pageTraceTime = 0;
        if (memcpy_s(&pageTraceTime, sizeof(pageTraceTime), g_buffer + bytes, sizeof(uint64_t)) != EOK) {
            HILOG_ERROR(LOG_CORE, "ReadTracePipeRawLoop: failed to memcpy g_buffer to pageTraceTime.");
            break;
        }
        // attention : only capture target duration trace data
        int pageValid = IsCurrentTracePageValid(pageTraceTime, request_.traceStartTime, request_.traceEndTime);
        if (pageValid < 0) {
            endFlag = true;
            bytes += (printFirstPageTime ? readBytes : 0);
            break;
        } else if (pageValid == 0) {
            continue;
        }

        lastPageTimeStamp_ = std::max(lastPageTimeStamp_, pageTraceTime);
        if (UNEXPECTANTLY(!printFirstPageTime)) {
            HILOG_INFO(LOG_CORE, "ReadTracePipeRawLoop: First page trace time(%{public}" PRIu64 ")", pageTraceTime);
            printFirstPageTime = true;
            firstPageTimeStamp_ = std::min(firstPageTimeStamp_, pageTraceTime);
        }

        if (!CheckPage(g_buffer + bytes)) {
            pageChkFailedTime++;
        }
        bytes += readBytes;
        if (pageChkFailedTime >= 2) { // 2 : check failed times threshold
            endFlag = true;
            break;
        }
    }
}

bool ITraceCpuRawContent::IsWriteFileOverflow(const int outputFileSize, const ssize_t writeLen,
                                              const int fileSizeThreshold)
{
    // attention: we only check file size threshold in CMD/CACHE mode if need.
    if ((request_.type != TRACE_TYPE::TRACE_RECORDING && request_.type != TRACE_TYPE::TRACE_CACHE) ||
        !request_.limitFileSz) {
        return false;
    }
    if (outputFileSize + writeLen + static_cast<int>(sizeof(TraceFileContentHeader)) >= fileSizeThreshold) {
        HILOG_ERROR(LOG_CORE, "Failed to write, current round write file size exceeds the file size limit(%{public}d).",
            fileSizeThreshold);
        return true;
    }
    if (writeLen > INT_MAX - BUFFER_SIZE) {
        HILOG_ERROR(LOG_CORE, "Failed to write, write file length is nearly overflow.");
        return true;
    }
    return false;
}

bool ITraceCpuRawContent::IsOverFlow()
{
    return isOverFlow_;
}

bool TraceCpuRawLinux::WriteTraceContent()
{
    int cpuNums = GetCpuProcessors();
    for (int cpuIdx = 0; cpuIdx < cpuNums; cpuIdx++) {
        std::string srcPath = tracefsPath_ + "per_cpu/cpu" + std::to_string(cpuIdx) + "/trace_pipe_raw";
        if (!WriteTracePipeRawData(srcPath, cpuIdx)) {
            return false;
        }
    }
    if (dumpStatus_ != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "TraceCpuRawLinux WriteTraceContent failed, dump status: %{public}hhu.", dumpStatus_);
        return false;
    }
    return true;
}

bool TraceCpuRawHM::WriteTraceContent()
{
    std::string srcPath = tracefsPath_ + "/trace_pipe_raw";
    if (!WriteTracePipeRawData(srcPath, 0)) { // 0 : hongmeng kernel only has one cpu trace raw pipe
        return false;
    }
    if (dumpStatus_ != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "TraceCpuRawHM WriteTraceContent failed, dump status: %{public}hhu.", dumpStatus_);
        return false;
    }
    return true;
}

bool ITraceCpuRawRead::CopyTracePipeRawLoop(const int srcFd, const int cpu, ssize_t& writeLen,
    int& pageChkFailedTime, bool& printFirstPageTime)
{
    const size_t bufferSz = TraceBufferManager::GetInstance().GetBlockSize();
    auto buffer = TraceBufferManager::GetInstance().AllocateBlock(request_.taskId, cpu);
    if (buffer == nullptr) {
        HILOG_ERROR(LOG_CORE, "CopyTracePipeRawLoop: Failed to allocate memory block.");
        return true;
    }
    bool isStopRead = false;
    ssize_t blockReadSz = 0;
    uint8_t pageBuffer[PAGE_SIZE] = {};
    while (blockReadSz <= static_cast<ssize_t>(bufferSz - PAGE_SIZE)) {
        ssize_t readBytes = TEMP_FAILURE_RETRY(read(srcFd, pageBuffer, PAGE_SIZE));
        if (readBytes <= 0) {
            HILOG_DEBUG(LOG_CORE, "CopyTracePipeRawLoop: read raw trace done, size(%{public}zd), err(%{public}s).",
                readBytes, strerror(errno));
            break;
        }
        uint64_t pageTraceTime = 0;
        if (memcpy_s(&pageTraceTime, sizeof(pageTraceTime), pageBuffer, sizeof(uint64_t)) != EOK) {
            HILOG_ERROR(LOG_CORE, "CopyTracePipeRawLoop: failed to memcpy pagebuffer to pageTraceTime.");
            break;
        }
        // attention : only capture target duration trace data
        int pageValid = IsCurrentTracePageValid(pageTraceTime, request_.traceStartTime, request_.traceEndTime);
        if (pageValid < 0) {
            isStopRead = true;
            blockReadSz += (printFirstPageTime ? readBytes : 0);
            buffer->Append(pageBuffer, readBytes);
            break;
        } else if (pageValid == 0) {
            continue;
        }
        lastPageTimeStamp_ = std::max(lastPageTimeStamp_, pageTraceTime);
        if (UNEXPECTANTLY(!printFirstPageTime)) {
            HILOG_INFO(LOG_CORE, "CopyTracePipeRawLoop: First page trace time(%{public}" PRIu64 ")", pageTraceTime);
            printFirstPageTime = true;
            firstPageTimeStamp_ = std::min(firstPageTimeStamp_, pageTraceTime);
        }
        if (!CheckPage(pageBuffer)) {
            pageChkFailedTime++;
        }
        blockReadSz += readBytes;
        buffer->Append(pageBuffer, readBytes);
        if (pageChkFailedTime >= 2) { // 2 : check failed times threshold
            isStopRead = true;
            break;
        }
    }
    writeLen += blockReadSz;
    return isStopRead;
}

bool ITraceCpuRawRead::CacheTracePipeRawData(const std::string& srcPath, const int cpuIdx)
{
    std::string path = CanonicalizeSpecPath(srcPath.c_str());
    auto rawTraceFd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (rawTraceFd < 0) {
        HILOG_ERROR(LOG_CORE, "CacheTracePipeRawData: open %{public}s failed.", srcPath.c_str());
        return false;
    }
    ssize_t writeLen = 0;
    int pageChkFailedTime = 0;
    bool printFirstPageTime = false; // attention: update first page time in every WriteTracePipeRawData calling.
    while (true) {
        if (CopyTracePipeRawLoop(rawTraceFd, cpuIdx, writeLen, pageChkFailedTime, printFirstPageTime)) {
            break;
        }
    }
    if (writeLen > 0) {
        dumpStatus_ = TraceErrorCode::SUCCESS;
    } else if (dumpStatus_ == TraceErrorCode::UNSET) {
        dumpStatus_ = TraceErrorCode::OUT_OF_TIME;
    }
    HILOG_INFO(LOG_CORE, "CacheTracePipeRawData end, path: %{public}s, byte: %{public}zd. g_writeFileLimit: %{public}d",
        srcPath.c_str(), writeLen, g_writeFileLimit);
    close(rawTraceFd);
    return true;
}

bool TraceCpuRawReadLinux::WriteTraceContent()
{
    int cpuNums = GetCpuProcessors();
    for (int cpuIdx = 0; cpuIdx < cpuNums; cpuIdx++) {
        std::string srcPath = tracefsPath_ + "per_cpu/cpu" + std::to_string(cpuIdx) + "/trace_pipe_raw";
        if (!CacheTracePipeRawData(srcPath, cpuIdx)) {
            return false;
        }
    }
    if (dumpStatus_ != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "TraceCpuRawReadLinux WriteTraceContent failed, dump status: %{public}hhu.", dumpStatus_);
        TraceBufferManager::GetInstance().ReleaseTaskBlocks(request_.taskId);
        return false;
    }
    return true;
}

bool TraceCpuRawReadHM::WriteTraceContent()
{
    std::string srcPath = tracefsPath_ + "/trace_pipe_raw";
    if (!CacheTracePipeRawData(srcPath, 0)) { // 0 : hongmeng kernel only has one cpu trace raw pipe
        return false;
    }
    if (dumpStatus_ != TraceErrorCode::SUCCESS) {
        HILOG_ERROR(LOG_CORE, "TraceCpuRawReadHM WriteTraceContent failed, dump status: %{public}hhu.", dumpStatus_);
        TraceBufferManager::GetInstance().ReleaseTaskBlocks(request_.taskId);
        return false;
    }
    return true;
}

bool TraceCpuRawWriteLinux::WriteTraceContent()
{
    if (!IsFileExist()) {
        HILOG_ERROR(LOG_CORE, "TraceCpuRawWriteLinux::WriteTraceContent trace file (%{public}s) not found.",
            traceFilePath_.c_str());
        return false;
    }
    int prevCpu = -1;
    ssize_t writeLen = 0;
    struct TraceFileContentHeader rawHeader;
    auto buffers = TraceBufferManager::GetInstance().GetTaskBuffers(taskId_);
    for (auto& bufItem : buffers) {
        int cpuIdx = bufItem->cpu;
        if (cpuIdx != prevCpu) {
            writeLen = 0;
            if (!DoWriteTraceContentHeader(rawHeader, CONTENT_TYPE_CPU_RAW + cpuIdx)) {
                return false;
            }
        }
        DoWriteTraceData(bufItem->data.data(), bufItem->usedBytes, writeLen); // attention: maybe write null data.
        UpdateTraceContentHeader(rawHeader, static_cast<uint32_t>(writeLen));
    }
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId_);
    HILOG_INFO(LOG_CORE, "TraceCpuRawWriteLinux::WriteTraceContent write len %{public}zd", writeLen);
    return true;
}

bool TraceCpuRawWriteHM::WriteTraceContent()
{
    if (!IsFileExist()) {
        HILOG_ERROR(LOG_CORE, "TraceCpuRawWriteHM::WriteTraceContent trace file (%{public}s) not found.",
            traceFilePath_.c_str());
        return false;
    }
    struct TraceFileContentHeader rawHeader;
    if (!DoWriteTraceContentHeader(rawHeader, CONTENT_TYPE_CPU_RAW)) {
        return false;
    }
    ssize_t writeLen = 0;
    auto buffers = TraceBufferManager::GetInstance().GetTaskBuffers(taskId_);
    for (auto& bufItem : buffers) {
        DoWriteTraceData(bufItem->data.data(), bufItem->usedBytes, writeLen); // attention: maybe write null data.
    }
    UpdateTraceContentHeader(rawHeader, static_cast<uint32_t>(writeLen));
    TraceBufferManager::GetInstance().ReleaseTaskBlocks(taskId_);
    return true;
}

TraceHeaderPageLinux::TraceHeaderPageLinux(const int fd,
                                           const std::string& tracefsPath, const std::string& traceFilePath)
    : ITraceHeaderPageContent(fd, tracefsPath, traceFilePath, false)
{
    const std::string headerPagePath = tracefsPath_ + "events/header_page";
    traceSourceFd_ = open(headerPagePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (traceSourceFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TraceHeaderPageLinux: open %{public}s failed.", headerPagePath.c_str());
    }
}

bool TraceHeaderPageLinux::WriteTraceContent()
{
    return WriteTraceData(CONTENT_TYPE_HEADER_PAGE);
}

bool TraceHeaderPageHM::WriteTraceContent()
{
    return true;
}

TracePrintkFmtLinux::TracePrintkFmtLinux(const int fd,
                                         const std::string& tracefsPath, const std::string& traceFilePath)
    : ITracePrintkFmtContent(fd, tracefsPath, traceFilePath, false)
{
    const std::string printkFormatPath = tracefsPath_ + "printk_formats";
    traceSourceFd_ = open(printkFormatPath.c_str(), O_RDONLY | O_NONBLOCK);
    if (traceSourceFd_ < 0) {
        HILOG_ERROR(LOG_CORE, "TracePrintkFmtLinux: open %{public}s failed.", printkFormatPath.c_str());
    }
}

bool TracePrintkFmtLinux::WriteTraceContent()
{
    return WriteTraceData(CONTENT_TYPE_PRINTK_FORMATS);
}

bool TracePrintkFmtHM::WriteTraceContent()
{
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS