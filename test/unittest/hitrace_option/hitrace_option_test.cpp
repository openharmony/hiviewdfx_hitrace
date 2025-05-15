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

#include <fstream>
#include <ios>
#include "parameters.h"
#include "hitrace_option/hitrace_option.h"
#include <gtest/gtest.h>

using namespace testing::ext;
using namespace OHOS::HiviewDFX::Hitrace;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceOptionTest"
#endif

const std::string TELEMETRY_APP_PARAM = "debug.hitrace.telemetry.app";
const std::string SET_EVENT_PID = "/sys/kernel/tracing/set_event_pid";
const std::string DEBUG_SET_EVENT_PID = "/sys/kernel/debug/tracing/set_event_pid";

class HitraceOptionTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void HitraceOptionTest::SetUpTestCase() { }

void HitraceOptionTest::TearDownTestCase() { }

void HitraceOptionTest::SetUp() { }

void HitraceOptionTest::TearDown() { }

bool WriteStrToFile(const std::string& filename, const std::string& str)
{
    std::ofstream out;
    out.open(filename, std::ios::out);
    if (out.fail()) {
        return false;
    }
    out << str;
    if (out.bad()) {
        out.close();
        return false;
    }
    out.flush();
    out.close();
    return true;
}

std::string ReadFile(const std::string& filename)
{
    std::ifstream fileIn(filename.c_str());
    if (!fileIn.is_open()) {
        return "";
    }
    std::string str((std::istreambuf_iterator<char>(fileIn)), std::istreambuf_iterator<char>());
    fileIn.close();
    return str;
}

HWTEST_F(HitraceOptionTest, SetTelemetryAppNameTest, TestSize.Level1)
{
    ASSERT_TRUE(OHOS::system::SetParameter(TELEMETRY_APP_PARAM, "a"));
    ASSERT_EQ(OHOS::system::GetParameter(TELEMETRY_APP_PARAM, ""), "a");

    EXPECT_EQ(SetFilterAppName("com.test.app"), HITRACE_NO_ERROR);
    EXPECT_EQ(OHOS::system::GetParameter(TELEMETRY_APP_PARAM, ""), "com.test.app");
}

HWTEST_F(HitraceOptionTest, AddFilterPid, TestSize.Level1)
{
    WriteStrToFile(SET_EVENT_PID, "");
    ASSERT_EQ(ReadFile(SET_EVENT_PID), "");
    ASSERT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "");

    EXPECT_EQ(AddFilterPid(1), HITRACE_NO_ERROR);
    EXPECT_EQ(ReadFile(SET_EVENT_PID), "1\n");
    EXPECT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "1\n");

    pid_t pid = getpid();
    EXPECT_EQ(AddFilterPid(pid), HITRACE_NO_ERROR);
    EXPECT_EQ(ReadFile(SET_EVENT_PID), "1\n" + std::to_string(pid) + "\n");
    EXPECT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "1\n" + std::to_string(pid) + "\n");
}

HWTEST_F(HitraceOptionTest, ClearFilterPid, TestSize.Level1)
{
    WriteStrToFile(SET_EVENT_PID, "1");
    ASSERT_EQ(ReadFile(SET_EVENT_PID), "1\n");
    ASSERT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "1\n");

    EXPECT_EQ(ClearFilterPid(), HITRACE_NO_ERROR);
    EXPECT_EQ(ReadFile(SET_EVENT_PID), "");
    EXPECT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "");
}

HWTEST_F(HitraceOptionTest, FilterAppTrace, TestSize.Level1)
{
    ASSERT_TRUE(OHOS::system::SetParameter(TELEMETRY_APP_PARAM, ""));
    ASSERT_EQ(OHOS::system::GetParameter(TELEMETRY_APP_PARAM, "null"), "");
    WriteStrToFile(SET_EVENT_PID, "");
    ASSERT_EQ(ReadFile(SET_EVENT_PID), "");
    ASSERT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "");

    FilterAppTrace("com.test.app", 1);
    EXPECT_EQ(ReadFile(SET_EVENT_PID), "");

    ASSERT_TRUE(OHOS::system::SetParameter(TELEMETRY_APP_PARAM, "com.test.app"));
    ASSERT_EQ(OHOS::system::GetParameter(TELEMETRY_APP_PARAM, ""), "com.test.app");
    FilterAppTrace("com.test.app", 1);
    EXPECT_NE(ReadFile(SET_EVENT_PID), "");
}

} // namespace HitraceTest
} // namespace HiviewDFX
} // namespace OHOS
