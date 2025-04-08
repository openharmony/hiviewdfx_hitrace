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
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

#include "common_define.h"
#include "common_utils.h"
#include "trace_json_parser.h"

using namespace testing::ext;
using namespace std;

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

/**
 * @tc.name: JsonParserTest001
 * @tc.desc: Test TraceJsonParser function, parse json infos step by step, check all data and every step state.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest001, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->GetAllTagInfos().empty());
    ASSERT_TRUE(jsonParser->GetTagGroups().empty());
    ASSERT_TRUE(jsonParser->GetBaseFmtPath().empty());

    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_BUFSZ_INFO));
    ASSERT_TRUE(jsonParser->GetTagGroups().empty());
    ASSERT_TRUE(jsonParser->GetBaseFmtPath().empty());
    ASSERT_TRUE(jsonParser->GetAllTagInfos().empty());
    ASSERT_EQ(jsonParser->GetSnapShotBufSzKb(), 0);

    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_BASE_INFO));
    ASSERT_TRUE(jsonParser->GetTagGroups().empty());
    ASSERT_TRUE(jsonParser->GetBaseFmtPath().empty());
    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_TRUE(tags["sched"].enablePath.empty());
    ASSERT_TRUE(tags["sched"].formatPath.empty());

    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_ENABLE_INFO));
    tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_FALSE(tags["sched"].enablePath.empty());
    ASSERT_TRUE(tags["sched"].formatPath.empty());

    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_FORMAT_INFO));
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();
    tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_FALSE(tags["sched"].enablePath.empty());
    ASSERT_FALSE(tags["sched"].formatPath.empty());

    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_GROUP_INFO));
    ASSERT_FALSE(jsonParser->GetTagGroups().empty());
}

/**
 * @tc.name: JsonParserTest002
 * @tc.desc: Test TraceJsonParser function, if only parse format infos, tag base infos should also be parsed.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest002, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->ParseTraceJson(TRACE_TAG_FORMAT_INFO));
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();

    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_TRUE(tags["sched"].enablePath.empty());
    ASSERT_FALSE(tags["sched"].formatPath.empty());
}

/**
 * @tc.name: JsonParserTest003
 * @tc.desc: Test TraceJsonParser function, check parse format infos, base format info should not be parsed twice.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest003, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_FORMAT_INFO));
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();

    int basePathCnt1 = jsonParser->GetBaseFmtPath().size();
    GTEST_LOG_(INFO) << "base format path size:" << basePathCnt1;
    jsonParser->ParseTraceJson(PARSE_TRACE_FORMAT_INFO);
    ASSERT_FALSE(jsonParser->GetBaseFmtPath().empty()) << "base format path size:" <<
        jsonParser->GetBaseFmtPath().size();
    int basePathCnt2 = jsonParser->GetBaseFmtPath().size();
    GTEST_LOG_(INFO) << "base format path size:" << basePathCnt2;

    ASSERT_EQ(basePathCnt1, basePathCnt2);
}

/**
 * @tc.name: JsonParserTest004
 * @tc.desc: Test TraceJsonParser function. call it as same as hitrace command.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest004, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_BASE_INFO));
    ASSERT_TRUE(jsonParser->GetTagGroups().empty());
    ASSERT_TRUE(jsonParser->GetBaseFmtPath().empty());
    auto tags = jsonParser->GetAllTagInfos();
    ASSERT_FALSE(tags.empty());
    ASSERT_FALSE(tags.find("sched") == tags.end());
    ASSERT_TRUE(tags["sched"].enablePath.empty());
    ASSERT_TRUE(tags["sched"].formatPath.empty());
}

/**
 * @tc.name: JsonParserTest005
 * @tc.desc: Test TraceJsonParser function. call it as same as hitrace dump record mode.
 * @tc.type: FUNC
*/
HWTEST_F(HitraceUtilsTest, JsonParserTest005, TestSize.Level2)
{
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_TRACE_GROUP_INFO));
    ASSERT_EQ(jsonParser->GetSnapShotBufSzKb(), 0);
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
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->ParseTraceJson(PARSE_ALL_INFO));
    ASSERT_EQ(jsonParser->GetSnapShotBufSzKb(), 0);
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
    std::shared_ptr<TraceJsonParser> jsonParser = std::make_shared<TraceJsonParser>();
    ASSERT_TRUE(jsonParser->ParseTraceJson(TRACE_SNAPSHOT_FILE_AGE));
    ASSERT_TRUE(jsonParser->GetSnapShotFileAge());
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
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS