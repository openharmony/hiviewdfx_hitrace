/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

using namespace testing::ext;
using namespace std;

const int TIME_COUNT = 10000;
const std::string TRACING_ON_DIR = "/sys/kernel/tracing/tracing_on";

namespace OHOS {
namespace HiviewDFX {
class HitraceSystemTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

static bool RunCmd(const string& cmdstr)
{
    FILE *fp = popen(cmdstr.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    pclose(fp);
    return true;
}

static bool IsTracingOn()
{
    std::ifstream tracingOnFile(TRACING_ON_DIR);
    if (!tracingOnFile.is_open()) {
        std::cout << "Failed to open /sys/kernel/tracing/tracing_on." << std::endl;
        return false;
    }
    std::string content;
    getline(tracingOnFile, content);
    std::cout << "content is " << content << std::endl;
    tracingOnFile.close();
    return content == "1";
}

/**
 * @tc.name: HitraceSystemTest_001
 * @tc.desc: when excute hitrace record function, check tracing_on switcher status
 * @tc.type: FUNC
 */
HWTEST_F(HitraceSystemTest, HitraceSystemTest_001, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin ace"));
    int testCnt = TIME_COUNT;
    while (testCnt > 0) {
        ASSERT_TRUE(IsTracingOn()) << "tracing_on switcher status is not 1";
        usleep(10);
        testCnt--;
    }
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump"));
}
} // namespace HiviewDFX
} // namespace OHOS
