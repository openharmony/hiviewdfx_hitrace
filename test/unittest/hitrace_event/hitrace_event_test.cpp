/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <ctime>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_event_listener.h"
#include "hisysevent_manager.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
static std::string g_traceRootPath;
const int TRACING_ON_CLOSED = 2003;

class HitraceEventTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

void HitraceEventTest::SetUpTestCase()
{
    if (access((DEBUGFS_TRACING_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        g_traceRootPath = DEBUGFS_TRACING_DIR;
    } else if (access((TRACEFS_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        g_traceRootPath = TRACEFS_DIR;
    } else {
        GTEST_LOG_(INFO) << "Error: Finding trace folder failed";
    }
}

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

/**
 * @tc.name: HitraceEventTest001
 * @tc.desc: test Report DumpTextTrace HitraceEvent in HandleRecordingLongFinish with tracing_on closed
 * @tc.type: FUNC
 */
HWTEST_F(HitraceEventTest, HitraceEventTest001, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv >> /data/local/tmp/test.txt"));
    TraceSysEventParams traceEventParams;
    traceEventParams.caller = "CMD";
    traceEventParams.opt = "DumpTextTrace";
    traceEventParams.errorCode = TRACING_ON_CLOSED;
    traceEventParams.errorMessage = "Warning: tracing on is closed, no trace can be read.";
    std::shared_ptr<HitraceEventListener> traceListener = std::make_shared<HitraceEventListener>();
    ListenerRule tagRule("PROFILER", "HITRACE_USAGE", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(traceListener, sysRules);
    traceListener->SetEvent(traceEventParams);
    ASSERT_TRUE(RunCmd("hitrace --trace_finish -o /data/local/tmp/test.ftrace"));
    ASSERT_TRUE(traceListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(traceListener);
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv >> /data/local/tmp/test.txt"));
}

/**
 * @tc.name: HitraceEventTest002
 * @tc.desc: test Report DumpTextTrace HitraceEvent in HandleRecordingLongFinish with tracing_on opened
 * @tc.type: FUNC
 */
HWTEST_F(HitraceEventTest, HitraceEventTest002, TestSize.Level2)
{
    ASSERT_TRUE(RunCmd("hitrace --trace_begin app >> /data/local/tmp/test.txt"));
    TraceSysEventParams traceEventParams;
    traceEventParams.caller = "CMD";
    traceEventParams.opt = "DumpTextTrace";
    std::shared_ptr<HitraceEventListener> traceListener = std::make_shared<HitraceEventListener>();
    ListenerRule tagRule("PROFILER", "HITRACE_USAGE", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(traceListener, sysRules);
    traceListener->SetEvent(traceEventParams);
    ASSERT_TRUE(RunCmd("hitrace --trace_finish -o /data/local/tmp/test.ftrace"));
    ASSERT_TRUE(traceListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(traceListener);
}

/**
 * @tc.name: HitraceEventTest003
 * @tc.desc: test Report DumpTextTrace HitraceEvent in HandleRecordingShortText
 * @tc.type: FUNC
 */
HWTEST_F(HitraceEventTest, HitraceEventTest003, TestSize.Level2)
{
    TraceSysEventParams traceEventParams;
    traceEventParams.caller = "CMD";
    traceEventParams.opt = "DumpTextTrace";
    std::shared_ptr<HitraceEventListener> traceListener = std::make_shared<HitraceEventListener>();
    ListenerRule tagRule("PROFILER", "HITRACE_USAGE", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(traceListener, sysRules);
    traceListener->SetEvent(traceEventParams);
    ASSERT_TRUE(RunCmd("hitrace -t 5 app -o /data/local/tmp/test.ftrace"));
    ASSERT_TRUE(traceListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(traceListener);
}

/**
 * @tc.name: HitraceEventTest004
 * @tc.desc: test Report ShowListCategory HitraceEvent
 * @tc.type: FUNC
 */
HWTEST_F(HitraceEventTest, HitraceEventTest004, TestSize.Level2)
{
    TraceSysEventParams traceEventParams;
    traceEventParams.caller = "CMD";
    traceEventParams.opt = "ShowListCategory";
    std::shared_ptr<HitraceEventListener> traceListener = std::make_shared<HitraceEventListener>();
    ListenerRule tagRule("PROFILER", "HITRACE_USAGE", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(traceListener, sysRules);
    traceListener->SetEvent(traceEventParams);
    ASSERT_TRUE(RunCmd("hitrace -l >> /data/local/tmp/showlist.txt"));
    ASSERT_TRUE(traceListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(traceListener);
}

/**
 * @tc.name: HitraceEventTest005
 * @tc.desc: test Report ShowHelp HitraceEvent
 * @tc.type: FUNC
 */
HWTEST_F(HitraceEventTest, HitraceEventTest005, TestSize.Level2)
{
    TraceSysEventParams traceEventParams;
    traceEventParams.caller = "CMD";
    traceEventParams.opt = "ShowHelp";
    std::shared_ptr<HitraceEventListener> traceListener = std::make_shared<HitraceEventListener>();
    ListenerRule tagRule("PROFILER", "HITRACE_USAGE", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(traceListener, sysRules);
    traceListener->SetEvent(traceEventParams);
    ASSERT_TRUE(RunCmd("hitrace --help >> /data/local/tmp/showhelp.txt"));
    ASSERT_TRUE(traceListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(traceListener);
}
} // namespace HiviewDFX
} // namespace OHOS
