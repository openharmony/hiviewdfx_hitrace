/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#include <string>
#include <fstream>
#include <fcntl.h>
#include <regex>
#include <gtest/gtest.h>
#include <hilog/log.h>
#include "securec.h"
#include "hitrace/trace.h"
#include "hitrace_osal.h"
#include "parameters.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS::HiviewDFX;
using namespace OHOS::HiviewDFX::HitraceOsal;

#define EXPECTANTLY(exp) (__builtin_expect(!!(exp), true))

namespace OHOS {
namespace HiviewDFX {
namespace HitraceTest {
const string TRACE_MARKER_PATH = "trace_marker";
const string TRACING_ON = "tracing_on";
const string TRACE_MARK_WRITE = "tracing_mark_write";
const string TRACE_PATH = "trace";
const string TRACE_PATTERN = "\\s*(.*?)-(.*?)\\s+(.*?)\\[(\\d+?)\\]\\s+(.*?)\\s+((\\d+).(\\d+)?):\\s+"
    + TRACE_MARK_WRITE + ": ";
const string TRACE_START = TRACE_PATTERN + "B\\|(.*?)\\|H:";
const string TRACE_ASYNC_START = TRACE_PATTERN + "S\\|(.*?)\\|H:";
const string TRACE_ASYNC_FINISH = TRACE_PATTERN + "F\\|(.*?)\\|H:";
const string TRACE_COUNT = TRACE_PATTERN + "C\\|(.*?)\\|H:";
const string TRACE_PROPERTY = "debug.hitrace.tags.enableflags";
constexpr uint32_t TASK = 1;
constexpr uint32_t TGID = 3;
constexpr uint32_t TID = 2;
constexpr uint32_t CPU = 4;
constexpr uint32_t DNH2 = 5;
constexpr uint32_t TIMESTAMP = 6;
constexpr uint32_t PID = 9;
constexpr uint32_t NUM = 11;
constexpr uint32_t TRACE_NAME = 10;
constexpr uint32_t TRACE_FMA12 = 12;
constexpr uint32_t TRACE_FMA11 = 11;

constexpr uint64_t HITRACE_TAG = 0xD002D33;
const constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HITRACE_TAG, "Hitrace_TEST"};
const uint64_t TAG = (1ULL << 62);
static string g_traceRootPath;

bool SetProperty(const string& property, const string& value);
string GetProperty(const string& property, const string& value);
bool CleanFtrace();
bool CleanTrace();
bool SetFtrace(const string& filename, bool enabled);

class HitraceNDKTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown() {} // empty function
};

void  HitraceNDKTest::SetUpTestCase()
{
    const string tracefsDir = "/sys/kernel/tracing/";
    const string debugfsDir = "/sys/kernel/debug/tracing/";
    if (access((debugfsDir + TRACE_MARKER_PATH).c_str(), F_OK) != -1) {
        g_traceRootPath = debugfsDir;
    } else if (access((tracefsDir + TRACE_MARKER_PATH).c_str(), F_OK) != -1) {
        g_traceRootPath = tracefsDir;
    } else {
        HiLog::Error(LABEL, "Error: Finding trace folder failed.");
    }
    CleanFtrace();
}

void HitraceNDKTest::TearDownTestCase()
{
    SetFtrace(TRACING_ON, false);
    SetProperty(TRACE_PROPERTY, "0");
    CleanTrace();
}

void HitraceNDKTest::SetUp()
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on 1 failed.";
    string value = to_string(TAG);
    SetProperty(TRACE_PROPERTY, value);
    HiLog::Info(LABEL, "current tag is %{public}s", GetProperty(TRACE_PROPERTY, "0").c_str());
    ASSERT_TRUE(GetProperty(TRACE_PROPERTY, "-123") == value);
}

struct Param {
    string mTask;
    string mTid;
    string mTgid;
    string mCpu;
    string mDnh2;
    string mTimeStamp;
    string mPid;
    string mTraceName;
    string mNum;
};

