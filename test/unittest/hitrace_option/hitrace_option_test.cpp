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
#include <sys/utsname.h>
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
const std::string NO_FILTER_EVENT = "/sys/kernel/tracing/no_filter_events";
const std::string DEBUG_NO_FILTER_EVENT = "/sys/kernel/debug/tracing/no_filter_events";

bool WriteStrToFile(const std::string& filename, const std::string& str);

class HitraceOptionTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void HitraceOptionTest::SetUpTestCase() { }

void HitraceOptionTest::TearDownTestCase() { }

void HitraceOptionTest::SetUp()
{
    WriteStrToFile(SET_EVENT_PID, "");
    WriteStrToFile(DEBUG_SET_EVENT_PID, "");
}

void HitraceOptionTest::TearDown()
{
    WriteStrToFile(SET_EVENT_PID, "");
    WriteStrToFile(DEBUG_SET_EVENT_PID, "");
}

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

bool ContainsPid(const std::string& filename, pid_t pid)
{
    std::string pidStr = std::to_string(pid);
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool find = false;
    while (std::getline(file, line)) {
        if (line == pidStr) {
            find = true;
            break;
        }
    }

    file.close();
    return find;
}

bool ContainsEvents(const std::string& filename, const std::string& events)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool find = false;
    while (std::getline(file, line)) {
        if (line.find(events) != std::string::npos) {
            find = true;
            break;
        }
    }

    file.close();
    return find;
}

bool IsHm()
{
    bool isHM = false;
    utsname unameBuf;
    if ((uname(&unameBuf)) == 0) {
        std::string osRelease = unameBuf.release;
        isHM = osRelease.find("HongMeng") != std::string::npos;
    }
    return isHM;
}

HWTEST_F(HitraceOptionTest, SetTelemetryAppNameTest_001, TestSize.Level1)
{
    ASSERT_TRUE(OHOS::system::SetParameter(TELEMETRY_APP_PARAM, "a"));
    ASSERT_EQ(OHOS::system::GetParameter(TELEMETRY_APP_PARAM, ""), "a");

    EXPECT_EQ(SetFilterAppName("com.test.app"), HITRACE_NO_ERROR);
    EXPECT_EQ(OHOS::system::GetParameter(TELEMETRY_APP_PARAM, ""), "com.test.app");
}

HWTEST_F(HitraceOptionTest, AddFilterPid_001, TestSize.Level1)
{
    WriteStrToFile(SET_EVENT_PID, "");
    ASSERT_EQ(ReadFile(SET_EVENT_PID), "");
    ASSERT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "");

    EXPECT_EQ(AddFilterPid(1), HITRACE_NO_ERROR);
    EXPECT_TRUE(ContainsPid(SET_EVENT_PID, 1));
    EXPECT_TRUE(ContainsPid(DEBUG_SET_EVENT_PID, 1));

    pid_t pid = getpid();
    EXPECT_EQ(AddFilterPid(pid), HITRACE_NO_ERROR);
    EXPECT_TRUE(ContainsPid(SET_EVENT_PID, 1));
    EXPECT_TRUE(ContainsPid(DEBUG_SET_EVENT_PID, 1));
    EXPECT_TRUE(ContainsPid(SET_EVENT_PID, pid));
    EXPECT_TRUE(ContainsPid(DEBUG_SET_EVENT_PID, pid));
}

HWTEST_F(HitraceOptionTest, ClearFilterPid_001, TestSize.Level1)
{
    WriteStrToFile(SET_EVENT_PID, "1");
    ASSERT_EQ(ReadFile(SET_EVENT_PID), "1\n");
    ASSERT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "1\n");

    EXPECT_EQ(ClearFilterPid(), HITRACE_NO_ERROR);
    EXPECT_EQ(ReadFile(SET_EVENT_PID), "");
    EXPECT_EQ(ReadFile(DEBUG_SET_EVENT_PID), "");
}

HWTEST_F(HitraceOptionTest, FilterAppTrace_001, TestSize.Level1)
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
    EXPECT_TRUE(ContainsPid(SET_EVENT_PID, 1));
    EXPECT_TRUE(ContainsPid(DEBUG_SET_EVENT_PID, 1));
}

HWTEST_F(HitraceOptionTest, AddNoFilterEvents001, TestSize.Level1)
{
    if (IsHm()) {
        std::vector<std::string> events = {"binder:binder_transaction"};
        int32_t ret = AddNoFilterEvents(events);
        EXPECT_EQ(ret, HITRACE_NO_ERROR);
        EXPECT_TRUE(ContainsEvents(NO_FILTER_EVENT, "binder:binder_transaction"));
        EXPECT_TRUE(ContainsEvents(DEBUG_NO_FILTER_EVENT, "binder:binder_transaction"));
        ret = ClearNoFilterEvents();
        EXPECT_EQ(ret, HITRACE_NO_ERROR);
    }
}

HWTEST_F(HitraceOptionTest, AddNoFilterEvents002, TestSize.Level1)
{
    if (IsHm()) {
        std::vector<std::string> events = {"binder:binder_transaction", "binder:binder_transaction_received"};
        int32_t ret = AddNoFilterEvents(events);
        EXPECT_EQ(ret, HITRACE_NO_ERROR);
        EXPECT_TRUE(ContainsEvents(NO_FILTER_EVENT, "binder:binder_transaction"));
        EXPECT_TRUE(ContainsEvents(DEBUG_NO_FILTER_EVENT, "binder:binder_transaction"));
        EXPECT_TRUE(ContainsEvents(NO_FILTER_EVENT, "binder:binder_transaction_received"));
        EXPECT_TRUE(ContainsEvents(DEBUG_NO_FILTER_EVENT, "binder:binder_transaction_received"));
        ret = ClearNoFilterEvents();
        EXPECT_EQ(ret, HITRACE_NO_ERROR);
        EXPECT_EQ(ReadFile(NO_FILTER_EVENT), "");
        EXPECT_EQ(ReadFile(DEBUG_NO_FILTER_EVENT), "");
    }
}

} // namespace HitraceTest
} // namespace HiviewDFX
} // namespace OHOS
