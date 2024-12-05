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

#include <fstream>
#include <iostream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "common_define.h"

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
class HitraceSystemTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
const int TIME_COUNT = 10000;

bool RunCmd(const string& cmdstr)
{
    if (cmdstr.empty()) {
        return false;
    }
    FILE *fp = popen(cmdstr.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    char res[4096] = { '\0' };
    while (fgets(res, sizeof(res), fp) != nullptr) {
        std::cout << res;
    }
    pclose(fp);
    return true;
}

bool IsTracingOn()
{
    std::ifstream tracingOnFile(TRACEFS_DIR + TRACING_ON_NODE);
    if (!tracingOnFile.is_open()) {
        std::cout << "Failed to open /sys/kernel/tracing/tracing_on." << std::endl;
        return false;
    }
    std::string content;
    getline(tracingOnFile, content);
    tracingOnFile.close();
    return content == "1";
}

bool IsFileIncludeAllKeyWords(const string& fileName, std::vector<std::string>& keywords)
{
    std::ifstream readFile;
    readFile.open(fileName.c_str(), std::ios::in);
    if (readFile.fail()) {
        GTEST_LOG_(ERROR) << "Failed to open file " << fileName;
        return false;
    }
    int keywordIdx = 0;
    std::string readLine;
    while (getline(readFile, readLine, '\n') && keywordIdx < keywords.size()) {
        if (readLine.find(keywords[keywordIdx]) == std::string::npos) {
            continue;
        }
        keywordIdx++;
    }
    readFile.close();
    if (keywordIdx < keywords.size()) {
        GTEST_LOG_(ERROR) << "Failed to find keyword: " << keywords[keywordIdx];
    }
    return keywordIdx == keywords.size();
}

bool IsFileExcludeAllKeyWords(const string& fileName, std::vector<std::string>& keywords)
{
    std::ifstream readFile;
    readFile.open(fileName.c_str(), std::ios::in);
    if (readFile.fail()) {
        GTEST_LOG_(ERROR) << "Failed to open file " << fileName;
        return false;
    }
    std::string readLine;
    while (getline(readFile, readLine, '\n')) {
        for (auto& word : keywords) {
            if (readLine.find(word) != std::string::npos) {
                GTEST_LOG_(ERROR) << "File contained keyword: " << word;
                return false;
            }
        }
    }
    readFile.close();
    return true;
}

/**
 * @tc.name: HitraceSystemTest001
 * @tc.desc: when excute hitrace record command, check tracing_on switcher status
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTest001, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin ace"));
    int testCnt = TIME_COUNT;
    while (testCnt > 0) {
        usleep(10);
        ASSERT_TRUE(IsTracingOn()) << "tracing_on switcher status is not 1";
        testCnt--;
    }
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
}

/**
 * @tc.name: HitraceSystemTest002
 * @tc.desc: when excute hitrace record command, check saved_events_format
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTest002, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin sched"));
    std::vector<std::string> includeWords = { "name: print", "name: sched_wakeup", "name: sched_switch" };
    ASSERT_TRUE(IsFileIncludeAllKeyWords(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT, includeWords));
    std::vector<std::string> excludeWords = { "name: binder_transaction", "name: binder_transaction_received" };
    ASSERT_TRUE(IsFileExcludeAllKeyWords(TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT, excludeWords));
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
}
} // namespace
} // namespace HiviewDFX
} // namespace OHOS