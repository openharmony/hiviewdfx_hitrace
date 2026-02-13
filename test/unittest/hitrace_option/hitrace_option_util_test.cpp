/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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


#include <gtest/gtest.h>

#include "hitrace_option_util.h"

#include <cstring>
#include <set>
#include <thread>

using namespace testing::ext;
using namespace OHOS::HiviewDFX::Hitrace;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
constexpr auto SET_EVENT_PID = "set_event_pid";

class HitraceOptionUtilTest : public testing::Test {
protected:
    const std::string& traceRootPath_ = GetTraceRootPath();
};

HWTEST_F(HitraceOptionUtilTest, HitraceOptionUtilTest001, TestSize.Level1)
{
    ASSERT_TRUE(ClearTracePoint(SET_EVENT_PID));
    auto parentPidStr = std::to_string(getpid());
    ASSERT_TRUE(AppendTracePoint(SET_EVENT_PID, parentPidStr));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ASSERT_TRUE(TraverseFileLineByLine(traceRootPath_ + SET_EVENT_PID,
        [] (const char* lineContent, size_t lineNum) {
            return false;
        }));
    std::set<std::string> setEventPidContent;
    ASSERT_TRUE(TraverseFileLineByLine(traceRootPath_ + SET_EVENT_PID,
        [&setEventPidContent] (const char* lineContent, size_t lineNum) {
            GTEST_LOG_(INFO) << "lineNum: " << lineNum << " lineContent: " << lineContent;
            setEventPidContent.insert(std::string(lineContent, strlen(lineContent) - 1));
            return true;
        }));
    ASSERT_TRUE(setEventPidContent.find(parentPidStr) != setEventPidContent.end());
    ASSERT_TRUE(ClearTracePoint(SET_EVENT_PID));
}

HWTEST_F(HitraceOptionUtilTest, HitraceOptionUtilTest002, TestSize.Level1)
{
    ASSERT_FALSE(TraverseFileLineByLine(traceRootPath_, {}));
    ASSERT_FALSE(TraverseFileLineByLine("", {}));
    ASSERT_FALSE(TraverseFileLineByLine(traceRootPath_, [] (const char* lineContent, size_t lineNum) {
        return false;
    }));
}

HWTEST_F(HitraceOptionUtilTest, HitraceOptionUtilTest003, TestSize.Level1)
{
    std::vector<std::string> subTids;
    ASSERT_FALSE(GetSubThreadIds(0, subTids));
    ASSERT_FALSE(GetSubThreadIds("unknow", subTids));
    ASSERT_TRUE(GetSubThreadIds(getpid(), subTids));
    ASSERT_GE(subTids.size(), 1);
}

HWTEST_F(HitraceOptionUtilTest, HitraceOptionUtilTest004, TestSize.Level1)
{
    ASSERT_FALSE(AppendTracePoint("unknow", ""));
}

HWTEST_F(HitraceOptionUtilTest, HitraceOptionUtilTest005, TestSize.Level1)
{
    ASSERT_FALSE(ClearTracePoint("unknow"));
}
} // namespace HitraceTest
} // namespace HiviewDFX
} // namespace OHOS
