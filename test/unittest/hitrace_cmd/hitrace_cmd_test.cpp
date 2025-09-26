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
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_cmd.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX::Hitrace;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
class HitraceCMDTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void)
    {
        RunCmd("hitrace --trace_finish_nodump");
        RunCmd("hitrace --trace_finish --record");
        RunCmd("hitrace --stop_bgsrv");
    }

    void SetUp()
    {
        Reset();
        originalCoutBuf = std::cout.rdbuf();
        std::cout.rdbuf(coutBuffer.rdbuf());
        RunCmd("hitrace --trace_finish_nodump");
        RunCmd("hitrace --trace_finish --record");
        RunCmd("hitrace --stop_bgsrv");
        coutBuffer.str("");
    }

    void TearDown()
    {
        std::cout.rdbuf(originalCoutBuf);
    }

    std::string GetOutput()
    {
        std::string output = coutBuffer.str();
        coutBuffer.str("");
        return output;
    }

    static void RunCmd(const string& cmd)
    {
        std::vector<std::string> args;
        std::stringstream ss(cmd);
        std::string arg;

        while (ss >> arg) {
            args.push_back(arg);
        }
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }

        (void)HiTraceCMDTestMain(static_cast<int>(args.size()), argv.data());
        Reset();
    }

    bool CheckTraceCommandOutput(const string& cmd, const std::vector<std::string>& keywords)
    {
        RunCmd(cmd);

        std::string output = GetOutput();
        int matchNum = 0;
        for (auto& keyword : keywords) {
            if (output.find(keyword) == std::string::npos) {
                GTEST_LOG_(INFO) << "command:" << cmd;
                GTEST_LOG_(INFO) << "command output:" << output;
                break;
            } else {
                matchNum++;
            }
        }
        return matchNum == keywords.size();
    }
private:
    std::streambuf* originalCoutBuf;
    std::stringstream coutBuffer;
};

namespace {
std::string ReadBufferSizeKB()
{
    std::ifstream file(TRACEFS_DIR + "buffer_size_kb");
    if (!file.is_open()) {
        GTEST_LOG_(ERROR) << "Failed to open buffer_size_kb";
        return "Unknown";
    }
    std::string line;
    if (std::getline(file, line)) {
        GTEST_LOG_(INFO) << "Reading buffer_size_kb: " << line;
        return line;
    }
    return "Unknown";
}
}

