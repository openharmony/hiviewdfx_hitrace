/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#include "file_ageing_utils.h"

#include <queue>
#include <set>
#include <sys/xattr.h>
#include <unistd.h>

#include "hilog/log.h"

#include "common_utils.h"
#include "trace_file_utils.h"
#include "trace_json_parser.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceUtils"
#endif

namespace {
constexpr int BYTE_PER_KB = 1024;
constexpr size_t DEFAULT_LINK_NUM = 15;
using namespace OHOS::HiviewDFX::Hitrace;

class FileAgeingChecker {
public:
    virtual bool ShouldAgeing(const TraceFileInfo& traceFileInfo) = 0;
    virtual ~FileAgeingChecker() = default;

    static std::shared_ptr<FileAgeingChecker> CreateFileChecker(const TraceDumpType traceType,
        const CheckType checkType);
};

class FileNumberChecker : public FileAgeingChecker {
public:
    explicit FileNumberChecker(int64_t count) : maxCount_(count) {};

    bool ShouldAgeing(const TraceFileInfo& traceFileInfo) override
    {
        if (currentCount_ >= maxCount_) {
            return true;
        }
        currentCount_++;
        return false;
    }
private:
    int64_t currentCount_ = 0;
    const int64_t maxCount_;
};

class FileSizeChecker : public FileAgeingChecker {
public:
    explicit FileSizeChecker(int64_t sizeKb) : maxSizeKb_(sizeKb) {};

    bool ShouldAgeing(const TraceFileInfo& traceFileInfo) override
    {
        if (currentFileNumber_ < minFileNumber_) {
            currentFileNumber_++;
            currentSizeKb_ += traceFileInfo.fileSize / BYTE_PER_KB;
            return false;
        }

        if (currentSizeKb_ >= maxSizeKb_) {
            return true;
        }
        currentSizeKb_ += traceFileInfo.fileSize / BYTE_PER_KB;
        return false;
    }

private:
    int64_t currentSizeKb_ = 0;
    const int64_t maxSizeKb_;

