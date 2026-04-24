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

#include <cerrno>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "common_define.h"
#include "common_utils.h"
#include "hitrace_cmd.h"
#include "parameters.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX::Hitrace;

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
class HitraceCMDTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        constexpr const char* kBootTraceLogDir = "/data/local/tmp";
        static constexpr mode_t kBootTraceLogDirMode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        struct stat st {};
        if (stat(kBootTraceLogDir, &st) == 0 && S_ISDIR(st.st_mode)) {
            return;
        }
        if (mkdir(kBootTraceLogDir, kBootTraceLogDirMode) != 0 && errno != EEXIST) {
            GTEST_LOG_(WARNING) << "mkdir " << kBootTraceLogDir << " failed errno=" << errno;
        }
    }
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
        GTEST_LOG_(INFO) << "command: " << cmd;
        GTEST_LOG_(INFO) << "command output: " << output;

        int matchNum = 0;
        for (auto& keyword : keywords) {
            if (output.find(keyword) == std::string::npos) {
                break;
            } else {
                matchNum++;
            }
        }
        return matchNum == keywords.size();
    }

    /** Run command and return stdout output for boundary tests (success/failure string checks). */
    std::string RunCmdAndGetOutput(const std::string& cmd)
    {
        RunCmd(cmd);
        std::string output = GetOutput();
        GTEST_LOG_(INFO) << "command: " << cmd;
        GTEST_LOG_(INFO) << "command output: " << output;
        return output;
    }

    /** Run command and return exit code (for boot-trace subcommand validation). */
    static int RunCmdWithExitCode(const std::string& cmd)
    {
        std::vector<std::string> args;
        std::stringstream ss(cmd);
        std::string arg;
        while (ss >> arg) {
            args.push_back(arg);
        }
        std::vector<char*> argv;
        for (auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        int ret = HiTraceCMDTestMain(static_cast<int>(args.size()), argv.data());
        Reset();
        return ret;
    }

private:
    std::streambuf* originalCoutBuf;
    std::stringstream coutBuffer;
};

namespace {
std::string ReadFileContent(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string ReadBufferSizeKB()
{
    std::ifstream file(std::string(TRACEFS_DIR) + "buffer_size_kb");
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

std::string ReadBootTraceConfig()
{
    const std::string path = std::string(BOOT_TRACE_CONFIG_DIR) + BOOT_TRACE_CONFIG_FILE;
    std::ifstream file(path);
    if (!file.is_open()) {
        GTEST_LOG_(INFO) << "boot trace config not existing: " << path;
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool WriteBootTraceConfig(const std::string& content)
{
    const std::string path = std::string(BOOT_TRACE_CONFIG_DIR) + BOOT_TRACE_CONFIG_FILE;
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return !file.fail();
}

void RemoveBootTraceConfig()
{
    const std::string path = std::string(BOOT_TRACE_CONFIG_DIR) + BOOT_TRACE_CONFIG_FILE;
    (void)remove(path.c_str());
}

void RemoveBootTraceOutput(const std::string& fileName)
{
    const std::string path = std::string(BOOT_TRACE_CONFIG_DIR) + fileName;
    (void)remove(path.c_str());
}

bool BootTraceConfigPathExists()
{
    const std::string path = std::string(BOOT_TRACE_CONFIG_DIR) + BOOT_TRACE_CONFIG_FILE;
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/** boot_trace / boot-trace CLI requires root euid in production; skip (pass) when running as shell user. */
void SkipBootTracePrivilegedTestsUnlessRoot()
{
    if (geteuid() != 0) {
        GTEST_SKIP() << "non-root euid: skip boot_trace/boot-trace privileged CLI tests (not a failure)";
    }
}

/* boot-trace capture requires kernel/userspace tag sections (matches --boot_trace written cfg shape) */
const char* const K_BOOT_TRACE_UT_CONFIG_MINIMAL = R"({"duration_sec":1,)"
    R"("kernel":{"enabled":true,"tags":["sched"],)"
    R"("buffer_size_kb":4096,"clock":"boot"},)"
    R"("userspace":{"enabled":false,"tags":[]}})";

const char* const K_BOOT_TRACE_UT_CONFIG_INCREMENT0 = R"({"duration_sec":1,"increment_index":0,)"
    R"("file_prefix":"boot_trace",)"
    R"("kernel":{"enabled":true,"tags":["sched"],)"
    R"("buffer_size_kb":4096,"clock":"boot"},)"
    R"("userspace":{"enabled":false,"tags":[]}})";

/* increment on; tag unknown to hitrace_utils so LoadBootTraceCaptureConfig fails before capture */
const char* const K_BOOT_TRACE_UT_CONFIG_INCREMENT_BAD_TAG = R"({"duration_sec":1,"increment_index":3,)"
    R"("file_prefix":"boot_trace",)"
    R"("kernel":{"enabled":true,"tags":["___boot_trace_ut_bad_tag___"],)"
    R"("buffer_size_kb":4096,"clock":"boot"},)"
    R"("userspace":{"enabled":false,"tags":[]}})";

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

/**
 * @tc.name: HitraceCMDTest011
 * @tc.desc: test the normal input --help
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: HitraceCMDTest012
 * @tc.desc: test the normal input --list_categories
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: HitraceCMDTest013
 * @tc.desc: test the normal longopt input time,file_size,buffer_size,trace_clock,overwritr
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest013, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest013: start.";

    std::string cmd =
        "hitrace --raw --time 1 --file_size 51200 --buffer_size 102400 --trace_clock boot --overwrite ace app ability";
    std::vector<std::string> keywords = {
        "RECORDING_SHORT_RAW",
        "start capture",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest013: end.";
}

/**
 * @tc.name: HitraceCMDTest014
 * @tc.desc: test normal trace dump in snapshot mode
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: HitraceCMDTest015
 * @tc.desc: test normal text trace dump with longopt input parameters
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest015, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest015: start.";

    std::string cmdStart = "hitrace --trace_begin app ace -b 102400 --overwrite --trace_clock mono";
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

/**
 * @tc.name: HitraceCMDTest016
 * @tc.desc: test the error longopt input
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest016, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest016: start.";

    std::string cmd = "hitrace --TEST";
    std::vector<std::string> keywords = {
        "parsing args failed",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest016: end.";
}

/**
 * @tc.name: HitraceCMDTest017
 * @tc.desc: test the error input
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest017, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest017: start.";

    std::string cmd = "hitrace -m";
    std::vector<std::string> keywords = {
        "parsing args failed",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest017: end.";
}

/**
 * @tc.name: HitraceCMDTest017
 * @tc.desc: test the buffer_size illegal input
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest018, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest018: start.";

    std::string cmd = "hitrace --trace_begin app ace -b 102400ss";
    std::vector<std::string> keywords = {
        "illegal input",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest018: end.";
}

/**
 * @tc.name: HitraceCMDTest017
 * @tc.desc: test the trace_clock illegal input
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest019, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HitraceCMDTest019: start.";

    std::string cmd = "hitrace --trace_begin app ace --trace_clock boott";
    std::vector<std::string> keywords = {
        "illegal input",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    GTEST_LOG_(INFO) << "HitraceCMDTest019: end.";
}

/**
 * @tc.name: HitraceCMDTest020
 * @tc.desc: test --boot_trace basic config with full parameters
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest020, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest020: start.";

    RemoveBootTraceConfig();

    std::string cmd = "hitrace --boot_trace -b 10240 -t 30 --file_prefix bootA sched irq";
    std::vector<std::string> keywords = {
        "CONFIG_BOOT_TRACE",
        "tags: sched irq",
        "buffer_size: 10240",
        "time: 30",
        "file_prefix: bootA",
        "boot_trace configuration success.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"duration_sec\": 30"), std::string::npos);
    EXPECT_NE(content.find("\"output\": \"/data/local/tmp/bootA_default.sys\""), std::string::npos);
    EXPECT_NE(content.find("\"file_size_kb\": 102400"), std::string::npos);
    EXPECT_EQ(content.find("\"max_file_count\""), std::string::npos);
    EXPECT_NE(content.find("\"increment_index\": -1"), std::string::npos);
    EXPECT_NE(content.find("\"file_prefix\": \"bootA\""), std::string::npos);
    EXPECT_NE(content.find("\"sched\""), std::string::npos);
    EXPECT_NE(content.find("\"irq\""), std::string::npos);
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, ""), "1") <<
        "debug.hitrace.boot_trace.count should be 1 (default repeat 1)";

    GTEST_LOG_(INFO) << "HitraceCMDTest020: end.";
}

/**
 * @tc.name: HitraceCMDTest021
 * @tc.desc: test --boot_trace default duration and repeat
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest021, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest021: start.";

    RemoveBootTraceConfig();

    std::string cmd = "hitrace --boot_trace -b 4096 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("CONFIG_BOOT_TRACE"), std::string::npos);
    EXPECT_NE(output.find("tags: sched"), std::string::npos);
    EXPECT_NE(output.find("buffer_size: 4096"), std::string::npos);
    EXPECT_NE(output.find("boot_trace configuration success."), std::string::npos);
    EXPECT_EQ(output.find("time: 30"), std::string::npos);
    EXPECT_EQ(output.find("file_prefix: boot_trace"), std::string::npos);
    EXPECT_EQ(output.find("repeat: 1"), std::string::npos);
    EXPECT_EQ(output.find("overwrite: true"), std::string::npos);

    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"duration_sec\": 30"), std::string::npos);
    EXPECT_NE(content.find("\"file_size_kb\": 102400"), std::string::npos);
    EXPECT_EQ(content.find("\"max_file_count\""), std::string::npos);
    EXPECT_NE(content.find("\"increment_index\": -1"), std::string::npos);
    EXPECT_NE(content.find("\"file_prefix\": \"boot_trace\""), std::string::npos);
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, ""), "1") <<
        "debug.hitrace.boot_trace.count should be 1";

    GTEST_LOG_(INFO) << "HitraceCMDTest021: end.";
}

/**
 * @tc.name: HitraceCMDTest022
 * @tc.desc: test --boot_trace without tag should fail and not change param or config
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest022, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest022: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(BOOT_TRACE_COUNT_PARAM, "0"));

    std::string cmd = "hitrace --boot_trace -b 10240";
    std::vector<std::string> keywords = {
        "boot_trace requires at least one tag.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    EXPECT_TRUE(ReadBootTraceConfig().empty());
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, "?"), "0") <<
        "BOOT_TRACE_COUNT_PARAM should remain 0 when config command fails";

    GTEST_LOG_(INFO) << "HitraceCMDTest022: end.";
}

/**
 * @tc.name: HitraceCMDTest023
 * @tc.desc: test --boot_trace repeat out of range
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest023, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest023: start.";

    RemoveBootTraceConfig();

    std::string cmd = "hitrace --boot_trace sched ohos ability";
    std::vector<std::string> keywords = {
        "CONFIG_BOOT_TRACE",
        "boot_trace configuration success.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"sched\""), std::string::npos);
    EXPECT_NE(content.find("\"ohos\""), std::string::npos);
    EXPECT_NE(content.find("\"ability\""), std::string::npos);
    EXPECT_NE(content.find("\"file_size_kb\": 102400"), std::string::npos);
    EXPECT_EQ(content.find("\"max_file_count\""), std::string::npos);
    EXPECT_NE(content.find("\"increment_index\": -1"), std::string::npos);

    GTEST_LOG_(INFO) << "HitraceCMDTest023: end.";
}

/**
 * @tc.name: HitraceCMDTest024
 * @tc.desc: test --boot_trace off to disable boot trace (persist.hitrace.boot_trace.count set to 0)
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest024, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest024: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_MINIMAL));
    ASSERT_TRUE(BootTraceConfigPathExists());
    ASSERT_TRUE(SetPropertyInner(BOOT_TRACE_COUNT_PARAM, "3"));
    std::string cmd = "hitrace --boot_trace off";
    std::vector<std::string> keywords = {
        "CONFIG_BOOT_TRACE",
        "boot_trace off success.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, "?"), "0") <<
        "persist.hitrace.boot_trace.count should be 0 after off";
    EXPECT_TRUE(BootTraceConfigPathExists()) << "--boot_trace off must not remove boot_trace.cfg";
    ASSERT_FALSE(ReadBootTraceConfig().empty()) << "cfg content should remain after off";

    RemoveBootTraceConfig();
    GTEST_LOG_(INFO) << "HitraceCMDTest024: end.";
}

/**
 * @tc.name: HitraceCMDTest025
 * @tc.desc: test --boot_trace time less than 1
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest025, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest025: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";
    const std::string failStr = "to be greater than zero";

    std::string cmd = "hitrace --boot_trace -t 0 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find(failStr), std::string::npos) << "expect failure message: " << failStr;
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when time invalid";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    cmd = "hitrace --boot_trace -t -1 sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find(failStr), std::string::npos) << "expect failure message: " << failStr;
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when time invalid";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    GTEST_LOG_(INFO) << "HitraceCMDTest025: end.";
}

/**
 * @tc.name: HitraceCMDTest026
 * @tc.desc: test --boot_trace buffer_size out of range
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest026, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest026: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";
    const std::string failStr = "buffer size must be from 256 KB";

    std::string cmd = "hitrace --boot_trace -b 128 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find(failStr), std::string::npos) << "expect failure message: " << failStr;
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when buffer_size out of range";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    GTEST_LOG_(INFO) << "HitraceCMDTest026: end.";
}

/**
 * @tc.name: HitraceCMDTest027
 * @tc.desc: test --boot_trace file_size out of range and illegal
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest027, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest027: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";

    // illegal string
    std::string cmd = "hitrace --boot_trace --file_size abc sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("file size is illegal input"), std::string::npos) << "expect illegal file_size message";
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when file_size illegal";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    // smaller than 50 MB
    cmd = "hitrace --boot_trace --file_size 10240 sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("file size must be from 50 MB to 500 MB"), std::string::npos) <<
        "expect file_size range message";
    EXPECT_EQ(output.find(successStr), std::string::npos) <<
        "expect no success when file_size too small";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    // larger than 500 MB
    cmd = "hitrace --boot_trace --file_size 600000 sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("file size must be from 50 MB to 500 MB"), std::string::npos) <<
        "expect file_size range message";
    EXPECT_EQ(output.find(successStr), std::string::npos) <<
        "expect no success when file_size too large";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    GTEST_LOG_(INFO) << "HitraceCMDTest027: end.";
}

/**
 * @tc.name: HitraceCMDTest028
 * @tc.desc: test --boot_trace file_size valid lower boundary
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest028, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest028: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";

    std::string cmd = "hitrace --boot_trace --file_size 51200 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("CONFIG_BOOT_TRACE"), std::string::npos) << "expect success state";
    EXPECT_NE(output.find(successStr), std::string::npos) << "expect success message";
    EXPECT_EQ(output.find("file size is illegal input"), std::string::npos) << "expect no file_size error";
    EXPECT_EQ(output.find("file size must be from 50 MB"), std::string::npos) << "expect no file_size range error";

    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"file_size_kb\": 51200"), std::string::npos);

    GTEST_LOG_(INFO) << "HitraceCMDTest028: end.";
}

/**
 * @tc.name: HitraceCMDTest029
 * @tc.desc: test --boot_trace --repeat boundary: 0 and 101 must fail
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest029, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest029: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";
    const std::string failStr = "--repeat must be from 1 to 100";

    std::string cmd = "hitrace --boot_trace --repeat 0 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find(failStr), std::string::npos) << "expect failure message for repeat 0";
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when repeat 0";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    RemoveBootTraceConfig();
    cmd = "hitrace --boot_trace --repeat 101 sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find(failStr), std::string::npos) << "expect failure message for repeat 101";
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when repeat 101";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    GTEST_LOG_(INFO) << "HitraceCMDTest029: end.";
}

/**
 * @tc.name: HitraceCMDTest030
 * @tc.desc: test --boot_trace --repeat illegal: negative and non-numeric
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest030, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest030: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";

    std::string cmd = "hitrace --boot_trace --repeat -1 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("--repeat must be from 1 to 100"), std::string::npos) <<
        "expect failure message for repeat -1";
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when repeat negative";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    RemoveBootTraceConfig();
    cmd = "hitrace --boot_trace --repeat abc sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("repeat is illegal input"), std::string::npos) << "expect illegal repeat message";
    EXPECT_EQ(output.find(successStr), std::string::npos) << "expect no success when repeat non-numeric";
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    GTEST_LOG_(INFO) << "HitraceCMDTest030: end.";
}

/**
 * @tc.name: HitraceCMDTest031
 * @tc.desc: test --boot_trace --repeat valid boundary 1 and 100
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest031, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest031: start.";

    RemoveBootTraceConfig();

    const std::string successStr = "boot_trace configuration success.";

    std::string cmd = "hitrace --boot_trace --repeat 1 sched";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("CONFIG_BOOT_TRACE"), std::string::npos) << "expect success state for repeat 1";
    EXPECT_NE(output.find(successStr), std::string::npos) << "expect success message for repeat 1";
    EXPECT_EQ(output.find("--repeat must be from 1 to 100"), std::string::npos) << "expect no repeat range error";
    EXPECT_EQ(output.find("repeat is illegal input"), std::string::npos) << "expect no repeat illegal error";
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, ""), "1") <<
        "debug.hitrace.boot_trace.count should be 1";

    RemoveBootTraceConfig();
    cmd = "hitrace --boot_trace --repeat 100 sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("CONFIG_BOOT_TRACE"), std::string::npos) << "expect success state for repeat 100";
    EXPECT_NE(output.find(successStr), std::string::npos) << "expect success message for repeat 100";
    EXPECT_EQ(output.find("--repeat must be from 1 to 100"), std::string::npos) << "expect no repeat range error";
    EXPECT_EQ(output.find("repeat is illegal input"), std::string::npos) << "expect no repeat illegal error";
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, ""), "100") <<
        "debug.hitrace.boot_trace.count should be 100";

    GTEST_LOG_(INFO) << "HitraceCMDTest031: end.";
}

/**
 * @tc.name: HitraceCMDTest031b
 * @tc.desc: test --boot_trace --repeat 3 writes persist and config has no remaining_count;
 *     --overwrite writes overwrite true
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest031b, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest031b: start.";

    RemoveBootTraceConfig();

    std::string cmd = "hitrace --boot_trace -b 10240 -t 30 --repeat 3 sched irq";
    std::string output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("boot_trace configuration success."), std::string::npos);
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, ""), "3") <<
        "debug.hitrace.boot_trace.count should be 3";

    std::string content = ReadBootTraceConfig();
    EXPECT_NE(content.find("\"duration_sec\": 30"), std::string::npos);
    EXPECT_NE(content.find("\"buffer_size_kb\": 10240"), std::string::npos);
    EXPECT_EQ(content.find("remaining_count"), std::string::npos) << "config should not contain remaining_count";

    RemoveBootTraceConfig();
    cmd = "hitrace --boot_trace --overwrite sched";
    output = RunCmdAndGetOutput(cmd);
    EXPECT_NE(output.find("boot_trace configuration success."), std::string::npos);
    content = ReadBootTraceConfig();
    EXPECT_NE(content.find("\"overwrite\": true"), std::string::npos) <<
        "config should contain overwrite: true when --overwrite";

    GTEST_LOG_(INFO) << "HitraceCMDTest031b: end.";
}

// ===================== hitrace boot-trace subcommand (stage 2) =====================

/**
 * @tc.name: HitraceCMDTest032
 * @tc.desc: boot-trace when config file does not exist -> exit 2, message "config not found"
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest032, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "active 0: hitrace sets active then fails load when config missing";
    EXPECT_NE(output.find("boot trace config not found"), std::string::npos) << output;

    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "1"));
    code = RunCmdWithExitCode("hitrace boot-trace");
    output = GetOutput();
    EXPECT_EQ(code, 0) << "active 1: duplicate launch, no config read";
    EXPECT_NE(output.find("duplicate launch ignored"), std::string::npos) << output;
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest033
 * @tc.desc: boot-trace when config file is empty -> exit 2, cfg file kept, message "empty"
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest033, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig(""));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "expect exit 2 when config empty";
    EXPECT_NE(output.find("empty"), std::string::npos) << output;
    EXPECT_TRUE(BootTraceConfigPathExists()) << "config path should remain (no unlink)";
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest034
 * @tc.desc: boot-trace when config is invalid JSON -> exit 2, cfg file kept, message "parse"
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest034, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig("{ invalid }"));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "expect exit 2 when JSON invalid";
    EXPECT_NE(output.find("parse"), std::string::npos) << output;
    EXPECT_TRUE(BootTraceConfigPathExists()) << "config path should remain (no unlink)";
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest035
 * @tc.desc: boot-trace when duration_sec is missing -> exit 2, cfg file kept
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest035, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig("{\"other\":1}"));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "expect exit 2 when duration_sec missing";
    EXPECT_NE(output.find("duration_sec is missing or invalid"), std::string::npos) << output;
    EXPECT_TRUE(BootTraceConfigPathExists()) << "config path should remain (no unlink)";
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest036
 * @tc.desc: boot-trace when duration_sec is 0 -> exit 2, cfg file kept
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest036, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig("{\"duration_sec\":0}"));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "expect exit 2 when duration_sec is 0";
    EXPECT_NE(output.find("duration_sec is missing or invalid"), std::string::npos) << output;
    EXPECT_TRUE(BootTraceConfigPathExists()) << "config path should remain (no unlink)";
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest037
 * @tc.desc: boot-trace when duration_sec is negative -> exit 2, cfg file kept
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest037, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig("{\"duration_sec\":-1}"));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "expect exit 2 when duration_sec is negative";
    EXPECT_NE(output.find("duration_sec is missing or invalid"), std::string::npos) << output;
    EXPECT_TRUE(BootTraceConfigPathExists()) << "config path should remain (no unlink)";
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest038
 * @tc.desc: boot-trace when duration_sec is not a number -> exit 2, cfg file kept
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest038, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig("{\"duration_sec\":\"abc\"}"));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2) << "expect exit 2 when duration_sec is not a number";
    EXPECT_NE(output.find("duration_sec is missing or invalid"), std::string::npos) << output;
    EXPECT_TRUE(BootTraceConfigPathExists()) << "config path should remain (no unlink)";
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest039
 * @tc.desc: boot-trace with valid config -> exit 0, config kept
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest039, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    RemoveBootTraceOutput("boot_trace_default.sys");
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_MINIMAL));
    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 0) << "expect exit 0 when config is valid";
    EXPECT_NE(output.find("boot_trace finished"), std::string::npos) << output;
    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty()) << "config should still exist";
    std::string traceContent = ReadFileContent(std::string(BOOT_TRACE_CONFIG_DIR) + "boot_trace_default.sys");
    EXPECT_FALSE(traceContent.empty()) << "boot-trace should dump non-empty output file";
    RemoveBootTraceOutput("boot_trace_default.sys");
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest040
 * @tc.desc: boot-trace does not change debug.hitrace.boot_trace.count (count managed by init)
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest040, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest040: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_MINIMAL));
    // Simulate init-side counter; hitrace boot-trace should NOT modify it.
    ASSERT_TRUE(SetPropertyInner(BOOT_TRACE_COUNT_PARAM, "5"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    EXPECT_EQ(code, 0) << "expect exit 0 for valid config";
    EXPECT_EQ(GetPropertyInner(BOOT_TRACE_COUNT_PARAM, ""), "5") <<
        "boot-trace should not change debug.hitrace.boot_trace.count";

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest041
 * @tc.desc: boot-trace is idempotent for valid config (multiple calls keep config)
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest041, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_MINIMAL));

    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
        int code = RunCmdWithExitCode("hitrace boot-trace");
        EXPECT_EQ(code, 0);
        std::string content = ReadBootTraceConfig();
        ASSERT_FALSE(content.empty()) << "config should still exist after call " << i;
    }

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest042
 * @tc.desc: boot-trace clears debug.hitrace.boot_trace.active flag when active
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest042, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest042: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_MINIMAL));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    EXPECT_EQ(GetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"), "0");

    int code = RunCmdWithExitCode("hitrace boot-trace");
    EXPECT_EQ(code, 0) << "expect exit 0 with valid config and active flag";
    EXPECT_EQ(GetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"), "0") <<
        "boot-trace should clear debug.hitrace.boot_trace.active back to 0";

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest043
 * @tc.desc: boot-trace clears active flag when capture setup fails
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest043, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    ASSERT_TRUE(WriteBootTraceConfig(
        R"({"duration_sec":1,"kernel":{"enabled":true,"tags":["invalid_tag"]},)"
        R"("userspace":{"enabled":false,"tags":[]}})"));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 2);
    EXPECT_NE(output.find("unsupported tag"), std::string::npos) << output;
    EXPECT_EQ(GetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"), "0") <<
        "boot-trace should clear active flag when capture config is invalid";

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest044
 * @tc.desc: boot-trace honors output path and overwrite-related fields from config
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest044, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    const std::string fileName = "boot_trace_custom.sys";
    RemoveBootTraceConfig();
    RemoveBootTraceOutput(fileName);
    const std::string customOut = std::string(BOOT_TRACE_CONFIG_DIR) + "boot_trace_custom.sys";
    const std::string cfgJson = std::string("{\"duration_sec\":1,\"output\":\"") + customOut +
        R"(","file_size_kb":51200,"overwrite":true,"kernel":{"enabled":true,"tags":["sched"],)"
        R"("buffer_size_kb":4096,"clock":"boot"},"userspace":{"enabled":false,"tags":[]}})";
    ASSERT_TRUE(WriteBootTraceConfig(cfgJson));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 0);
    EXPECT_NE(output.find("boot_trace: wrote"), std::string::npos) << output;
    std::string traceContent = ReadFileContent(std::string(BOOT_TRACE_CONFIG_DIR) + fileName);
    EXPECT_FALSE(traceContent.empty()) << "boot-trace should respect configured output path";

    RemoveBootTraceOutput(fileName);
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
}

/**
 * @tc.name: HitraceCMDTest047
 * @tc.desc: boot-trace with active 0 and valid cfg runs capture (manual launch path)
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest047, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    RemoveBootTraceConfig();
    RemoveBootTraceOutput("boot_trace_default.sys");
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_MINIMAL));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 0) << output;
    EXPECT_NE(output.find("boot_trace finished"), std::string::npos) << output;
    ASSERT_FALSE(ReadBootTraceConfig().empty()) << "config kept after capture";

    RemoveBootTraceOutput("boot_trace_default.sys");
    RemoveBootTraceConfig();
}

/**
 * @tc.name: HitraceCMDTest048
 * @tc.desc: test --boot_trace --increment writes increment_index 0 and output path with _0 suffix
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest048, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest048: start.";

    RemoveBootTraceConfig();

    std::string cmd = "hitrace --boot_trace -t 5 --increment sched";
    std::vector<std::string> keywords = {
        "CONFIG_BOOT_TRACE",
        "increment: true",
        "boot_trace configuration success.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"increment_index\": 0"), std::string::npos) << content;
    EXPECT_NE(content.find("\"output\": \"/data/local/tmp/boot_trace_default_0.sys\""), std::string::npos) << content;

    RemoveBootTraceConfig();
    GTEST_LOG_(INFO) << "HitraceCMDTest048: end.";
}

/**
 * @tc.name: HitraceCMDTest049
 * @tc.desc: boot-trace success bumps increment_index and updates output in cfg
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest049, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest049: start.";

    RemoveBootTraceConfig();
    RemoveBootTraceOutput("boot_trace_default_0.sys");
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_INCREMENT0));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 0) << output;
    EXPECT_NE(output.find("output=/data/local/tmp/boot_trace_default_0.sys"), std::string::npos) << output;

    std::string content = ReadBootTraceConfig();
    EXPECT_NE(content.find("\"increment_index\": 1"), std::string::npos) << content;
    EXPECT_NE(content.find("\"output\": \"/data/local/tmp/boot_trace_default_1.sys\""), std::string::npos) << content;

    RemoveBootTraceOutput("boot_trace_default_0.sys");
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    GTEST_LOG_(INFO) << "HitraceCMDTest049: end.";
}

/**
 * @tc.name: HitraceCMDTest050
 * @tc.desc: --increment before --boot_trace is rejected
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest050, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest050: start.";

    RemoveBootTraceConfig();
    std::string cmd = "hitrace --increment --boot_trace sched";
    std::vector<std::string> keywords = {
        "error: --increment only supports --boot_trace.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));
    EXPECT_TRUE(ReadBootTraceConfig().empty());

    GTEST_LOG_(INFO) << "HitraceCMDTest050: end.";
}

/**
 * @tc.name: HitraceCMDTest051
 * @tc.desc: test --boot_trace --increment with --file_prefix uses stem default_0 in output path
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest051, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest051: start.";

    RemoveBootTraceConfig();

    std::string cmd = "hitrace --boot_trace -t 5 --increment --file_prefix mypre sched";
    std::vector<std::string> keywords = {
        "CONFIG_BOOT_TRACE",
        "file_prefix: mypre",
        "increment: true",
        "boot_trace configuration success.",
    };
    ASSERT_TRUE(CheckTraceCommandOutput(cmd, keywords));

    std::string content = ReadBootTraceConfig();
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"file_prefix\": \"mypre\""), std::string::npos) << content;
    EXPECT_NE(content.find("\"increment_index\": 0"), std::string::npos) << content;
    EXPECT_NE(content.find("\"output\": \"/data/local/tmp/mypre_default_0.sys\""), std::string::npos) << content;

    RemoveBootTraceConfig();
    GTEST_LOG_(INFO) << "HitraceCMDTest051: end.";
}

/**
 * @tc.name: HitraceCMDTest052
 * @tc.desc: boot-trace duplicate launch does not bump increment_index
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest052, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest052: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_INCREMENT0));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "1"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    std::string output = GetOutput();
    EXPECT_EQ(code, 1) << output;
    EXPECT_NE(output.find("duplicate launch ignored"), std::string::npos) << output;

    std::string content = ReadBootTraceConfig();
    EXPECT_NE(content.find("\"increment_index\": 0"), std::string::npos) << content;
    EXPECT_NE(content.find("\"output\": \"/data/local/tmp/boot_trace_default_0.sys\""), std::string::npos) << content;

    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    RemoveBootTraceConfig();
    GTEST_LOG_(INFO) << "HitraceCMDTest052: end.";
}

/**
 * @tc.name: HitraceCMDTest053
 * @tc.desc: two successful boot-trace runs bump increment_index 0 -> 1 -> 2 and output paths
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest053, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest053: start.";

    RemoveBootTraceConfig();
    RemoveBootTraceOutput("boot_trace_default_0.sys");
    RemoveBootTraceOutput("boot_trace_default_1.sys");
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_INCREMENT0));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));

    int code1 = RunCmdWithExitCode("hitrace boot-trace");
    EXPECT_EQ(code1, 0) << GetOutput();
    std::string after1 = ReadBootTraceConfig();
    EXPECT_NE(after1.find("\"increment_index\": 1"), std::string::npos) << after1;
    EXPECT_NE(after1.find("\"output\": \"/data/local/tmp/boot_trace_default_1.sys\""), std::string::npos) << after1;

    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    int code2 = RunCmdWithExitCode("hitrace boot-trace");
    EXPECT_EQ(code2, 0) << GetOutput();
    std::string after2 = ReadBootTraceConfig();
    EXPECT_NE(after2.find("\"increment_index\": 2"), std::string::npos) << after2;
    EXPECT_NE(after2.find("\"output\": \"/data/local/tmp/boot_trace_default_2.sys\""), std::string::npos) << after2;

    RemoveBootTraceOutput("boot_trace_default_0.sys");
    RemoveBootTraceOutput("boot_trace_default_1.sys");
    RemoveBootTraceOutput("boot_trace_default_2.sys");
    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    GTEST_LOG_(INFO) << "HitraceCMDTest053: end.";
}

/**
 * @tc.name: HitraceCMDTest054
 * @tc.desc: boot-trace load failure (bad tag) does not bump increment_index
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest054, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest054: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(WriteBootTraceConfig(K_BOOT_TRACE_UT_CONFIG_INCREMENT_BAD_TAG));
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));

    int code = RunCmdWithExitCode("hitrace boot-trace");
    EXPECT_NE(code, 0) << "expect non-zero when config tags are unsupported";
    std::string content = ReadBootTraceConfig();
    EXPECT_NE(content.find("\"increment_index\": 3"), std::string::npos) << content;

    RemoveBootTraceConfig();
    ASSERT_TRUE(SetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0"));
    GTEST_LOG_(INFO) << "HitraceCMDTest054: end.";
}

/**
 * @tc.name: HitraceCMDTest055
 * @tc.desc: configuring without --increment after --increment resets increment_index to -1 and default.sys output
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest055, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();
    GTEST_LOG_(INFO) << "HitraceCMDTest055: start.";

    RemoveBootTraceConfig();
    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --boot_trace -t 5 --increment sched", {
        "CONFIG_BOOT_TRACE",
        "increment: true",
        "boot_trace configuration success.",
    }));
    std::string incOn = ReadBootTraceConfig();
    ASSERT_FALSE(incOn.empty());
    EXPECT_NE(incOn.find("\"increment_index\": 0"), std::string::npos) << incOn;

    ASSERT_TRUE(CheckTraceCommandOutput("hitrace --boot_trace -t 5 sched", {
        "CONFIG_BOOT_TRACE",
        "boot_trace configuration success.",
    }));
    std::string incOff = ReadBootTraceConfig();
    ASSERT_FALSE(incOff.empty());
    EXPECT_NE(incOff.find("\"increment_index\": -1"), std::string::npos) << incOff;
    EXPECT_NE(incOff.find("\"output\": \"/data/local/tmp/boot_trace_default.sys\""), std::string::npos) << incOff;

    RemoveBootTraceConfig();
    GTEST_LOG_(INFO) << "HitraceCMDTest055: end.";
}

/**
 * @tc.name: HitraceCMDTest045
 * @tc.desc: --boot_trace, --boot (prefix of boot_trace), and boot-trace require root;
 *           non-root sees unrecognized messages
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest045, TestSize.Level1)
{
    SetBootTraceForceRootForTest(false);

    std::vector<std::string> argsBootTrace = { "hitrace", "--boot_trace", "sched" };
    std::vector<char*> argvBoot;
    for (auto& a : argsBootTrace) {
        argvBoot.push_back(const_cast<char*>(a.c_str()));
    }
    int codeCfg = HiTraceCMDTestMain(static_cast<int>(argvBoot.size()), argvBoot.data());
    std::string outCfg = GetOutput();
    Reset();
    EXPECT_EQ(codeCfg, -1) << "expect exit -1 for --boot_trace when not root";
    EXPECT_NE(outCfg.find("error: unrecognized option '--boot_trace'."), std::string::npos) << outCfg;

    SetBootTraceForceRootForTest(false);
    std::vector<std::string> argsSub = { "hitrace", "boot-trace" };
    std::vector<char*> argvSub;
    for (auto& a : argsSub) {
        argvSub.push_back(const_cast<char*>(a.c_str()));
    }
    int codeSub = HiTraceCMDTestMain(static_cast<int>(argvSub.size()), argvSub.data());
    std::string outSub = GetOutput();
    Reset();
    EXPECT_EQ(codeSub, -1) << "expect exit -1 for boot-trace when not root";
    EXPECT_NE(outSub.find("error: unrecognized command 'boot-trace'."), std::string::npos) << outSub;

    SetBootTraceForceRootForTest(false);
    std::vector<std::string> argsBootAbbr = { "hitrace", "--boot", "sched" };
    std::vector<char*> argvAbbr;
    for (auto& a : argsBootAbbr) {
        argvAbbr.push_back(const_cast<char*>(a.c_str()));
    }
    int codeAbbr = HiTraceCMDTestMain(static_cast<int>(argvAbbr.size()), argvAbbr.data());
    std::string outAbbr = GetOutput();
    Reset();
    EXPECT_EQ(codeAbbr, -1) << "expect exit -1 for --boot when not root";
    EXPECT_NE(outAbbr.find("error: unrecognized option '--boot_trace'."), std::string::npos) << outAbbr;

    SetBootTraceForceRootForTest(true);
}

/**
 * @tc.name: HitraceCMDTest046
 * @tc.desc: under root, --boot matches --boot_trace via getopt_long unique-prefix (same as --boot_trace sched)
 * @tc.type: FUNC
 */
HWTEST_F(HitraceCMDTest, HitraceCMDTest046, TestSize.Level1)
{
    SkipBootTracePrivilegedTestsUnlessRoot();

    RemoveBootTraceConfig();
    std::vector<std::string> args = { "hitrace", "--boot", "sched" };
    std::vector<char*> argv;
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    int code = HiTraceCMDTestMain(static_cast<int>(argv.size()), argv.data());
    std::string out = GetOutput();
    Reset();
    EXPECT_EQ(code, 0) << out;
    EXPECT_NE(out.find("CONFIG_BOOT_TRACE"), std::string::npos) << out;
    EXPECT_NE(out.find("boot_trace configuration success."), std::string::npos) << out;
    RemoveBootTraceConfig();
}
}
}
}
}