/**
 * @tc.name: HitraceCMDTest001
 * @tc.desc: test --trace_level command with correct parameters
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest001: start.";

    std::string cmd = "hitrace --trace_level I";
    std::vector<std::string> keywords = {
        "SET_TRACE_LEVEL",
        "success to set trace level",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest001: end.";
}

/**
 * @tc.name: HitraceCMDTest002
 * @tc.desc: test --trace_level command with wrong parameters
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest002: start.";

    std::string cmd = "hitrace --trace_level K";
    std::vector<std::string> keywords = {
        "error: trace level is illegal input",
        "parsing args failed",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest002: end.";
}

/**
 * @tc.name: HitraceCMDTest003
 * @tc.desc: test --get_level command when the value of level is normal
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest003: start.";

    std::string cmd = "hitrace --trace_level I";
    std::vector<std::string> keywords = {
        "SET_TRACE_LEVEL",
        "success to set trace level",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    cmd = "hitrace --get_level";
    keywords = {
        "GET_TRACE_LEVEL",
        "the current trace level threshold is Info",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest003: end.";
}

/**
 * @tc.name: HitraceCMDTest004
 * @tc.desc: test --get_level command when the value of level is abnormal
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest004: start.";

    constexpr int invalidLevel = -1;
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(invalidLevel)));

    std::string cmd = "hitrace --get_level";
    std::vector<std::string> keywords = {
        "GET_TRACE_LEVEL",
        "error: get trace level threshold failed, level(-1) cannot be parsed",
    };
    EXPECT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    constexpr int infoLevel = 1;
    ASSERT_TRUE(SetPropertyInner(TRACE_LEVEL_THRESHOLD, std::to_string(infoLevel)));

    GTEST_LOG_(INFO) << "HitraceCMDTest004: end.";
}

/**
 * @tc.name: HitraceCMDTest005
 * @tc.desc: test the normal custom buffer size in recording mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest005: start.";

    std::string cmd = "hitrace --trace_begin --record sched -b 102400";
    std::vector<std::string> keywords = {
        "RECORDING_LONG_BEGIN_RECORD",
        "tags:sched",
        "bufferSize:102400",
        "trace capturing",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
    ASSERT_EQ(ReadBufferSizeKB(), IsHmKernel() ? "102400" : "102402");

    GTEST_LOG_(INFO) << "HitraceCMDTest005: end.";
}

/**
 * @tc.name: HitraceCMDTest006
 * @tc.desc: test the lower limit of the custom buffer size in recording mode in hm kernel: [256, 1048576]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest006: start.";

    if (IsHmKernel()) {
        std::string cmd = "hitrace --trace_begin --record sched -b 255";
        std::vector<std::string> keywords = {
            "buffer size must be from 256 KB to 1024 MB",
            "parsing args failed, exit",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

        cmd = "hitrace --trace_begin --record sched -b 256";
        keywords = {
            "RECORDING_LONG_BEGIN_RECORD",
            "tags:sched",
            "bufferSize:256",
            "trace capturing",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
        ASSERT_EQ(ReadBufferSizeKB(), "512");
    }

    GTEST_LOG_(INFO) << "HitraceCMDTest006: end.";
}

/**
 * @tc.name: HitraceCMDTest007
 * @tc.desc: test the upper limit of the custom buffer size in recording mode in hm kernel: [256, 1048576]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest007, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest007: start.";

    if (IsHmKernel()) {
        std::string cmd = "hitrace --trace_begin --record sched -b 1048577";
        std::vector<std::string> keywords = {
            "buffer size must be from 256 KB to 1024 MB",
            "parsing args failed, exit",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

        cmd = "hitrace --trace_begin --record sched -b 1048576";
        keywords = {
            "RECORDING_LONG_BEGIN_RECORD",
            "tags:sched",
            "bufferSize:1048576",
            "trace capturing",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
        ASSERT_EQ(ReadBufferSizeKB(), "1048576");
    }

    GTEST_LOG_(INFO) << "HitraceCMDTest007: end.";
}

/**
 * @tc.name: HitraceCMDTest008
 * @tc.desc: test the lower limit of the custom buffer size in recording mode in linux kernel:[256, 307200]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest008: start.";

    if (!IsHmKernel()) {
        std::string cmd = "hitrace --trace_begin --record sched -b 255";
        std::vector<std::string> keywords = {
            "buffer size must be from 256 KB to 300 MB",
            "parsing args failed, exit",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

        cmd = "hitrace --trace_begin --record sched -b 256";
        keywords = {
            "RECORDING_LONG_BEGIN_RECORD",
            "tags:sched",
            "bufferSize:256",
            "trace capturing",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
        ASSERT_EQ(ReadBufferSizeKB(), "258");
    }

    GTEST_LOG_(INFO) << "HitraceCMDTest008: end.";
}

/**
 * @tc.name: HitraceCMDTest009
 * @tc.desc: test the upper limit of the custom buffer size in recording mode in linux kernel:[256, 307200]
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest009, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest009: start.";

    if (!IsHmKernel()) {
        std::string cmd = "hitrace --trace_begin --record sched -b 307201";
        std::vector<std::string> keywords = {
            "buffer size must be from 256 KB to 300 MB",
            "parsing args failed, exit",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

        cmd = "hitrace --trace_begin --record sched -b 307200";
        keywords = {
            "RECORDING_LONG_BEGIN_RECORD",
            "tags:sched",
            "bufferSize",
            "trace capturing",
        };
        ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
    }

    GTEST_LOG_(INFO) << "HitraceCMDTest009: end.";
}

/**
 * @tc.name: HitraceCMDTest010
 * @tc.desc: test the abnormal custom buffer size in recording mode
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest010, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest010: start.";

    std::string cmd = "hitrace --trace_begin ace -b abc";
    std::vector<std::string> keywords = {
        "buffer size is illegal input",
        "parsing args failed, exit",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest010: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest011, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest011: start.";

    std::string cmd = "hitrace --help";
    std::vector<std::string> keywords = {
        "SHOW_HELP",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest011: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest012, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest012: start.";

    std::string cmd = "hitrace --list_categories";
    std::vector<std::string> keywords = {
        "SHOW_LIST_CATEGORY",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest012: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest013, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest013: start.";

    std::string cmd = 
        "hitrace --raw --time 5 --file_size 51200 --buffer_size 102400 --trace_clock boot --overwrite ace app ability";
    std::vector<std::string> keywords = {
        "RECORDING_SHORT_RAW",
        "start capture",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest013: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest014, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest014: start.";

    std::string cmdStart = "hitrace --start_bgsrv";
    std::vector<std::string> keywordsStart = {
        "SNAPSHOT_START",
        "OpenSnapshot done",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmdStart, keywordsStart));

    std::string cmdDump = "hitrace --dump_bgsrv";
    std::vector<std::string> keywordsDump = {
        "SNAPSHOT_DUMP",
        "DumpSnapshot done",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmdDump, keywordsDump));

    std::string cmdStop = "hitrace --stop_bgsrv";
    std::vector<std::string> keywordsStop = {
        "SNAPSHOT_STOP",
        "CloseSnapshot done"
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmdStop, keywordsStop));

    GTEST_LOG_(INFO) << "HitraceCMDTest014: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest015, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest015: start.";

    std::string cmdStart = "hitrace --trace_begin app ace -b 102400 --overwrite --trace_clock boot";
    std::vector<std::string> keywordsStart = {
        "RECORDING_LONG_BEGIN",
        "OpenRecording done",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmdStart, keywordsStart));

    std::string cmdDump = "hitrace --trace_dump --output /data/local/tmp/testtrace.txt";
    std::vector<std::string> keywordsDump = {
        "start to read trace",
        "trace read done",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmdDump, keywordsDump));

    std::string cmdStop = "hitrace --trace_finish_nodump";
    std::vector<std::string> keywordsStop = {
        "RECORDING_LONG_FINISH_NODUMP",
        "end capture trace"
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmdStop, keywordsStop));

    GTEST_LOG_(INFO) << "HitraceCMDTest015: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest016, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest016: start.";

    std::string cmd = "hitrace TEST";
    std::vector<std::string> keywords = {
        "not support category",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest016: end.";
}

HWTEST_F(HitraceCMDTest, HitraceCMDTest017, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest017: start.";

    std::string cmd = "hitrace m";
    std::vector<std::string> keywords = {
        "not support category",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest017: end.";
}
}
}
}
