/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "hitrace_util.h"

#include <thread>
#include <gtest/gtest.h>
#include "gtest/gtest-message.h"
#include "gtest/gtest-test-part.h"
#include "gtest/gtest_pred_impl.h"
#include "gtest/hwext/gtest-tag.h"

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;

class HiTraceUtilTest : public testing::Test {};

/**
 * @tc.name: StoppableThreadHelperTest_001
 * @tc.desc: Start a sub thread and stop it when the subthread is waiting.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceUtilTest, StoppableThreadHelperTest_001, TestSize.Level1)
{
    StoppableThreadHelper helper;
    ASSERT_TRUE(helper.StartSubThread([] {
        return true;
    }, 1, "testSubThread1"));
    constexpr auto waitTime = 500;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    ASSERT_TRUE(helper.StopSubThread());
}

/**
 * @tc.name: StoppableThreadHelperTest_002
 * @tc.desc: Start a sub thread and stop it when the subthread is executing task.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceUtilTest, StoppableThreadHelperTest_002, TestSize.Level1)
{
    StoppableThreadHelper helper;
    ASSERT_TRUE(helper.StartSubThread([] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    }, 1, "testSubThread2"));
    constexpr auto waitTime = 500;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    ASSERT_TRUE(helper.StopSubThread());
}

/**
 * @tc.name: StoppableThreadHelperTest_003
 * @tc.desc: Stop a sub thread when the thread is exited.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA0
 */
HWTEST_F(HiTraceUtilTest, StoppableThreadHelperTest_003, TestSize.Level1)
{
    StoppableThreadHelper helper;
    ASSERT_TRUE(helper.StartSubThread([] {
        return false;
    }, 1, "testSubThread2"));
    constexpr auto waitTime = 500;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    ASSERT_FALSE(helper.StopSubThread());
}

/**
 * @tc.name: StoppableThreadHelperTest_004
 * @tc.desc: Start a sub thread when there is already a sub thread running.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA0
 */
HWTEST_F(HiTraceUtilTest, StoppableThreadHelperTest_004, TestSize.Level1)
{
    StoppableThreadHelper helper;
    ASSERT_FALSE(helper.StartSubThread(nullptr, 1, "testSubThread2"));
    auto task = [] {
        return true;
    };
    ASSERT_TRUE(helper.StartSubThread(task, 1, "testSubThread2"));
    ASSERT_FALSE(helper.StartSubThread(task, 1, "testSubThread2"));
}
}
}