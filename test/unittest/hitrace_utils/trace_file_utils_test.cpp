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
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "trace_file_utils.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class TraceFileUtilsTest : public testing::Test {
public:
    void SetUp() override
    {
        FILE* fp = fopen("/data/local/tmp/iswritable_test_file.txt", "w");
        if (fp != nullptr) {
            fclose(fp);
        }
        constexpr auto mode = 0755;
        mkdir("/data/local/tmp/subdir", mode);
        mkdir("/data/local/tmpX_test", mode);
    }
    void TearDown() override
    {
        (void)remove("/data/local/tmp/iswritable_test_file.txt");
        rmdir("/data/local/tmp/subdir");
        rmdir("/data/local/tmpX_test");
    }
};

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

/**
 * @tc.name: IsWritable01
 * @tc.desc: Test IsWritable() with paths covering realpath-fail branches via parentDir resolution.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritable01, TestSize.Level2)
{
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/test.txt"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/"));

    ASSERT_FALSE(Hitrace::IsWritable("/system/bin/test.txt"));
    ASSERT_FALSE(Hitrace::IsWritable("/data/local/tmp/../test.txt"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/./test.txt"));
    ASSERT_FALSE(Hitrace::IsWritable("/data/local/tmp/.."));
}

/**
 * @tc.name: IsWritable04
 * @tc.desc: Test IsWritable() with existing file under writable path, non-existent file in
 *           writable subdirectory, path with prefix collision, and filename containing "..".
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritable04, TestSize.Level2)
{
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/iswritable_test_file.txt"));
    ASSERT_FALSE(Hitrace::IsWritable("/data/local/tmpX_test"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/subdir/nonexist.txt"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/..hidden"));
}

/**
 * @tc.name: IsWritable05
 * @tc.desc: Test IsWritable() with symbolic link paths.
 *           realpath resolves symlinks to their target before prefix check.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritable05, TestSize.Level2)
{
    const char* linkToWritable = "/data/local/tmp/symlink_to_writable";
    EXPECT_EQ(symlink("/data/local/tmp/iswritable_test_file.txt", linkToWritable), 0);
    EXPECT_TRUE(Hitrace::IsWritable(linkToWritable));

    const char* linkToSystem = "/data/local/tmp/symlink_to_system";
    EXPECT_EQ(symlink("/system/bin", linkToSystem), 0);
    EXPECT_FALSE(Hitrace::IsWritable(linkToSystem));

    const char* danglingLink = "/data/local/tmp/dangling_symlink";
    EXPECT_EQ(symlink("/data/local/tmp/nonexist_target.txt", danglingLink), 0);
    EXPECT_TRUE(Hitrace::IsWritable(danglingLink));

    unlink(linkToWritable);
    unlink(linkToSystem);
    unlink(danglingLink);
}

/**
 * @tc.name: IsWritableDir01
 * @tc.desc: Test IsWritableDir(), enter an existing file path.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritableDir01, TestSize.Level2)
{
    ASSERT_TRUE(Hitrace::IsWritableDir("/data/local/tmp"));
    ASSERT_TRUE(Hitrace::IsWritableDir("/data/local/tmp/"));

    ASSERT_FALSE(Hitrace::IsWritableDir("/data/local/tmp/test.txt"));
    ASSERT_FALSE(Hitrace::IsWritableDir("/system/bin"));
}
} // namespace HiviewDFX
} // namespace OHOS
