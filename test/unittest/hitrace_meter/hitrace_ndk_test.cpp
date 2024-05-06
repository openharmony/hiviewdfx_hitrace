/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
#include <regex>
#include <string>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <hilog/log.h>
#include "securec.h"
#include "hitrace_meter.h"
#include "hitrace_osal.h"
#include "parameters.h"
#include "hitrace/tracechain.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS::HiviewDFX;
using namespace OHOS::HiviewDFX::HitraceOsal;

#define EXPECTANTLY(exp) (__builtin_expect(!!(exp), true))

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
const string TRACE_MARKER_PATH = "trace_marker";
const string TRACING_ON_PATH = "tracing_on";
const string TRACING_ON = "tracing_on";
const string TRACE_PATH = "trace";
const string TRACE_PROPERTY = "debug.hitrace.tags.enableflags";
const string KEY_RO_DEBUGGABLE = "ro.debuggable";
const string KEY_APP_NUMBER = "debug.hitrace.app_number";
const string LABEL_HEADER = "|H:";
const string VERTICAL_LINE = "|";

constexpr uint64_t TRACE_INVALIDATE_TAG = 0x1000000;
constexpr uint32_t SLEEP_ONE_SECOND = 1;
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33

#undef LOG_TAG
#define LOG_TAG "Hitrace_TEST"
const uint64_t TAG = HITRACE_TAG_OHOS;
constexpr int HITRACEID_LEN = 64;
static string g_traceRootPath;
static string g_traceHmDir;
static int g_pid;
CachedHandle g_cachedHandle;
CachedHandle g_appPidCachedHandle;

bool SetProperty(const string& property, const string& value);
string GetProperty(const string& property, const string& value);
bool CleanTrace();
bool CleanFtrace();
bool SetFtrace(const string& filename, bool enabled);

class HitraceNDKTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown() {}
};

void  HitraceNDKTest::SetUpTestCase()
{
    g_pid = getpid();
    const string debugfsDir = "/sys/kernel/debug/tracing/";
    const string tracefsDir = "/sys/kernel/tracing/";
    if (access((debugfsDir + TRACE_MARKER_PATH).c_str(), F_OK) != -1) {
        g_traceRootPath = debugfsDir;
    } else if (access((tracefsDir + TRACE_MARKER_PATH).c_str(), F_OK) != -1) {
        g_traceRootPath = tracefsDir;
    } else {
        HILOG_ERROR(LOG_CORE, "Error: Finding trace folder failed");
    }
    if (access((g_traceRootPath + "hongmeng/").c_str(), F_OK) != -1) {
        g_traceHmDir = "hongmeng/";
    }
    CleanFtrace();
}

void HitraceNDKTest::TearDownTestCase()
{
    SetProperty(TRACE_PROPERTY, "0");
    SetFtrace(TRACING_ON, false);
    CleanTrace();
}

void HitraceNDKTest::SetUp()
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    string value = to_string(TAG);
    SetProperty(TRACE_PROPERTY, value);
    HILOG_INFO(LOG_CORE, "current tag is %{public}s", GetProperty(TRACE_PROPERTY, "0").c_str());
    ASSERT_TRUE(GetProperty(TRACE_PROPERTY, "-123") == value);
    UpdateTraceLabel();
}

bool SetProperty(const string& property, const string& value)
{
    bool result = false;
    result = OHOS::system::SetParameter(property, value);
    if (!result) {
        HILOG_ERROR(LOG_CORE, "Error: setting %s failed", property.c_str());
        return false;
    }
    return true;
}

string GetProperty(const string& property, const string& value)
{
    return OHOS::system::GetParameter(property, value);
}

bool GetTimeDuration(int64_t time1, int64_t time2, int64_t diffRange)
{
    int64_t duration = time2 - time1;
    return (duration > 0) && (duration <= diffRange ? true : false);
}

string& Trim(string& s)
{
    if (s.empty()) {
        return s;
    }
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

int64_t GetTimeStamp(string str)
{
    if (str == "") {
        return 0;
    }
    int64_t time;
    Trim(str);
    time = atol(str.erase(str.find("."), 1).c_str());
    return time;
}

string GetRecord(HiTraceId hiTraceId)
{
    std::string record;
    char buf[HITRACEID_LEN] = {0};
    int bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "[%llx,%llx,%llx]#",
        hiTraceId.GetChainId(), hiTraceId.GetSpanId(), hiTraceId.GetParentSpanId());
    if (EXPECTANTLY(bytes > 0)) {
        record += buf;
    }
    std::transform(record.cbegin(), record.cend(), record.begin(), [](unsigned char c) { return tolower(c); });
    return record;
}