    int64_t currentFileNumber_ = 0;
    const int64_t minFileNumber_ = 2;  // To prevent all files from being cleared, set a minimum file number limit
};

std::shared_ptr<FileAgeingChecker> FileAgeingChecker::CreateFileChecker(const TraceDumpType traceType,
    const CheckType checkType)
{
    if (traceType != TraceDumpType::TRACE_RECORDING && traceType != TraceDumpType::TRACE_SNAPSHOT) {
        return nullptr;
    }

    const AgeingParam& param = TraceJsonParser::Instance().GetAgeingParam(traceType);
    if (IsRootVersion() && !param.rootEnable) {
        return nullptr;
    }

    if (param.fileSizeKbLimit > 0 && checkType == CheckType::FILESIZE) {
        return std::make_shared<FileSizeChecker>(param.fileSizeKbLimit);
    }

    if (param.fileNumberLimit > 0 && checkType == CheckType::FILENUMBER) {
        return std::make_shared<FileNumberChecker>(param.fileNumberLimit);
    }

    return nullptr;
}

void HandleFileNotInVec(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType, int32_t& deleteCount)
{
    // handle files that are not saved in vector
    std::set<std::string> traceFiles = {};
    GetTraceFileNamesInDir(traceFiles, traceType);
    for (const auto& traceFileInfo : fileList) {
        traceFiles.erase(traceFileInfo.filename);
    }
    for (const auto& filename : traceFiles) {
        if (RemoveFile(filename)) {
            deleteCount++;
        }
    }
    HILOG_INFO(LOG_CORE, "HandleAgeing: deleteCount:%{public}d type:%{public}d",
               deleteCount, static_cast<int32_t>(traceType));
}

void HandleFileNeedAgeing(const int64_t& needDelete, const CheckType checkType, std::vector<TraceFileInfo>& fileList,
    std::set<std::string>& needRemoveFiles, std::queue<TraceFileInfo>& linkFiles)
{
    int64_t currCount = 0;
    for (const auto& fileInfo : fileList) {
        if (currCount >= needDelete) {
            break;
        }
        ssize_t len = TEMP_FAILURE_RETRY(getxattr(fileInfo.filename.c_str(), ATTR_NAME_LINK, nullptr, 0));
        if (len == -1) {
            needRemoveFiles.insert(fileInfo.filename);
        } else {
            linkFiles.push(fileInfo);
            if (linkFiles.size() <= DEFAULT_LINK_NUM) {
                continue;
            }
            auto linkfileInfo = linkFiles.front();
            needRemoveFiles.insert(linkfileInfo.filename);
            linkFiles.pop();
            if (checkType == CheckType::FILESIZE) {
                currCount += linkfileInfo.fileSize;
                continue;
            }
        }

        if (checkType == CheckType::FILESIZE) {
            currCount += fileInfo.fileSize;
        } else if (checkType == CheckType::FILENUMBER) {
            currCount++;
        } else {
            break;
        }
    }
}

void HandleAgeingImpl(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType, FileAgeingChecker& helper)
{
    int32_t deleteCount = 0;
    // handle the files saved in vector
    std::vector<TraceFileInfo> result = {};
    for (auto it = fileList.rbegin(); it != fileList.rend(); it++) {
        if (helper.ShouldAgeing(*it)) {
            if (RemoveFile(it->filename)) {
                deleteCount++;
            }
        } else {
            result.emplace_back(*it);
        }
    }
    fileList.assign(result.rbegin(), result.rend());
    HandleFileNotInVec(fileList, traceType, deleteCount);
}

static void CalculateFilesize(const std::vector<TraceFileInfo>& fileList, int64_t& countSize)
{
    size_t currLinkNum = 0;
    for (const auto &fileInfo : fileList) {
        if (TEMP_FAILURE_RETRY(getxattr(fileInfo.filename.c_str(), ATTR_NAME_LINK, nullptr, 0)) == -1) {
            countSize += fileInfo.fileSize;
        } else {
            if ((++currLinkNum) > DEFAULT_LINK_NUM) {
                countSize += fileInfo.fileSize;
            }
        }
    }
}

void HandleAgeingSnapShort(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType,
    const CheckType checkType)
{
    const AgeingParam& param = TraceJsonParser::Instance().GetAgeingParam(traceType);
    int64_t needDelete = 0;
    int32_t deleteCount = 0;
    std::set<std::string> needRemoveFiles = {};
    std::queue<TraceFileInfo> linkFiles= {};
    if (checkType == CheckType::FILESIZE) {
        int64_t countSize = 0;
        CalculateFilesize(fileList, countSize);
        needDelete = countSize - param.fileSizeKbLimit * BYTE_PER_KB;
        if (needDelete <= 0 || needDelete == countSize) {
            HandleFileNotInVec(fileList, traceType, deleteCount);
            return;
        }
        HandleFileNeedAgeing(needDelete, CheckType::FILESIZE, fileList, needRemoveFiles, linkFiles);
    } else if (checkType == CheckType::FILENUMBER) {
        needDelete = static_cast<int64_t>(fileList.size()) - param.fileNumberLimit;
        if (needDelete <= 0 || needDelete == static_cast<int64_t>(fileList.size())) {
            HandleFileNotInVec(fileList, traceType, deleteCount);
            return;
        }
        HandleFileNeedAgeing(needDelete, CheckType::FILENUMBER, fileList, needRemoveFiles, linkFiles);
    }
    
    for (auto iter = needRemoveFiles.begin(); iter != needRemoveFiles.end();) {
        if (RemoveFile(*iter)) {
            deleteCount++;
            ++iter;
        } else {
            iter = needRemoveFiles.erase(iter);
        }
    }
    fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
        [&needRemoveFiles] (const TraceFileInfo& fileInfo) {
            return needRemoveFiles.count(fileInfo.filename) != 0;
        }), fileList.end());
    HandleFileNotInVec(fileList, traceType, deleteCount);
}
}  // namespace

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

void FileAgeingUtils::HandleAgeing(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType)
{
    std::shared_ptr<FileAgeingChecker> checkerFilenumber = FileAgeingChecker::CreateFileChecker(traceType,
        CheckType::FILENUMBER);
    if (checkerFilenumber != nullptr) {
        if (traceType == TraceDumpType::TRACE_RECORDING) {
            HandleAgeingImpl(fileList, traceType, *checkerFilenumber);
        } else if (traceType == TraceDumpType::TRACE_SNAPSHOT) {
            HandleAgeingSnapShort(fileList, traceType, CheckType::FILENUMBER);
        }
    }
    std::shared_ptr<FileAgeingChecker> checkerFilesize = FileAgeingChecker::CreateFileChecker(traceType,
        CheckType::FILESIZE);
    if (checkerFilesize != nullptr) {
        if (traceType == TraceDumpType::TRACE_RECORDING) {
            HandleAgeingImpl(fileList, traceType, *checkerFilesize);
        } else if (traceType == TraceDumpType::TRACE_SNAPSHOT) {
            HandleAgeingSnapShort(fileList, traceType, CheckType::FILESIZE);
        }
    }
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