class MyTrace {
    Param mParam;
    bool mLoaded = false;
public:
    MyTrace() : mLoaded(false)
    {
        mParam.mTask = "";
        mParam.mTid = "";
        mParam.mTgid = "";
        mParam.mCpu = "";
        mParam.mDnh2 = "";
        mParam.mTimeStamp = "";
        mParam.mPid = "";
        mParam.mTraceName = "";
        mParam.mNum = "";
    }

    ~MyTrace()
    {
    }

    // task-pid  ( tig) [cpu] ...1    timestamp: tracing_mark_write: B|pid|traceName
    // task-pid  ( tig) [cpu] ...1    timestamp: tracing_mark_write: E|pid
    void Load(const Param& param)
    {
        mParam.mTask = param.mTask;
        mParam.mPid = param.mPid;
        mParam.mTid = param.mTid;
        mParam.mTgid = param.mTgid;
        mParam.mCpu = param.mCpu;
        mParam.mDnh2 = param.mDnh2;
        mParam.mTimeStamp = param.mTimeStamp;
        mParam.mTraceName = param.mTraceName;
        mParam.mNum = param.mNum;
        mLoaded = true;
    }

    string GetTask()
    {
        return mParam.mTask;
    }

    string GetPid()
    {
        if (mLoaded) {
            return mParam.mPid;
        }
        return "";
    }

    string GetTgid()
    {
        if (mLoaded) {
            return mParam.mTgid;
        }
        return "";
    }

    string GetCpu()
    {
        if (mLoaded) {
            return mParam.mCpu;
        }
        return "";
    }

    string GetDnh2()
    {
        if (mLoaded) {
            return mParam.mDnh2;
        }
        return "";
    }

    string GetTimestamp()
    {
        if (mLoaded) {
            return mParam.mTimeStamp;
        }
        return "";
    }

    string GetTraceName()
    {
        if (mLoaded) {
            return mParam.mTraceName;
        }
        return "";
    }

    string GetNum()
    {
        if (mLoaded) {
            return mParam.mNum;
        }
        return "";
    }

    string GetTid()
    {
        if (mLoaded) {
            return mParam.mTid;
        }
        return "";
    }

    bool IsLoaded() const
    {
        return mLoaded;
    }
};

string GetProperty(const string& property, const string& value)
{
    return OHOS::system::GetParameter(property, value);
}

bool SetProperty(const string& property, const string& value)
{
    bool result = false;
    result = OHOS::system::SetParameter(property, value);
    if (!result) {
        // SetParameter failed
        HiLog::Error(LABEL, "Error: setting %s failed", property.c_str());
        return false;
    }
    return true;
}

MyTrace GetTraceResult(const string& checkContent, const vector<string>& list)
{
    MyTrace trace;
    if (list.empty() || checkContent.empty()) {
        return trace;
    }
    smatch match;
    regex pattern(checkContent);
    Param param {""};
    for (int i = list.size() - 1; i >= 0; i--) {
        if (regex_match(list[i],  match, pattern)) {
            param.mTask = match[TASK];
            param.mTid =  match[TID];
            param.mTgid = match[TGID];
            param.mCpu = match[CPU];
            param.mDnh2 = match[DNH2];
            param.mTimeStamp = match[TIMESTAMP];
            param.mPid = match[PID];
            if (match.size() == TRACE_FMA11) {
                param.mTraceName =   match[TRACE_NAME],
                param.mNum = "";
            } else if (match.size() == TRACE_FMA12) {
                param.mTraceName =   match[TRACE_NAME],
                param.mNum = match[NUM];
            } else {
                param.mTraceName = "";
                param.mNum = "";
            }
            trace.Load(param);
            break;
        }
    }
    return trace;
}

static bool WriteStringToFile(const string& fileName, const string& str)
{
    if (g_traceRootPath == "") {
        HiLog::Error(LABEL, "Error: trace path not found.");
        return false;
    }
    ofstream out;
    out.open(g_traceRootPath + fileName, ios::out);
    out << str;
    out.close(); // release resources
    return true;
}