bool FindResult(string& str, const vector<string>& list)
{
    for (int i = list.size() - 1; i >= 0; i--) {
        std::string ele = list[i];
        if (ele.find(str) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool GetTraceResult(const char type, const string& traceName, const HiTraceId* hiTraceId,
                    const int taskId, const vector<string>& list)
{
    if (list.empty()) {
        return false;
    }

    std::string prefix;
    std::string chainStr = "";
    std::string str;

    if (hiTraceId != nullptr) {
        chainStr = GetRecord(*hiTraceId);
    }

    if (type == 'B') {
        prefix = "tracing_mark_write: B|";
        str = prefix + std::to_string(g_pid) + LABEL_HEADER + chainStr + traceName;
    } else if (type == 'E') {
        prefix = "tracing_mark_write: E|";
        str = prefix + std::to_string(g_pid) + VERTICAL_LINE;
    } else if (type == 'S') {
        prefix = "tracing_mark_write: S|";
        str = prefix + std::to_string(g_pid) + LABEL_HEADER + chainStr
        + traceName + " " + to_string(taskId);
    } else if (type == 'F') {
        prefix = "tracing_mark_write: F|";
        str = prefix + std::to_string(g_pid) + LABEL_HEADER + chainStr
        + traceName + " " + to_string(taskId);
    } else if (type == 'C') {
        prefix = "tracing_mark_write: C|";
        str = prefix + std::to_string(g_pid) + LABEL_HEADER + chainStr + traceName;
    } else {
        return false;
    }
    return FindResult(str, list);
}

static bool WriteStrToFileInner(const string& fileName, const string& str)
{
    if (g_traceRootPath == "") {
        HILOG_ERROR(LOG_CORE, "Error: trace path not found.");
        return false;
    }
    ofstream out;
    out.open(fileName, ios::out);
    out << str;
    out.close();
    return true;
}

static bool WriteStringToFile(const std::string& filename, const std::string& str)
{
    bool ret = false;
    if (access((g_traceRootPath + "hongmeng/" + filename).c_str(), W_OK) == 0) {
        if (WriteStrToFileInner(g_traceRootPath + "hongmeng/" + filename, str)) {
            ret = true;
        }
    }
    if (access((g_traceRootPath + filename).c_str(), W_OK) == 0) {
        if (WriteStrToFileInner(g_traceRootPath + filename, str)) {
            ret = true;
        }
    }

    return ret;
}

bool CleanTrace()
{
    if (g_traceRootPath.empty()) {
        HILOG_ERROR(LOG_CORE, "Error: trace path not found.");
        return false;
    }
    ofstream ofs;
    ofs.open(g_traceRootPath + g_traceHmDir + TRACE_PATH, ofstream::out);
    if (!ofs.is_open()) {
        HILOG_ERROR(LOG_CORE, "Error: opening trace path failed.");
        return false;
    }
    ofs << "";
    ofs.close();
    return true;
}

static stringstream ReadFile(const string& filename)
{
    stringstream ss;
    char resolvedPath[PATH_MAX] = { 0 };
    if (realpath(filename.c_str(), resolvedPath) == nullptr) {
        fprintf(stderr, "Error: _fullpath %s failed", filename.c_str());
        return ss;
    }
    ifstream fin(resolvedPath);
    if (!fin.is_open()) {
        fprintf(stderr, "opening file: %s failed!", filename.c_str());
        return ss;
    }
    ss << fin.rdbuf();
    fin.close();
    return ss;
}

static bool IsFileExisting(const string& filename)
{
    return access(filename.c_str(), F_OK) != -1;
}

bool SetFtrace(const string& filename, bool enabled)
{
    return WriteStringToFile(filename, enabled ? "1" : "0");
}

bool CleanFtrace()
{
    return WriteStringToFile("events/enable", "0");
}

vector<string> ReadFile2string(const string& filename)
{
    vector<string> list;
    if (IsFileExisting(filename)) {
        stringstream ss = ReadFile(filename);
        string line;
        while (getline(ss, line)) {
            list.emplace_back(move(line));
        }
    }
    return list;
}

vector<string> ReadTrace()
{
    return ReadFile2string(g_traceRootPath + g_traceHmDir + TRACE_PATH);
}

bool RunCmd(const string& cmdstr)
{
    FILE *fp = popen(cmdstr.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    pclose(fp);
    return true;
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_001, TestSize.Level0)
{
    std::string traceName = "HitraceStartTrace001";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    StartTrace(TAG, traceName);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing AddHitraceMeterMarker function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_001, TestSize.Level0)
{
    std::string traceName = "AddHitraceMeterMarker001";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    SetReloadPid(true);
    SetAddHitraceMeterMarker(TAG, traceName);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";

    traceName.resize(520);
    SetReloadPid(false);
    SetpidHasReload(false);
    SetAddHitraceMeterMarker(TAG, traceName);
    SetAddTraceMarkerLarge(traceName, 1);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
    list.clear();
    list = ReadTrace();
    isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_FALSE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing AddHitraceMeterMarker function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_002, TestSize.Level0)
{
    std::string traceName = "AddHitraceMeterMarker002";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    SetAppFd(1);
    SetAddHitraceMeterMarker(TAG, traceName);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
    SetAppFd(-1);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing AddHitraceMeterMarker function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_003, TestSize.Level0)
{
    std::string traceName = "AddHitraceMeterMarker003";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    SetWriteToTraceMarker("B|3728|H:AddHitraceMeterMarker003", 0);
    SetWriteToTraceMarker("B|3728|H:AddHitraceMeterMarker003", 640);
    FinishTraceDebug(false, TAG);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_004, TestSize.Level0)
{
    string traceName = "AddHitraceMeterMarker004";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    int var = 1;
    StartTraceArgs(HITRACE_TAG_ZAUDIO, traceName.c_str(), var);
    FinishTrace(HITRACE_TAG_ZAUDIO);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_005, TestSize.Level0)
{
    std::string traceName = "HitraceStartTrace005";
    string fileName;
    int fileSize = 100 * 1204 * 1024; // 100M

    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";

    SetWriteAppTrace(FLAG_MAIN_THREAD, traceName, 0, true);
    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, fileName);
    SetWriteAppTrace(FLAG_MAIN_THREAD, "", 0, true);
    SetWriteAppTrace(FLAG_MAIN_THREAD, traceName, 0, true);
    SetWriteAppTrace(FLAG_MAIN_THREAD, traceName, 0, false);

    StartTrace(TAG, traceName);
    SetWriteAppTrace(FLAG_ALL_THREAD, traceName, 0, true);
    traceName.insert(traceName.size() - 1, "a", 32729);
    SetWriteAppTrace(FLAG_ALL_THREAD, traceName, 0, false);
    SetWriteAppTrace(FLAG_MAIN_THREAD, traceName, 0, true);

    FinishTrace(TAG);
    ret = StopCaptureAppTrace();
    ASSERT_TRUE(ret == RetType::RET_SUCC);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";

    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_FALSE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_006, TestSize.Level0)
{
    std::string traceName = "HitraceStartTrace006";
    string fileName;
    int fileSize = 100 * 1204 * 1024; // 100M
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";

    SetWriteAppTrace(FLAG_ALL_THREAD, traceName, 0, true);

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, fileName);
    ASSERT_TRUE(ret == RetType::RET_SUCC);
    ret = StopCaptureAppTrace();
    ASSERT_TRUE(ret == RetType::RET_SUCC);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, AddHitraceMeterMarker_007, TestSize.Level0)
{
    std::string traceName = "HitraceStartTrace007";

    StartTrace(TAG, traceName);
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    GetSetMainThreadInfo();
    GetSetCommStr();
    SetTraceBuffer(32775);
    FinishTrace(TAG);

    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_FALSE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output  hitraceId.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartHiTraceIdTest_001, TestSize.Level0)
{
    std::string traceName = "StartHiTraceIdTest001";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    HiTraceId hiTraceId = HiTraceChain::Begin(traceName, HiTraceFlag::HITRACE_FLAG_DEFAULT);
    StartTrace(TAG, traceName);
    FinishTrace(TAG);
    HiTraceChain::End(hiTraceId);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, &hiTraceId, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, &hiTraceId, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node large output  hitraceId.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartHiTraceIdTest_002, TestSize.Level0)
{
    std::string longTraceName = "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    HiTraceId hiTraceId = HiTraceChain::Begin(longTraceName, HiTraceFlag::HITRACE_FLAG_DEFAULT);
    StartTrace(TAG, longTraceName, SLEEP_ONE_SECOND);
    FinishTrace(TAG);
    HiTraceChain::End(hiTraceId);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', longTraceName, &hiTraceId, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + longTraceName + "\" from trace.";

    bool isFinishSuc = GetTraceResult('E', longTraceName, &hiTraceId, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartAsyncHiTraceIdTest_001, TestSize.Level0)
{
    string traceName = "StartAsyncHiTraceIdTest001";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    HiTraceId hiTraceId = HiTraceChain::Begin(traceName, HiTraceFlag::HITRACE_FLAG_DEFAULT);
    StartAsyncTrace(TAG, traceName, taskId);
    FinishAsyncTrace(TAG, traceName, taskId);
    HiTraceChain::End(hiTraceId);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('S', traceName, &hiTraceId, taskId, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"S|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('F', traceName, &hiTraceId, taskId, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"F|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node has no output.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_002, TestSize.Level0)
{
    std::string longTraceName = "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002StartHiTraceIdTest002";
    longTraceName += "StartHiTraceIdTest002";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, longTraceName);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', longTraceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + longTraceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', longTraceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";
}

/**
  * @tc.name: Hitrace
  * @tc.desc: tracing_mark_write file node normal output start trace and end trace.
  * @tc.type: FUNC
  */
HWTEST_F(HitraceNDKTest, StartTrace_003, TestSize.Level0)
{
    string traceName = "StartTraceTest003 %s";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, traceName);
    FinishTrace(TAG);
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";

    ASSERT_TRUE(CleanTrace());
    list.clear();
    traceName = "StartTraceTest003 %p";
    StartTrace(TAG, traceName);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    list = ReadTrace();
    isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: test Input and output interval 1ms execution, time fluctuation 1ms
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_004, TestSize.Level0)
{
    string traceName = "StartTraceTest004";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, traceName);
    usleep(1000);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_005, TestSize.Level0)
{
    string traceName = "asyncTraceTest005";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTrace(TAG, traceName, taskId);
    FinishAsyncTrace(TAG, traceName, taskId);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('S', traceName, nullptr, taskId, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"S|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('F', traceName, nullptr, taskId, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"F|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_006, TestSize.Level0)
{
    string traceName = "countTraceTest006";
    int count = 1;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTrace(TAG, traceName, count);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isCountSuc = GetTraceResult('C', traceName, nullptr, count, list);
    ASSERT_TRUE(isCountSuc) << "Hitrace Can't find \"C|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_007, TestSize.Level1)
{
    string traceName = "StartTraceTest007";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TRACE_INVALIDATE_TAG, traceName);
    FinishTrace(TRACE_INVALIDATE_TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    EXPECT_FALSE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    EXPECT_FALSE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_008, TestSize.Level1)
{
    string traceName = "StartTraceTest008 %s";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TRACE_INVALIDATE_TAG, traceName);
    FinishTrace(TRACE_INVALIDATE_TAG);
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    EXPECT_FALSE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    EXPECT_FALSE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";

    ASSERT_TRUE(CleanTrace());
    list.clear();
    traceName = "StartTraceTest008 %p";
    StartTrace(TRACE_INVALIDATE_TAG, traceName);
    FinishTrace(TRACE_INVALIDATE_TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    list = ReadTrace();
    isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    EXPECT_FALSE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    EXPECT_FALSE(isFinishSuc) << "Hitrace Can't find \"E|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_009, TestSize.Level1)
{
    string traceName = "asyncTraceTest009";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTrace(TRACE_INVALIDATE_TAG, traceName, taskId);
    FinishAsyncTrace(TRACE_INVALIDATE_TAG, traceName, taskId);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('S', traceName, nullptr, taskId, list);
    EXPECT_FALSE(isStartSuc) << "Hitrace Can't find \"S|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('F', traceName, nullptr, taskId, list);
    EXPECT_FALSE(isFinishSuc) << "Hitrace Can't find \"F|pid|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_010, TestSize.Level1)
{
    string traceName = "countTraceTest010";
    int count = 1;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTrace(TRACE_INVALIDATE_TAG, traceName, count);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('C', traceName, nullptr, count, list);
    EXPECT_FALSE(isStartSuc) << "Hitrace Can't find \"C|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node general output start and end tracing for debugging.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_011, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTraceDebug(true, TAG, "StartTraceTest011");
    FinishTraceDebug(true, TAG);
}

/**
  * @tc.name: Hitrace
  * @tc.desc: tracing_mark_write file node general output start and end tracing for debugging.
  * @tc.type: FUNC
  */
HWTEST_F(HitraceNDKTest, StartTrace_012, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTraceDebug(true, TAG, "StartTraceTest012 %s");
    FinishTraceDebug(true, TAG);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing StartAsyncTraceDebug and FinishAsyncTraceDebug functions
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_013, TestSize.Level1)
{
    const char* traceName = "StartTrace013";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTraceDebug(true, TAG, "asyncTraceTest013", 123);
    FinishAsyncTraceDebug(true, TAG, "asyncTraceTest013", 123);
    StartAsyncTraceDebug(false, TAG, "asyncTraceTest013", 123);
    FinishAsyncTraceDebug(false, TAG, "asyncTraceTest013", 123);

    StartTraceChain(TAG, nullptr, traceName);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing CountTraceDebug function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_014, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTraceDebug(true, TAG, "countTraceTest014", 1);
    CountTraceDebug(false, TAG, "countTraceTest014", 1);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing MiddleTrace function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_015, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    MiddleTrace(TAG, "MiddleTraceTest015", "050tseTecarTelddiM");
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing MiddleTraceDebug function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_016, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    MiddleTraceDebug(true, TAG, "MiddleTraceTest016", "061tseTecarTelddiM");
    MiddleTraceDebug(false, TAG, "MiddleTraceTest016", "061tseTecarTelddiM");
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_017, TestSize.Level1)
{
    string traceName = "StartTraceTest017-%d";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    int var = 1;
    StartTraceArgs(TAG, traceName.c_str(), var);
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();

    bool isStartSuc = GetTraceResult('B', traceName.replace(18, 2, to_string(var)), nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName.replace(18, 2, to_string(var)), nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace async with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_018, TestSize.Level1)
{
    string traceName = "asyncTraceTest018-%d";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    int var = 1;
    StartAsyncTraceArgs(TAG, taskId, traceName.c_str(), var);
    FinishAsyncTraceArgs(TAG, taskId, traceName.c_str(), var);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('S', traceName.replace(18, 2, to_string(var)), nullptr, taskId, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"S|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('F', traceName.replace(18, 2, to_string(var)), nullptr, taskId, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"F|pid|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing StartTraceArgsDebug function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_019, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    int var = 1;
    StartTraceArgsDebug(true, TAG, "StartTraceTest019-%d", var);
    FinishTrace(TAG);
    StartTraceArgsDebug(false, TAG, "StartTraceTest019-%d", var);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing StartAsyncTraceArgsDebug and FinishAsyncTraceArgsDebug function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_020, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    int var = 1;
    StartAsyncTraceArgsDebug(true, TAG, 123, "asyncTraceTest020-%d", var);
    FinishAsyncTraceArgsDebug(true, TAG, 123, "asyncTraceTest020-%d", var);

    SetTraceDisabled(true);
    StartAsyncTraceArgsDebug(false, TAG, 123, "asyncTraceTest020-%d", var);
    FinishAsyncTraceArgsDebug(false, TAG, 123, "asyncTraceTest020-%d", var);
    SetTraceDisabled(false);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing StartTraceWrapper function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTraceWrapper_001, TestSize.Level0)
{
    string traceName = "StartTraceWrapper001";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTraceWrapper(TAG, traceName.c_str());
    FinishTrace(TAG);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('B', traceName, nullptr, 0, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"B|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('E', traceName, nullptr, 0, list);
    ASSERT_TRUE(isFinishSuc) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing  StartAsyncTraceWrapper function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartAsyncTraceWrapper_001, TestSize.Level1)
{
    string traceName = "StartAsyncTraceWrapper009";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTraceWrapper(TRACE_INVALIDATE_TAG, traceName.c_str(), taskId);
    FinishAsyncTraceWrapper(TRACE_INVALIDATE_TAG, traceName.c_str(), taskId);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('S', traceName, nullptr, 0, list);
    EXPECT_FALSE(isStartSuc) << "Hitrace Can't find \"S|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('F', traceName, nullptr, 0, list);
    EXPECT_FALSE(isFinishSuc) << "Hitrace Can't find \"F|pid|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing CountTraceWrapper function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, CountTraceWrapper_001, TestSize.Level0)
{
    string traceName = "CountTraceWrapper001";
    int count = 1;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTraceWrapper(TAG, traceName.c_str(), count);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('C', traceName, nullptr, count, list);
    ASSERT_TRUE(isStartSuc) << "Hitrace Can't find \"C|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing SetTraceDisabled function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_021, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    SetTraceDisabled(true);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing SetTraceDisabled function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_022, TestSize.Level1)
{
    std::string traceName = "StartTrace022";
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    ASSERT_TRUE(SetProperty(TRACE_PROPERTY, "0"));
    SetCachedHandleAndAppPidCachedHandle(nullptr, nullptr);
    SetCachedHandleAndAppPidCachedHandle(nullptr, (CachedHandle)0xf7696e60);
    SetCachedHandleAndAppPidCachedHandle((CachedHandle)0xf7c8c130, nullptr);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsAppValid function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_023, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    SetProperty(KEY_RO_DEBUGGABLE, "true");
    SetProperty(KEY_APP_NUMBER, "1");
    StartTrace(TRACE_INVALIDATE_TAG, "StartTraceTest023");
    FinishTrace(TRACE_INVALIDATE_TAG);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing trace cmd function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_024, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(RunCmd("hitrace -h > /data/local/tmp/test1.txt"));
    ASSERT_TRUE(RunCmd("hitrace -l > /data/local/tmp/test2.txt"));
    ASSERT_TRUE(RunCmd("hitrace --list_categories > /data/local/tmp/test3.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin > /data/local/tmp/test4.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_dump > /data/local/tmp/test5.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_finish > /data/local/tmp/test6.txt"));
    ASSERT_TRUE(RunCmd("hitrace --hlep > /data/local/tmp/test7.txt"));
    ASSERT_TRUE(RunCmd("hitrace -a > /data/local/tmp/test8.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_clock > /data/local/tmp/test9.txt"));
    ASSERT_TRUE(RunCmd("hitrace -t a > /data/local/tmp/test10.txt"));
    ASSERT_TRUE(RunCmd("hitrace -t -1 > /data/local/tmp/test11.txt"));
    ASSERT_TRUE(RunCmd("hitrace --time a > /data/local/tmp/test12.txt"));
    ASSERT_TRUE(RunCmd("hitrace --time -1 > /data/local/tmp/test13.txt"));
    ASSERT_TRUE(RunCmd("hitrace -b a > /data/local/tmp/test14.txt"));
    ASSERT_TRUE(RunCmd("hitrace -b -1 > /data/local/tmp/test15.txt"));
    ASSERT_TRUE(RunCmd("hitrace --buffer_size a > /data/local/tmp/test16.txt"));
    ASSERT_TRUE(RunCmd("hitrace --buffer_size -1 > /data/local/tmp/test17.txt"));
    ASSERT_TRUE(RunCmd("hitrace -z --time 1 --buffer_size 10240 --trace_clock clock ohos > /data/local/tmp/trace01"));
    ASSERT_TRUE(RunCmd("hitrace -z -t 1 -b 10240 --trace_clock clock --overwrite ohos > /data/local/tmp/trace02"));
    ASSERT_TRUE(RunCmd("hitrace -t 1 --trace_clock boot ohos > /data/local/tmp/trace03"));
    ASSERT_TRUE(RunCmd("hitrace -t 1 --trace_clock global ohos > /data/local/tmp/trace04"));
    ASSERT_TRUE(RunCmd("hitrace -t 1 --trace_clock mono ohos > /data/local/tmp/trace05"));
    ASSERT_TRUE(RunCmd("hitrace -t 1 --trace_clock uptime ohos > /data/local/tmp/trace06"));
    ASSERT_TRUE(RunCmd("hitrace -t 1 --trace_clock perf ohos > /data/local/tmp/trace07"));
    ASSERT_TRUE(RunCmd("hitrace -b 2048 -t 10 -o /data/local/tmp/test20.txt sched"));
    ASSERT_TRUE(RunCmd("hitrace -b 2048 -t 10 -o /data/local/tmp/test21 load"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin --record app > /data/local/tmp/test22.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_finish --record > /data/local/tmp/test23.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin --record app --file_size 10240 > /data/local/tmp/test24.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_begin --record app --file_size 102400 > /data/local/tmp/test25.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_finish --record > /data/local/tmp/test26.txt"));
    ASSERT_TRUE(RunCmd("hitrace --start_bgsrv > /data/local/tmp/test27.txt"));
    ASSERT_TRUE(RunCmd("hitrace --dump_bgsrv > /data/local/tmp/test28.txt"));
    ASSERT_TRUE(RunCmd("hitrace --stop_bgsrv > /data/local/tmp/test29.txt"));
    ASSERT_TRUE(RunCmd("hitrace -t 3 -b 10240 --text app --output /data/local/tmp/trace.txt"));
    ASSERT_TRUE(RunCmd("hitrace -t 3 -b 10240 --raw app > /data/local/tmp/test30.txt"));
    ASSERT_TRUE(RunCmd("hitrace -t 3 -b 10240 --raw app --file_size 102400 > /data/local/tmp/test31.txt"));
    ASSERT_TRUE(RunCmd("hitrace --trace_finish_nodump > /data/local/tmp/test3.txt"));
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing bytrace cmd function
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_025, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(RunCmd("bytrace -h > /data/local/tmp/test1.txt"));
    ASSERT_TRUE(RunCmd("bytrace -l > /data/local/tmp/test2.txt"));
    ASSERT_TRUE(RunCmd("bytrace --list_categories > /data/local/tmp/test3.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_begin > /data/local/tmp/test4.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_dump > /data/local/tmp/test5.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_finish > /data/local/tmp/test6.txt"));
    ASSERT_TRUE(RunCmd("bytrace --hlep > /data/local/tmp/test7.txt"));
    ASSERT_TRUE(RunCmd("bytrace -a > /data/local/tmp/test8.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_clock > /data/local/tmp/test9.txt"));
    ASSERT_TRUE(RunCmd("bytrace -t a > /data/local/tmp/test10.txt"));
    ASSERT_TRUE(RunCmd("bytrace -t -1 > /data/local/tmp/test11.txt"));
    ASSERT_TRUE(RunCmd("bytrace --time a > /data/local/tmp/test12.txt"));
    ASSERT_TRUE(RunCmd("bytrace --time -1 > /data/local/tmp/test13.txt"));
    ASSERT_TRUE(RunCmd("bytrace -b a > /data/local/tmp/test14.txt"));
    ASSERT_TRUE(RunCmd("bytrace -b -1 > /data/local/tmp/test15.txt"));
    ASSERT_TRUE(RunCmd("bytrace --buffer_size a > /data/local/tmp/test16.txt"));
    ASSERT_TRUE(RunCmd("bytrace --buffer_size -1 > /data/local/tmp/test17.txt"));
    ASSERT_TRUE(RunCmd("bytrace -z --time 1 --buffer_size 10240 --trace_clock clock ohos > /data/local/tmp/trace01"));
    ASSERT_TRUE(RunCmd("bytrace -z -t 1 -b 10240 --trace_clock clock --overwrite ohos > /data/local/tmp/trace02"));
    ASSERT_TRUE(RunCmd("bytrace -t 1 --trace_clock boot ohos > /data/local/tmp/trace03"));
    ASSERT_TRUE(RunCmd("bytrace -t 1 --trace_clock global ohos > /data/local/tmp/trace04"));
    ASSERT_TRUE(RunCmd("bytrace -t 1 --trace_clock mono ohos > /data/local/tmp/trace05"));
    ASSERT_TRUE(RunCmd("bytrace -t 1 --trace_clock uptime ohos > /data/local/tmp/trace06"));
    ASSERT_TRUE(RunCmd("bytrace -t 1 --trace_clock perf ohos > /data/local/tmp/trace07"));
    ASSERT_TRUE(RunCmd("bytrace -b 2048 -t 10 -o /data/local/tmp/test20.txt sched"));
    ASSERT_TRUE(RunCmd("bytrace -b 2048 -t 10 -o /data/local/tmp/test21 load"));
    ASSERT_TRUE(RunCmd("bytrace --trace_begin --record app > /data/local/tmp/test22.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_finish --record > /data/local/tmp/test23.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_begin --record app --file_size 10240 > /data/local/tmp/test24.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_begin --record app --file_size 102400 > /data/local/tmp/test25.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_finish --record > /data/local/tmp/test26.txt"));
    ASSERT_TRUE(RunCmd("bytrace --start_bgsrv > /data/local/tmp/test27.txt"));
    ASSERT_TRUE(RunCmd("bytrace --dump_bgsrv > /data/local/tmp/test28.txt"));
    ASSERT_TRUE(RunCmd("bytrace --stop_bgsrv > /data/local/tmp/test29.txt"));
    ASSERT_TRUE(RunCmd("bytrace -t 3 -b 10240 --text app --output /data/local/tmp/trace.txt"));
    ASSERT_TRUE(RunCmd("bytrace -t 3 -b 10240 --raw app > /data/local/tmp/test30.txt"));
    ASSERT_TRUE(RunCmd("bytrace -t 3 -b 10240 --raw app --file_size 102400 > /data/local/tmp/test31.txt"));
    ASSERT_TRUE(RunCmd("bytrace --trace_finish_nodump > /data/local/tmp/test3.txt"));
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_026, TestSize.Level1)
{
    const std::string keyTraceTag = "debug.hitrace.tags.enableflags";
    ASSERT_TRUE(SetProperty(keyTraceTag, std::to_string(HITRACE_TAG_USB | HITRACE_TAG_HDF)));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_USB));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_HDF));
    ASSERT_FALSE(IsTagEnabled(HITRACE_TAG_ZAUDIO));
    ASSERT_FALSE(IsTagEnabled(HITRACE_TAG_GLOBAL_RESMGR));
    ASSERT_FALSE(IsTagEnabled(HITRACE_TAG_POWER));
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_027, TestSize.Level1)
{
    const std::string keyTraceTag = "debug.hitrace.tags.enableflags";
    ASSERT_TRUE(SetProperty(keyTraceTag, std::to_string(HITRACE_TAG_ZIMAGE | HITRACE_TAG_HDF | HITRACE_TAG_ZAUDIO)));
    ASSERT_FALSE(IsTagEnabled(HITRACE_TAG_USB));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_HDF));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_ZAUDIO | HITRACE_TAG_HDF));
    ASSERT_TRUE(IsTagEnabled(HITRACE_TAG_ZAUDIO | HITRACE_TAG_HDF | HITRACE_TAG_ZIMAGE));
    ASSERT_FALSE(IsTagEnabled(HITRACE_TAG_POWER));
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node general output start and end tracing for debugging.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_028, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTraceDebug(false, TAG, "StartTraceTest028");
    FinishTraceDebug(true, TAG);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace async with args
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_029, TestSize.Level1)
{
    string traceName = "asyncTraceTest029-%d";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    int var = 1;
    StartAsyncTraceArgs(TAG, taskId, traceName.c_str(), var);
    FinishAsyncTraceArgs(TAG, taskId, traceName.c_str(), var);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    bool isStartSuc = GetTraceResult('S', traceName.replace(18, 2, to_string(var)), nullptr, taskId, list);
    ASSERT_FALSE(isStartSuc) << "Hitrace Can't find \"S|pid|" + traceName + "\" from trace.";
    bool isFinishSuc = GetTraceResult('F', traceName.replace(18, 2, to_string(var)), nullptr, taskId, list);
    ASSERT_FALSE(isFinishSuc) << "Hitrace Can't find \"F|pid|" + traceName + "\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartCaptureAppTrace_001, TestSize.Level1)
{
    std::string traceName = "StartCaptureAppTrace001";
    string fileName;
    int fileSize = 100 * 1204 * 1024; // 100M
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";

    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, fileName);
    ASSERT_TRUE(ret == RetType::RET_SUCC);
    ret = StopCaptureAppTrace();
    ASSERT_TRUE(ret == RetType::RET_SUCC);

    // exception scence
    ret = StartCaptureAppTrace((TraceFlag)3, TAG, fileSize, fileName);
    ASSERT_TRUE(ret != RetType::RET_SUCC);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TRACE_INVALIDATE_TAG, fileSize, fileName);
    ASSERT_TRUE(ret != RetType::RET_SUCC);
    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, 0, fileName);
    ASSERT_TRUE(ret != RetType::RET_SUCC);

    ret = StopCaptureAppTrace();
    ASSERT_TRUE(ret != RetType::RET_SUCC);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartCaptureAppTrace_002, TestSize.Level1)
{
    std::string traceName = "StartCaptureAppTrace002";
    string fileName;
    int fileSize = 100 * 1204 * 1024; // 100M
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";

    SetAppFd(1);
    int ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, TAG, fileSize, fileName);
    ASSERT_FALSE(ret == RetType::RET_SUCC);
    SetAppFd(-2);
    ret = StopCaptureAppTrace();
    SetAppFd(-1);
    ASSERT_FALSE(ret == RetType::RET_SUCC);

    ret = StartCaptureAppTrace(FLAG_MAIN_THREAD, 1, fileSize, fileName);
    ASSERT_FALSE(ret == RetType::RET_SUCC);
    ret = StopCaptureAppTrace();
    ASSERT_FALSE(ret == RetType::RET_SUCC);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartCaptureAppTrace_003, TestSize.Level1)
{
    std::string traceName = "StartCaptureAppTrace003";
    const char* filePath = "";
    SetGetProcData(filePath);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, HitraceMeterFmtScoped_001, TestSize.Level1)
{
    string traceName = "HitraceMeterFmtScoped001";
    const char* name = "TestHitraceMeterFmtScoped";
    int taskId = 123;
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTrace(TAG, traceName, taskId);
    SetTraceDisabled(true);
    HitraceMeterFmtScoped(TAG, name);
    SetTraceDisabled(false);
    HitraceMeterFmtScoped(TAG, name);
    FinishAsyncTrace(TAG, traceName, taskId);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, HitracePerfScoped_001, TestSize.Level1)
{
    std::string traceName = "HitracePerfScoped001";
    HitracePerfScoped hitrace(true, TAG, traceName);
    hitrace.SetHitracePerfScoped(-1, -1);
    HitracePerfScoped(true, TAG, traceName);
    HitracePerfScoped(false, TAG, traceName);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, HitracePerfScoped_002, TestSize.Level1)
{
    std::string traceName = "HitracePerfScoped002";
    HitracePerfScoped hitrace(true, TAG, traceName);
    hitrace.SetHitracePerfScoped(0, 0);
}

/**
 * @tc.name: Hitrace
 * @tc.desc: Testing IsTagEnabled with multiple tags
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, HitraceOsal_001, TestSize.Level1)
{
    SetPropertyInner("", "0");
    SetPropertyInner(TRACE_PROPERTY, "0");
    GetPropertyInner(TRACE_PROPERTY, "0");
    RefreshBinderServices();
    RefreshHalServices();
}
} // namespace HitraceTest
} // namespace HiviewDFX
} // namespace OHOS
