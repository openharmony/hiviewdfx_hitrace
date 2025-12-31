/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "trace_file_utils.h"
#include "trace_json_parser.h"

using namespace testing::ext;
using namespace std;

namespace {
const std::string TEST_TAG_UTILS_JSON = "/data/test/resource/testdata/test_hitrace_utils.json";
const std::string TEST_PRODUCT_CONFIG_JSON = "/data/test/resource/testdata/test_product_config.json";
} // namespace

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
class HitraceUtilsTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

std::string g_traceRootPath;

void HitraceUtilsTest::SetUpTestCase()
{
    if (access((DEBUGFS_TRACING_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        g_traceRootPath = DEBUGFS_TRACING_DIR;
    } else if (access((TRACEFS_DIR + TRACE_MARKER_NODE).c_str(), F_OK) != -1) {
        g_traceRootPath = TRACEFS_DIR;
    } else {
        GTEST_LOG_(ERROR) << "Error: Finding trace folder failed.";
    }
}

namespace {
/**
 * @tc.name: CommonUtilsTest001
 * @tc.desc: Test canonicalizeSpecPath(), enter an existing file path.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, CommonUtilsTest001, TestSize.Level2)
{
    // prepare a file
    std::string filePath = "/data/local/tmp/tmp.txt";
    if (access(filePath.c_str(), F_OK) != 0) {
        int fd = open(filePath.c_str(), O_CREAT);
        close(fd);
    }
    ASSERT_TRUE(CanonicalizeSpecPath(filePath.c_str()) == filePath);
    filePath = "/data/local/tmp/tmp1.txt";
    if (access(filePath.c_str(), F_OK) != 0) {
        ASSERT_TRUE(CanonicalizeSpecPath(filePath.c_str()) == filePath);
    }
    // prepare a file
    filePath = "../tmp2.txt";
    if (access(filePath.c_str(), F_OK) != 0) {
        ASSERT_TRUE(CanonicalizeSpecPath(filePath.c_str()) == "");
    }
}

/**
 * @tc.name: CommonUtilsTest002
 * @tc.desc: Test MarkClockSync().
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, CommonUtilsTest002, TestSize.Level2)
{
    ASSERT_TRUE(MarkClockSync(g_traceRootPath));
}

/**
 * @tc.name: CommonUtilsTest003
 * @tc.desc: Test IsNumber().
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, CommonUtilsTest003, TestSize.Level2)
{
    ASSERT_TRUE(IsNumber("1234"));
    ASSERT_FALSE(IsNumber("123ABC"));
}

/**
 * @tc.name: CommonUtilsTest004
 * @tc.desc: Test GetCpuProcessors/ReadCurrentCpuFrequencies function
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, CommonUtilsTest004, TestSize.Level2)
{
    ASSERT_TRUE(GetCpuProcessors() > 1);
    std::string freqStr;
    ReadCurrentCpuFrequencies(freqStr);
    GTEST_LOG_(INFO) << freqStr;
    ASSERT_TRUE(freqStr.find("cpu_id=") != std::string::npos);
    ASSERT_TRUE(freqStr.find("state=") != std::string::npos);
}

HWTEST_F(HitraceUtilsTest, JsonParserTest001, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.find("sched") == tags.end());
    EXPECT_FALSE(tags["sched"].enablePath.empty());
    EXPECT_FALSE(tags["sched"].formatPath.empty());

    EXPECT_FALSE(jsonParser->GetTagGroups().empty());
}

/**
 * @tc.name: JsonParserTest002
 * @tc.desc: Test TraceJsonParser function, if only parse format infos, tag base infos should also be parsed.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest002, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();

    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    EXPECT_FALSE(tags["sched"].enablePath.empty());
    EXPECT_FALSE(tags["sched"].formatPath.empty());
}

/**
 * @tc.name: JsonParserTest003
 * @tc.desc: Test TraceJsonParser function, check parse format infos, base format info should not be parsed twice.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest003, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();

    int basePathCnt1 = jsonParser->GetBaseFmtPath().size();
    GTEST_LOG_(INFO) << "base format path size:" << basePathCnt1;
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();
}

/**
 * @tc.name: JsonParserTest004
 * @tc.desc: Test TraceJsonParser function. call it as same as hitrace command.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest004, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    ASSERT_FALSE(jsonParser->GetTagGroups().empty());
    EXPECT_FALSE(jsonParser->GetBaseFmtPath().empty());
    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    EXPECT_FALSE(tags["sched"].enablePath.empty());
    EXPECT_FALSE(tags["sched"].formatPath.empty());
}

/**
 * @tc.name: JsonParserTest005
 * @tc.desc: Test TraceJsonParser function. call it as same as hitrace dump record mode.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest005, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    ASSERT_EQ(jsonParser->GetSnapshotDefaultBufferSizeKb(), 2580);
    auto groups = jsonParser->GetTagGroups();
    ASSERT_FALSE(groups.empty());
    ASSERT_FALSE(groups.find("default") == groups.end());
    ASSERT_FALSE(groups.find("scene_performance") == groups.end());
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty());
    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_FALSE(tags["sched"].enablePath.empty());
    ASSERT_FALSE(tags["sched"].formatPath.empty());
}

/**
 * @tc.name: JsonParserTest006
 * @tc.desc: Test TraceJsonParser function. call it as same as hitrace dump service mode.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest006, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    ASSERT_EQ(jsonParser->GetSnapshotDefaultBufferSizeKb(), 2580);
    auto groups = jsonParser->GetTagGroups();
    ASSERT_FALSE(groups.empty());
    ASSERT_FALSE(groups.find("default") == groups.end());
    ASSERT_FALSE(groups.find("scene_performance") == groups.end());
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty());
    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_FALSE(tags["sched"].enablePath.empty());
    ASSERT_FALSE(tags["sched"].formatPath.empty());
}

/**
 * @tc.name: JsonParserTest007
 * @tc.desc: Test TraceJsonParser function. call it as same as trace aging.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest007, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);
    ASSERT_TRUE(jsonParser->GetAgeingParam(TraceDumpType::TRACE_SNAPSHOT).rootEnable);
    ASSERT_TRUE(jsonParser->GetAgeingParam(TraceDumpType::TRACE_RECORDING).rootEnable);
}

HWTEST_F(HitraceUtilsTest, JsonParserTest008, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);

    AgeingParam param = jsonParser->GetAgeingParam(TraceDumpType::TRACE_SNAPSHOT);
    EXPECT_EQ(param.rootEnable, true);
    EXPECT_EQ(param.fileNumberLimit, 35);
    EXPECT_EQ(param.fileSizeKbLimit, 1024);

    param = jsonParser->GetAgeingParam(TraceDumpType::TRACE_RECORDING);
    EXPECT_EQ(param.rootEnable, true);
    EXPECT_EQ(param.fileNumberLimit, 15);
    EXPECT_EQ(param.fileSizeKbLimit, 2048);

    param = jsonParser->GetAgeingParam(TraceDumpType::TRACE_CACHE);
    EXPECT_EQ(param.rootEnable, true);
    EXPECT_EQ(param.fileNumberLimit, 0);
    EXPECT_EQ(param.fileSizeKbLimit, 0);

    EXPECT_EQ(jsonParser->GetSnapshotDefaultBufferSizeKb(), 2580);
}

HWTEST_F(HitraceUtilsTest, JsonParserTest009, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser
        = std::make_shared<TraceJsonParser>(TEST_TAG_UTILS_JSON, TEST_PRODUCT_CONFIG_JSON);

    const std::map<std::string, TraceTag>& allTagInfos = jsonParser->GetAllTagInfos();
    EXPECT_EQ(allTagInfos.size(), 78);

    auto it = allTagInfos.find("pagecache");
    ASSERT_NE(it, allTagInfos.end());
    TraceTag tag = it->second;
    EXPECT_EQ(tag.description, "Page cache");
    EXPECT_EQ(tag.tag, 1);
    EXPECT_EQ(tag.type, TraceType::KERNEL);

    std::vector<std::string> vec = {
        "events/filemap/file_check_and_advance_wb_err/format",
        "events/filemap/filemap_set_wb_err/format",
        "events/filemap/mm_filemap_add_to_page_cache/format",
        "events/filemap/mm_filemap_delete_from_page_cache/format"
    };
    EXPECT_EQ(tag.formatPath, vec);

    vec = {
        "events/filemap/enable"
    };
    EXPECT_EQ(tag.enablePath, vec);
}

/**
 * @tc.name: StringToNumberTest
 * @tc.desc: Test StringToInt/StringToInt64/StringToUint64/StringToDouble function.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, StringToNumberTest, TestSize.Level2)
{
    std::string traceParamsStr = "123";
    int paramsInt = 0;
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    EXPECT_EQ(paramsInt, 123);  // 123: test value
    traceParamsStr = "-123";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    EXPECT_EQ(paramsInt, -123);  // -123: test value
    traceParamsStr = "2147483647";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    EXPECT_EQ(paramsInt, INT_MAX);
    traceParamsStr = "-2147483648";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    EXPECT_EQ(paramsInt, INT_MIN);

    traceParamsStr = "1234567890";
    int64_t paramsInt64 = 0;
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    EXPECT_EQ(paramsInt64, 1234567890); // 1234567890: test value
    traceParamsStr = "-1234567890";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    EXPECT_EQ(paramsInt64, -1234567890); // -1234567890: test value
    traceParamsStr = "9223372036854775807";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    EXPECT_EQ(paramsInt64, LLONG_MAX);
    traceParamsStr = "-9223372036854775808";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    EXPECT_EQ(paramsInt64, LLONG_MIN);

    traceParamsStr = "1234567890";
    uint64_t paramsUint64 = 0;
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    EXPECT_EQ(paramsUint64, 1234567890); // 1234567890: test value
    traceParamsStr = "18446744073709551615";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    EXPECT_EQ(paramsUint64, ULLONG_MAX);

    traceParamsStr = "1234567890.123456";
    double paramsDouble = 0;
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
    EXPECT_EQ(paramsDouble, 1234567890.123456); // 1234567890.123456: test value
    traceParamsStr = "-1234567890.123456";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
    EXPECT_EQ(paramsDouble, -1234567890.123456); // -1234567890.123456: test value
    traceParamsStr = ".1";
    EXPECT_TRUE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
    EXPECT_EQ(paramsDouble, 0.1); // 0.1: test value
}

/**
 * @tc.name: StringToIntErrorTest
 * @tc.desc: Test StringToInt function.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, StringToIntErrorTest, TestSize.Level2)
{
    std::string traceParamsStr = "a123";
    int paramsInt = 0;
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = "12a3";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = "";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = "abc";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = ".1";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = "1.1";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = "2147483648";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
    traceParamsStr = "-2147483649";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt(traceParamsStr, paramsInt));
}

/**
 * @tc.name: StringToInt64ErrorTest
 * @tc.desc: Test StringToInt64 function.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, StringToInt64ErrorTest, TestSize.Level2)
{
    std::string traceParamsStr = "a123";
    int64_t paramsInt64 = 0;
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = "12a3";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = "";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = "abc";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = ".1";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = "1.1";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = "9223372036854775808";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
    traceParamsStr = "-9223372036854775809";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToInt64(traceParamsStr, paramsInt64));
}

/**
 * @tc.name: StringToUint64ErrorTest
 * @tc.desc: Test StringToUint64 function.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, StringToUint64ErrorTest, TestSize.Level2)
{
    std::string traceParamsStr = "-1234567890";
    uint64_t paramsUint64 = 0;
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = "a123";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = "";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = "12a3";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = "abc";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = ".1";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = "1.1";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
    traceParamsStr = "18446744073709551616";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToUint64(traceParamsStr, paramsUint64));
}

/**
 * @tc.name: StringToDoubleErrorTest
 * @tc.desc: Test StringToDouble function.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, StringToDoubleErrorTest, TestSize.Level2)
{
    std::string traceParamsStr = "a123";
    double paramsDouble = 0;
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
    traceParamsStr = "12a3";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
    traceParamsStr = "";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
    traceParamsStr = "abc";
    EXPECT_FALSE(OHOS::HiviewDFX::Hitrace::StringToDouble(traceParamsStr, paramsDouble));
}

HWTEST_F(HitraceUtilsTest, GetRemainingSpace_001, TestSize.Level2)
{
    uint64_t remainingSpace = GetRemainingSpace("/data");
    EXPECT_NE(remainingSpace, 0);
    EXPECT_NE(remainingSpace, UINT64_MAX);
}

HWTEST_F(HitraceUtilsTest, GetTraceFileNamesInDir_001, TestSize.Level2)
{
    std::filesystem::remove_all(TRACE_FILE_DEFAULT_DIR);
    std::set<std::string> fileSet = {};
    GetTraceFileNamesInDir(fileSet, TraceDumpType::TRACE_SNAPSHOT);

    EXPECT_TRUE(fileSet.empty());
    system("service_control stop hiview");
    system("service_control start hiview");
}

/**
 * @tc.name: GetMemInfoByNameTest001
 * @tc.desc: Test get total memory size.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetMemInfoByNameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest001: start.";
    const char* const testFile = "/data/test_meminfo_normal";
    std::ofstream ofs(testFile);
    ofs << "MemTotal:       16384000 kB\n";
    ofs.close();

    int result = GetMemInfoByName("MemTotal", testFile);
    GTEST_LOG_(INFO) << result;
    EXPECT_EQ(result, 16384000);

    std::remove(testFile);
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest001: end.";
}

/**
 * @tc.name: GetMemInfoByNameTest002
 * @tc.desc: Test GetMemInfoByName open file failed.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetMemInfoByNameTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest002: start.";
    const char* const testFile = "/data/non_existent_meminfo";
    int result = GetMemInfoByName("MemTotal", testFile);
    EXPECT_EQ(result, 0);
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest002: end.";
}

/**
 * @tc.name: GetMemInfoByNameTest003
 * @tc.desc: Test GetMemInfoByName can not find MemTotal.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetMemInfoByNameTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest003: start.";
    const char* const testFile = "/data/test_meminfo_no_total";
    std::ofstream ofs(testFile);
    ofs << "MemFree:        8192000 kB\n";
    ofs.close();

    int result = GetMemInfoByName("MemTotal", testFile);
    EXPECT_EQ(result, 0);

    std::remove(testFile);
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest003: end.";
}

/**
 * @tc.name: GetMemInfoByNameTest004
 * @tc.desc: Test GetMemInfoByName get memory size info failed.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetMemInfoByNameTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest004: start.";
    const char* const testFile = "/data/test_meminfo_invalid1";
    std::ofstream ofs(testFile);
    ofs << "MemTotal:       invalid kB\n";
    ofs.close();

    int result = GetMemInfoByName("MemTotal", testFile);
    EXPECT_EQ(result, 0);

    std::remove(testFile);
    GTEST_LOG_(INFO) << "GetMemInfoByNameTest004: end.";
}

/**
 * @tc.name: GetNoFilterEvents001
 * @tc.desc: Test fun GetNoFilterEvents single normal input.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetNoFilterEvents001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetNoFilterEvents001: start.";
    std::vector<std::string> enablePath = {"events/binder/binder_transaction/enable"};
    std::vector<std::string> expected = {"binder:binder_transaction"};
    auto result = GetNoFilterEvents(enablePath);
    EXPECT_EQ(result[0], expected[0]);
    GTEST_LOG_(INFO) << "GetNoFilterEvents001: end.";
}

/**
 * @tc.name: GetNoFilterEvents002
 * @tc.desc: Test fun GetNoFilterEvents multiple normal input.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetNoFilterEvents002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetNoFilterEvents002: start.";
    std::vector<std::string> enablePath = {
        "events/sched/sched_switch/enable",
        "events/irq/irq_handler_entry/enable",
        "events/power/cpu_idle/enable"
    };
    std::vector<std::string> expected = {
        "sched:sched_switch",
        "irq:irq_handler_entry",
        "power:cpu_idle"
    };
    auto result = GetNoFilterEvents(enablePath);
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    GTEST_LOG_(INFO) << "GetNoFilterEvents002: end.";
}

/**
 * @tc.name: GetNoFilterEvents003
 * @tc.desc: Test fun GetNoFilterEvents input null string.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetNoFilterEvents003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetNoFilterEvents003: start.";
    std::vector<std::string> enablePath = {""};
    auto result = GetNoFilterEvents(enablePath);
    EXPECT_EQ(result.size(), 0);
    GTEST_LOG_(INFO) << "GetNoFilterEvents003: end.";
}

/**
 * @tc.name: GetNoFilterEvents004
 * @tc.desc: Test fun GetNoFilterEvents input error string.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, GetNoFilterEvents004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetNoFilterEvents004: start.";
    std::vector<std::string> enablePath = {"events/binder"};
    std::vector<std::string> expected;
    auto result = GetNoFilterEvents(enablePath);
    EXPECT_EQ(result, expected);
    GTEST_LOG_(INFO) << "GetNoFilterEvents004: end.";
}

/**
 * @tc.name: ProcessExistTest001
 * @tc.desc: test IsProcessExist func
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, ProcessExistTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessExistTest001: start.";
    pid_t pid = fork();

    if (pid < 0) {
        GTEST_LOG_(INFO) << "fork failed" << strerror(errno);
    } else if (pid == 0) {
        sleep(2);
        GTEST_LOG_(INFO) << "child process exited";
    } else {
        // child proc exist
        EXPECT_TRUE(IsProcessExist(pid));

        sleep(6);
        // child proc zombie
        EXPECT_FALSE(IsProcessExist(pid));
    }
    // proc not exist
    EXPECT_FALSE(IsProcessExist(65536));
    GTEST_LOG_(INFO) << "ProcessExistTest001: end.";
}

/**
 * @tc.name: ProcessExistTest002
 * @tc.desc: test IsProcessExist func with invalid pid
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, ProcessExistTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessExistTest002: start.";
    EXPECT_FALSE(IsProcessExist(-1));
    GTEST_LOG_(INFO) << "ProcessExistTest002: end.";
}

/**
 * @tc.name: IsTraceFilePathLegal001
 * @tc.desc: test IsTraceFilePathLegal func
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, IsTraceFilePathLegal001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsTraceFilePathLegal001: start.";
    char realFilePath[PATH_MAX];
    std::string fileName = "/data/log/hitrace/trace.txt";
    std::ofstream file(fileName);
    EXPECT_TRUE(IsTraceFilePathLegal(fileName, realFilePath, PATH_MAX));
    std::string nonexistFile = "/data/log/hitrace/nonexistfile.txt";
    EXPECT_FALSE(IsTraceFilePathLegal(nonexistFile, realFilePath, PATH_MAX));
    std::string errpathFile = "/data/local/tmp/trace.txt";
    std::ofstream file2(errpathFile);
    EXPECT_FALSE(IsTraceFilePathLegal(errpathFile, realFilePath, PATH_MAX));
    const std::filesystem::path filePath= "/data/log/hitrace/trace.txt";
    std::filesystem::remove(filePath);
    const std::filesystem::path filePath2 = "/data/local/tmp/trace.txt";
    std::filesystem::remove(filePath2);
    GTEST_LOG_(INFO) << "IsTraceFilePathLegal001: end.";
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
