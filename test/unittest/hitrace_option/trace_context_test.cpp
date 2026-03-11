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
#include "common_utils.h"
#include "hitrace_option/hitrace_option.h"
#include "hitrace_option/trace_context.h"

#include <unistd.h>
#include <gtest/gtest.h>

#include "trace_file_utils.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX::Hitrace;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {

class HitraceContextTest : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
};

void HitraceContextTest::SetUp()
{
    TraceContextManager::GetInstance().GetTraceFilterContext(true);
}

void HitraceContextTest::TearDown()
{
    TraceContextManager::GetInstance().ReleaseContext();
}

HWTEST_F(HitraceContextTest, HitraceContextTest001, TestSize.Level1)
{
    EXPECT_NE(TraceContextManager::GetInstance().GetTraceFilterContext(), nullptr);
    TraceContextManager::GetInstance().ReleaseContext();
    EXPECT_EQ(TraceContextManager::GetInstance().GetTraceFilterContext(), nullptr);
    EXPECT_NE(TraceContextManager::GetInstance().GetTraceFilterContext(true), nullptr);
}

HWTEST_F(HitraceContextTest, HitraceContextTest002, TestSize.Level1)
{
    auto filterTraceContext = TraceContextManager::GetInstance().GetTraceFilterContext();
    ASSERT_NE(filterTraceContext, nullptr);
    std::vector<std::string> filteredPids;
    ASSERT_FALSE(filterTraceContext->AddFilterPids(filteredPids));
    filteredPids.emplace_back(std::to_string(getpid()));
    ASSERT_TRUE(filterTraceContext->AddFilterPids(filteredPids));
}

HWTEST_F(HitraceContextTest, HitraceContextTest003, TestSize.Level1)
{
    auto filterTraceContext = TraceContextManager::GetInstance().GetTraceFilterContext();
    ASSERT_NE(filterTraceContext, nullptr);
    std::vector<std::string> filteredPids;
    ASSERT_FALSE(filterTraceContext->AddFilterPids(filteredPids));
    ASSERT_FALSE(filterTraceContext->AddFilterPids({ "unknow "}));
    filteredPids.emplace_back(std::to_string(getpid()));
    ASSERT_TRUE(filterTraceContext->AddFilterPids(filteredPids));
}

HWTEST_F(HitraceContextTest, HitraceContextTest004, TestSize.Level1)
{
    auto filterTraceContext = TraceContextManager::GetInstance().GetTraceFilterContext();
    ASSERT_NE(filterTraceContext, nullptr);
    ASSERT_TRUE(filterTraceContext->AddFilterPids({ std::to_string(getpid()) }));
    std::this_thread::sleep_for(std::chrono::seconds(4));
    filterTraceContext->FilterTraceContent();
    filterTraceContext->TraverseFilterPid({});
    filterTraceContext->TraverseFilterPid([] (const std::string& pid) {
        GTEST_LOG_(INFO) << "FilterPid: " << pid;
    });
    filterTraceContext->TraverseTGidsContent({});
    filterTraceContext->TraverseTGidsContent([] (const std::pair<std::string, std::string>& tgid) {
        GTEST_LOG_(INFO) << "FilterTGids: tid " << tgid.first  << " pid " << tgid.second;
    });
    filterTraceContext->TraverseSavedCmdLine({});
    filterTraceContext->TraverseSavedCmdLine([] (const std::string& cmdline) {
        GTEST_LOG_(INFO) << "FilterCmdLine" << cmdline;
    });
    ASSERT_TRUE(true);
}
} // namespace HitraceTest
} // namespace HiviewDFX
} // namespace OHOS