static stringstream ReadFile(const string& filename)
{
    stringstream ss;
    char resolvedPath[PATH_MAX] = {};
    if (realpath(filename.c_str(), resolvedPath) == nullptr) {
        fprintf(stderr, "Error: _fullpath %s failed.", filename.c_str());
        return ss;
    }
    ifstream fin(resolvedPath);
    if (!fin.is_open()) {
        fprintf(stderr, "opening file: %s failed.", filename.c_str());
        return ss;
    }
    ss << fin.rdbuf();
    fin.close();
    return ss;
}

bool CleanTrace()
{
    // trace is not mounted
    if (g_traceRootPath  == "") {
        HiLog::Error(LABEL, "Error: trace path not found!");
        return false;
    }
    ofstream ofs;
    ofs.open(g_traceRootPath + TRACE_PATH, ofstream::out);
    if (!ofs.is_open()) {
        HiLog::Error(LABEL, "Error: opening trace path failed!");
        return false;
    }
    ofs << "";
    ofs.close(); // release resources
    return true;
}

static bool IsFileExisting(const string& filename)
{
    return access(filename.c_str(), F_OK) != -1;
}

bool SetFtrace(const string& filename, bool enabled)
{
    // true represents 1
    return WriteStringToFile(filename, enabled ? "1" : "0");
}

bool CleanFtrace()
{
    return WriteStringToFile("set_event", "");
}

vector<string> ReadFile2string(const string& filename)
{
    vector<string> list;
    if (!IsFileExisting(filename)) {
        return list;
    } else {
        string line;
        stringstream ss = ReadFile(filename);
        while (getline(ss, line)) {
            list.emplace_back(move(line));
        }
    }
    return list;
}

vector<string> ReadTrace()
{
    // read trace from /sys/kernel/debug/tracing/trace
    return ReadFile2string(g_traceRootPath + TRACE_PATH);
}

string GetFinishTraceRegex(MyTrace& trace)
{
    if (trace.IsLoaded()) {
        return "\\s*(.*?)-(" + trace.GetTid() + "?)\\s+(.*?)\\[(\\d+?)\\]\\s+(.*?)\\s+" + "((\\d+).(\\d+)?):\\s+" +
               TRACE_MARK_WRITE + ": E\\|(" + trace.GetPid() + ")|(.*)";
    } else {
        return "";
    }
}

/**
 * @tc.name: Hitrace
 * @tc.desc: test function OH_HiTrace_StartTrace OH_HiTrace_FinishTrace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_001, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Hitrace Setting tracing_on failed.";
    OH_HiTrace_StartTrace("HitraceStartTrace001");
    OH_HiTrace_FinishTrace();
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Hitrace Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(HitraceStartTrace001) ", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Hitrace Can't find \"B|pid|HitraceStartTrace001\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Hitrace Can't find \"E|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: test function OH_HiTrace_StartAsyncTrace OH_HiTrace_FinishAsyncTrace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_002, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    OH_HiTrace_StartAsyncTrace("countTraceTest002", 123);
    OH_HiTrace_FinishAsyncTrace("countTraceTest002", 123);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_ASYNC_START + "(countTraceTest002) (.*)", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Can't find \"S|pid|countTraceTest002\" from trace.";
    MyTrace finishTrace =
        GetTraceResult(TRACE_ASYNC_FINISH + startTrace.GetTraceName() + " " + startTrace.GetNum(), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Can't find \"F|\" from trace.";
}

/**
 * @tc.name: Hitrace
 * @tc.desc: test function OH_HiTrace_CountTrace.
 * @tc.type: FUNC
 */
HWTEST_F(HitraceNDKTest, StartTrace_003, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    OH_HiTrace_CountTrace("countTraceTest003", 1);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace countTrace = GetTraceResult(TRACE_COUNT + "(countTraceTest003) (.*)", list);
    ASSERT_TRUE(countTrace.IsLoaded()) << "Can't find \"C|\" from trace.";
}

} // namespace HitraceTest
} // namespace HiviewDFX
} // namespace OHOS