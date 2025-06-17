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

#include <set>

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
using namespace OHOS::HiviewDFX::Hitrace;

class FileAgeingChecker {
public:
    virtual bool ShouldAgeing(const TraceFileInfo& traceFileInfo) = 0;
    virtual ~FileAgeingChecker() = default;

    static std::shared_ptr<FileAgeingChecker> CreateFileChecker(const TRACE_TYPE traceType);
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
            currentSizeKb_ += traceFileInfo.fileSize;
            return false;
        }

        if (currentSizeKb_ >= maxSizeKb_) {
            return true;
        }
        currentSizeKb_ += traceFileInfo.fileSize;
        return false;
    }

private:
    int64_t currentSizeKb_ = 0;
    const int64_t maxSizeKb_;

    int64_t currentFileNumber_ = 0;
    const int64_t minFileNumber_ = 2;  // To prevent all files from being cleared, set a minimum file number limit
};

std::shared_ptr<FileAgeingChecker> FileAgeingChecker::CreateFileChecker(const TRACE_TYPE traceType)
{
    if (traceType != TRACE_TYPE::TRACE_RECORDING && traceType != TRACE_TYPE::TRACE_SNAPSHOT) {
        return nullptr;
    }

    const AgeingParam& param = TraceJsonParser::Instance().GetAgeingParam(traceType);
    if (IsRootVersion() && !param.rootEnable) {
        return nullptr;
    }

    if (param.fileSizeKbLimit > 0) {
        return std::make_shared<FileSizeChecker>(param.fileSizeKbLimit);
    }

    if (param.fileNumberLimit > 0) {
        return std::make_shared<FileNumberChecker>(param.fileNumberLimit);
    }

    return nullptr;
}

void HandleAgeingImpl(std::vector<TraceFileInfo>& fileList, const TRACE_TYPE traceType, FileAgeingChecker& helper)
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
}  // namespace

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

void FileAgeingUtils::HandleAgeing(std::vector<TraceFileInfo>& fileList, const TRACE_TYPE traceType)
{
    std::shared_ptr<FileAgeingChecker> checker = FileAgeingChecker::CreateFileChecker(traceType);
    if (checker != nullptr) {
        HandleAgeingImpl(fileList, traceType, *checker);
    }
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
