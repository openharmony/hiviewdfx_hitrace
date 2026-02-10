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
#include <cstring>
#include <string>
#include "trace_file_utils.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class TraceFileUtilsTest : public testing::Test {};

/**
 * @tc.name: TraverseFiles01
 * @tc.desc: Test TraverseFiles(), enter an existing file path.
 * @tc.type: FUNC
*/
HWTEST_F(TraceFileUtilsTest, TraverseFiles01, TestSize.Level2)
{
    EXPECT_FALSE(Hitrace::TraverseFiles("///", false, nullptr));
    EXPECT_FALSE(Hitrace::TraverseFiles("", false, nullptr));
    constexpr auto testPath = "/data/test/";
    EXPECT_TRUE(Hitrace::TraverseFiles("/data/test/", true,
        [testPath] (const char * directory, const dirent* item) {
            EXPECT_EQ(strncmp(directory, testPath, strlen(testPath) - 1), 0);
            EXPECT_NE(item->d_name, nullptr);
        }));
}
} // namespace HiviewDFX
} // namespace OHOS
