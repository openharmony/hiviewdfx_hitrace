/*
 * Copyright (C) 2022-2024 Huawei Device Co., Ltd.
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

#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <zlib.h>

#include "common_define.h"
#include "common_utils.h"
#include "hilog/log.h"
#include "hisysevent_c.h"
#include "hitrace_meter.h"
#include "parameters.h"
#include "securec.h"
#include "trace_collector_client.h"
#include "trace_json_parser.h"

using namespace OHOS::HiviewDFX::Hitrace;

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Hitrace"
#endif

namespace {
using HiviewTraceParam = OHOS::HiviewDFX::UCollectClient::TraceParam;
enum RunningState {
    /* Initial value */
    STATE_NULL = 0,

    /* Record a short trace */
    RECORDING_SHORT_TEXT = 1, // --text
    RECORDING_SHORT_RAW = 2,  // --raw

    /* Record a long trace */
    RECORDING_LONG_BEGIN = 10,         // --trace_begin
    RECORDING_LONG_DUMP = 11,          // --trace_dump
    RECORDING_LONG_FINISH = 12,        // --trace_finish
    RECORDING_LONG_FINISH_NODUMP = 13, // --trace_finish_nodump
    RECORDING_LONG_BEGIN_RECORD = 14,  // --trace_begin --record
    RECORDING_LONG_FINISH_RECORD = 15, // --trace_finish --record

    /* Manipulating trace services in snapshot mode */
    SNAPSHOT_START = 20, // --start_bgsrv
    SNAPSHOT_DUMP = 21,  // --dump_bgsrv
    SNAPSHOT_STOP = 22,  // --stop_bgsrv

    /* Help Info */
    SHOW_HELP = 31,          // -h, --help
    SHOW_LIST_CATEGORY = 32, // -l, --list_categories

    /* Set system parameter */
    SET_TRACE_LEVEL = 33,    // --trace_level level
    GET_TRACE_LEVEL = 34,    // --trace_level
};
}

using CommandFunc = std::function<bool(const RunningState)>;
using TaskFunc = std::function<bool()>;

static bool HandleRecordingShortRaw();
static bool HandleRecordingShortText();
static bool HandleRecordingLongBegin();
static bool HandleRecordingLongDump();
static bool HandleRecordingLongFinish();
static bool HandleRecordingLongFinishNodump();
static bool HandleRecordingLongBeginRecord();
static bool HandleRecordingLongFinishRecord();
static bool HandleOpenSnapshot();
static bool HandleDumpSnapshot();
static bool HandleCloseSnapshot();
static bool SetTraceLevel();
static bool GetTraceLevel();

static bool HandleOptBuffersize(const RunningState& setValue);
static bool HandleOptTraceclock(const RunningState& setValue);
static bool HandleOptTime(const RunningState& setValue);
static bool HandleOptOutput(const RunningState& setValue);
static bool HandleOptOverwrite(const RunningState& setValue);
static bool HandleOptRecord(const RunningState& setValue);
static bool HandleOptFilesize(const RunningState& setValue);
static bool HandleOptTracelevel(const RunningState& setValue);
static bool SetRunningState(const RunningState& setValue);

namespace {
struct TraceArgs {
    std::string tags;
    std::vector<std::string> tagsVec;
    std::string tagGroups;
    std::string clockType;
    std::string level;
    int bufferSize = 0;
    int fileSize = 0;
    bool overwrite = true;
    std::string output;

    int duration = 0;
    bool isCompress = false;
};

struct TraceSysEventParams {
    std::string opt;
    std::string caller;
    std::string tags;
    int duration = 0;
    int bufferSize = 0;
    int fileSize = 0;
    int fileLimit = 0;
    std::string clockType;
    bool isCompress = false;
    bool isRaw = false;
    bool isOverwrite = true;
    int errorCode = 0;
    std::string errorMessage;
};

enum CmdErrorCode {
    OPEN_ROOT_PATH_FAILURE = 2001,
    OPEN_FILE_PATH_FAILURE = 2002,
    TRACING_ON_CLOSED = 2003,
};

const std::map<RunningState, std::string> STATE_INFO = {
    { STATE_NULL, "STATE_NULL" },
    { RECORDING_SHORT_TEXT, "RECORDING_SHORT_TEXT" },
    { RECORDING_SHORT_RAW, "RECORDING_SHORT_RAW" },
    { RECORDING_LONG_BEGIN, "RECORDING_LONG_BEGIN" },
    { RECORDING_LONG_DUMP, "RECORDING_LONG_DUMP" },
    { RECORDING_LONG_FINISH_NODUMP, "RECORDING_LONG_FINISH_NODUMP" },
    { RECORDING_LONG_BEGIN_RECORD, "RECORDING_LONG_BEGIN_RECORD" },
    { RECORDING_LONG_FINISH_RECORD, "RECORDING_LONG_FINISH_RECORD" },
    { SNAPSHOT_START, "SNAPSHOT_START" },
    { SNAPSHOT_DUMP, "SNAPSHOT_DUMP" },
    { SNAPSHOT_STOP, "SNAPSHOT_STOP" },
    { SHOW_HELP, "SHOW_HELP" },
    { SHOW_LIST_CATEGORY, "SHOW_LIST_CATEGORY" },
    { SET_TRACE_LEVEL, "SET_TRACE_LEVEL"},
    { GET_TRACE_LEVEL, "GET_TRACE_LEVEL"},
};

constexpr struct option LONG_OPTIONS[] = {
    { "buffer_size",         required_argument, nullptr, 0 },
    { "trace_clock",         required_argument, nullptr, 0 },
    { "help",                no_argument,       nullptr, 0 },
    { "output",              required_argument, nullptr, 0 },
    { "time",                required_argument, nullptr, 0 },
    { "text",                no_argument,       nullptr, 0 },
    { "raw",                 no_argument,       nullptr, 0 },
    { "trace_begin",         no_argument,       nullptr, 0 },
    { "trace_finish",        no_argument,       nullptr, 0 },
    { "trace_finish_nodump", no_argument,       nullptr, 0 },
    { "record",              no_argument,       nullptr, 0 },
    { "trace_dump",          no_argument,       nullptr, 0 },
    { "list_categories",     no_argument,       nullptr, 0 },
    { "overwrite",           no_argument,       nullptr, 0 },
    { "start_bgsrv",         no_argument,       nullptr, 0 },
    { "dump_bgsrv",          no_argument,       nullptr, 0 },
    { "stop_bgsrv",          no_argument,       nullptr, 0 },
    { "file_size",           required_argument, nullptr, 0 },
    { "trace_level",         required_argument, nullptr, 0 },
    { "get_level",           no_argument,       nullptr, 0 },
    { nullptr,               0,                 nullptr, 0 },
};

const std::unordered_map<RunningState, TaskFunc> TASK_TABLE = {
    {RECORDING_SHORT_RAW, HandleRecordingShortRaw},
    {RECORDING_SHORT_TEXT, HandleRecordingShortText},
    {RECORDING_LONG_BEGIN, HandleRecordingLongBegin},
    {RECORDING_LONG_DUMP, HandleRecordingLongDump},
    {RECORDING_LONG_FINISH, HandleRecordingLongFinish},
    {RECORDING_LONG_FINISH_NODUMP, HandleRecordingLongFinishNodump},
    {RECORDING_LONG_BEGIN_RECORD, HandleRecordingLongBeginRecord},
    {RECORDING_LONG_FINISH_RECORD, HandleRecordingLongFinishRecord},
    {SNAPSHOT_START, HandleOpenSnapshot},
    {SNAPSHOT_DUMP, HandleDumpSnapshot},
    {SNAPSHOT_STOP, HandleCloseSnapshot},
    {SET_TRACE_LEVEL, SetTraceLevel},
    {GET_TRACE_LEVEL, GetTraceLevel}
};

const std::unordered_map<std::string, CommandFunc> COMMAND_TABLE = {
    {"buffer_size", HandleOptBuffersize},
    {"trace_clock", HandleOptTraceclock},
    {"help", SetRunningState},
    {"time",  HandleOptTime},
    {"list_categories", SetRunningState},
    {"output", HandleOptOutput},
    {"overwrite", HandleOptOverwrite},
    {"trace_begin",  SetRunningState},
    {"trace_finish", SetRunningState},
    {"trace_finish_nodump", SetRunningState},
    {"trace_dump", SetRunningState},
    {"record",  HandleOptRecord},
    {"start_bgsrv", SetRunningState},
    {"dump_bgsrv", SetRunningState},
    {"stop_bgsrv", SetRunningState},
    {"text",  SetRunningState},
    {"raw", SetRunningState},
    {"file_size", HandleOptFilesize},
    {"trace_level", HandleOptTracelevel},
    {"get_level",  SetRunningState}
};

std::unordered_map<std::string, RunningState> OPT_MAP = {
    {"buffer_size", STATE_NULL},
    {"trace_clock", STATE_NULL},
    {"help", SHOW_HELP},
    {"time",  STATE_NULL},
    {"list_categories", SHOW_LIST_CATEGORY},
    {"output", STATE_NULL},
    {"overwrite", STATE_NULL},
    {"trace_begin",  RECORDING_LONG_BEGIN},
    {"trace_finish", RECORDING_LONG_FINISH},
    {"trace_finish_nodump", RECORDING_LONG_FINISH_NODUMP},
    {"trace_dump", RECORDING_LONG_DUMP},
    {"record",  STATE_NULL},
    {"start_bgsrv", SNAPSHOT_START},
    {"dump_bgsrv", SNAPSHOT_DUMP},
    {"stop_bgsrv", SNAPSHOT_STOP},
    {"text",  RECORDING_SHORT_TEXT},
    {"raw", RECORDING_SHORT_RAW},
    {"file_size", STATE_NULL},
    {"trace_level", SET_TRACE_LEVEL},
    {"get_level",  GET_TRACE_LEVEL}
};

const std::set<std::string> CLOCK_TYPE = {
    "boot",
    "mono",
    "global",
    "perf",
    "uptime"
};

const unsigned int CHUNK_SIZE = 65536;

// support customization of some parameters
const int KB_PER_MB = 1024;
const int MIN_BUFFER_SIZE = 256;
const int MAX_BUFFER_SIZE = 307200; // 300 MB
const int HM_MAX_BUFFER_SIZE = 1024 * KB_PER_MB; // 1024 MB
constexpr uint32_t DEFAULT_BUFFER_SIZE = 18432; // 18 MB
constexpr unsigned int MAX_OUTPUT_LEN = 255;
const int PAGE_SIZE_KB = 4; // 4 KB
const int MIN_FILE_SIZE = 51200; // 50 MB
const int MAX_FILE_SIZE = 512000; // 500 MB

std::string g_traceRootPath;
std::shared_ptr<OHOS::HiviewDFX::UCollectClient::TraceCollector> g_traceCollector;
TraceArgs g_traceArgs;
TraceSysEventParams g_traceSysEventParams;
bool g_needSysEvent = false;
RunningState g_runningState = STATE_NULL;
}

#ifdef HITRACE_UNITTEST
void Reset()
{
    optind = 0;
    opterr = 1;
    optopt = 0;
    optarg = nullptr;
    g_traceRootPath = "";
    g_traceCollector = nullptr;
    g_needSysEvent = false;
    g_runningState = STATE_NULL;
    g_traceSysEventParams = {};
    g_traceArgs = {};
}
#endif

static void SetTraceSysEventParams()
{
    g_needSysEvent = true;
    g_traceSysEventParams.caller = "CMD";
}

static void ConsoleLog(const std::string& logInfo)
{
    // get localtime
    time_t currentTime;
    time(&currentTime);
    struct tm timeInfo = {};
    const int bufferSize = 20;
    char timeStr[bufferSize] = {0};
    localtime_r(&currentTime, &timeInfo);
    strftime(timeStr, bufferSize, "%Y/%m/%d %H:%M:%S", &timeInfo);
    std::cout << timeStr << " " << logInfo << std::endl;
}

static std::string GetStateInfo(const RunningState state)
{
    if (STATE_INFO.find(state) == STATE_INFO.end()) {
        ConsoleLog("error: running_state is invalid.");
        return "";
    }
    return STATE_INFO.at(state);
}

static bool WriteStrToFile(const std::string& filename, const std::string& str)
{
    std::ofstream out;
    std::string inSpecPath =
        OHOS::HiviewDFX::Hitrace::CanonicalizeSpecPath((g_traceRootPath + filename).c_str());
    out.open(inSpecPath, std::ios::out);
    if (out.fail()) {
        ConsoleLog("error: open " + inSpecPath + " failed.");
        return false;
    }
    out << str;
    if (out.bad()) {
        ConsoleLog("error: can not write " + inSpecPath);
        out.close();
        return false;
    }
    out.flush();
    out.close();
    return true;
}

static bool SetFtraceEnabled(const std::string& path, bool enabled)
{
    return WriteStrToFile(path, enabled ? "1" : "0");
}

static bool SetProperty(const std::string& property, const std::string& value)
{
    return SetPropertyInner(property, value);
}

static bool SetTraceTagsEnabled(uint64_t tags)
{
    std::string value = std::to_string(tags);
    return SetProperty(TRACE_TAG_ENABLE_FLAGS, value);
}

static void ShowListCategory()
{
    g_traceSysEventParams.opt = "ShowListCategory";
    printf("  %18s   description:\n", "tagName:");
    auto traceTags = TraceJsonParser::Instance().GetAllTagInfos();
    for (auto it = traceTags.begin(); it != traceTags.end(); ++it) {
        printf("  %18s - %s\n", it->first.c_str(), it->second.description.c_str());
    }
}

static void ShowHelp(const std::string& cmd)
{
    g_traceSysEventParams.opt = "ShowHelp";
    printf("usage: %s [options] [categories...]\n", cmd.c_str());
    printf("options include:\n"
           "  -b N                   Set the size of the buffer (KB) for storing and reading traces.\n"
           "                         The default buffer size is 18432 KB.\n"
           "  --buffer_size N        Like \"-b N\".\n"
           "  -l                     List available hitrace categories.\n"
           "  --list_categories      Like \"-l\".\n"
           "  -t N                   Set the hitrace running duration in seconds (5s by default), which depends on\n"
           "                         the time required for analysis.\n"
           "  --time N               Like \"-t N\".\n"
           "  --trace_clock clock    Sets the type of the clock for adding a timestamp to a trace, which can be\n"
           "                         boot (default), global, mono, uptime, or perf.\n"
           "  --trace_begin          Start capturing traces.\n"
           "  --trace_dump           Dump traces to a specified path (stdout by default).\n"
           "  --trace_finish         Stop capturing traces and dumps traces to a specified path (stdout by default).\n"
           "  --trace_finish_nodump  Stop capturing traces and not dumps traces.\n"
           "  --record               Enable or disable long-term trace collection tasks in conjunction with\n"
           "                         \"--trace_begin\" and \"--trace_finish\".\n"
           "  --overwrite            Set the action to take when the buffer is full. If this option is used,\n"
           "                         the latest traces are discarded; if this option is not used (default setting),\n"
           "                         the earliest traces are discarded.\n"
           "  -o filename            Specifies the name of the target file (stdout by default).\n"
           "  --output filename      Like \"-o filename\".\n"
           "  -z                     Compresses a captured trace.\n"
           "  --text                 Specify the output format of trace as text.\n"
           "  --raw                  Specify the output format of trace as raw trace, the default format is text.\n"
           "  --start_bgsrv          Enable trace_service in snapshot mode.\n"
           "  --dump_bgsrv           Trigger the dump trace task of the trace_service.\n"
           "  --stop_bgsrv           Disable trace_service in snapshot mode.\n"
           "  --file_size            Sets the size of the raw trace (KB). The default file size is 102400 KB.\n"
           "                         Only effective in raw trace mode\n"
           "  --trace_level level    Set the system parameter \"persist.hitrace.level.threshold\", which can control\n"
           "                         the level threshold of tracing. Valid values for \"level\" include\n"
           "                         D or Debug, I or Info, C or Critical, M or Commercial.\n"
           "  --get_level            Query the system parameter \"persist.hitrace.level.threshold\",\n"
           "                         which can control the level threshold of tracing.\n"
    );
}

static bool CheckTraceLevel(const std::string& arg)
{
    static const std::map<std::string, std::string> traceLevels = {
        {"D", "0"}, {"Debug", "0"},
        {"I", "1"}, {"Info", "1"},
        {"C", "2"}, {"Critical", "2"},
        {"M", "3"}, {"Commercial", "3"}
    };

    auto it = traceLevels.find(arg);
    if (it != traceLevels.end()) {
        g_traceArgs.level = it->second;
        return true;
    } else {
        ConsoleLog("error: trace level is illegal input. eg: \"--trace_level I\", \"--trace_level Info\".");
        return false;
    }
}

static bool SetTraceLevel()
{
    bool isSuccess = OHOS::system::SetParameter(TRACE_LEVEL_THRESHOLD, g_traceArgs.level);
    if (!isSuccess) {
        ConsoleLog("error: failed to set trace level.");
    } else {
        ConsoleLog("success to set trace level.");
    }
    return isSuccess;
}

static bool GetTraceLevel()
{
    std::string level = OHOS::system::GetParameter(TRACE_LEVEL_THRESHOLD, "");
    static const std::map<std::string, std::string> traceLevels = {
        {"0", "Debug"},
        {"1", "Info"},
        {"2", "Critical"},
        {"3", "Commercial"},
    };

    auto it = traceLevels.find(level);
    if (it != traceLevels.end()) {
        ConsoleLog("the current trace level threshold is " + it->second);
        return true;
    } else {
        ConsoleLog("error: get trace level threshold failed, level(" + level + ") cannot be parsed.");
        return false;
    }
}

template <typename T>
inline bool StrToNum(const std::string& sString, T &tX)
{
    std::istringstream iStream(sString);
    return (iStream >> tX) && iStream.eof();
}

static bool SetRunningState(const RunningState& setValue)
{
    if (g_runningState != STATE_NULL) {
        ConsoleLog("error: the parameter is set incorrectly, " + GetStateInfo(g_runningState) +
                   " and " + GetStateInfo(setValue) + " cannot coexist.");
        return false;
    }
    g_runningState = setValue;
    return true;
}

static bool CheckOutputFile(const char* path)
{
    struct stat buf;
    size_t len = strnlen(path, MAX_OUTPUT_LEN);
    if (len == MAX_OUTPUT_LEN || len < 1 || (stat(path, &buf) == 0 && (buf.st_mode & S_IFDIR))) {
        ConsoleLog("error: output file is illegal");
        return false;
    }
    g_traceArgs.output = path;
    return true;
}

static bool CheckClock(const char* clock)
{
    if (clock == nullptr) {
        return false;
    }
    std::string clockType(clock);
    if (CLOCK_TYPE.count(clockType) == 0) {
        return false;
    }
    return true;
}

static bool HandleOptBuffersize(const RunningState& setValue)
{
    if (optarg == nullptr) {
        return false;
    }
    int bufferSizeKB = 0;
    int maxBufferSizeKB = MAX_BUFFER_SIZE;
    if (IsHmKernel()) {
        maxBufferSizeKB = HM_MAX_BUFFER_SIZE;
    }
    bool isTrue = true;
    if (!StrToNum(optarg, bufferSizeKB)) {
        ConsoleLog("error: buffer size is illegal input. eg: \"--buffer_size 18432\".");
        isTrue = false;
    } else if (bufferSizeKB < MIN_BUFFER_SIZE || bufferSizeKB > maxBufferSizeKB) {
        ConsoleLog("error: buffer size must be from 256 KB to " + std::to_string(maxBufferSizeKB / KB_PER_MB) +
            " MB. eg: \"--buffer_size 18432\".");
        isTrue = false;
    }
    g_traceArgs.bufferSize = bufferSizeKB / PAGE_SIZE_KB * PAGE_SIZE_KB;
    return isTrue;
}

static bool HandleOptTraceclock(const RunningState& setValue)
{
    bool isTrue = true;
    if (CheckClock(optarg)) {
        g_traceArgs.clockType = optarg;
    } else {
        ConsoleLog("error: \"--trace_clock\" is illegal input. eg: \"--trace_clock boot\".");
        isTrue = false;
    }
    return isTrue;
}

static bool HandleOptTime(const RunningState& setValue)
{
    if (optarg == nullptr) {
        return false;
    }
    bool isTrue = true;
    if (!StrToNum(optarg, g_traceArgs.duration)) {
        ConsoleLog("error: the time is illegal input. eg: \"--time 5\".");
        isTrue = false;
    } else if (g_traceArgs.duration < 1) {
        ConsoleLog("error: \"-t " + std::string(optarg) + "\" to be greater than zero. eg: \"--time 5\".");
        isTrue = false;
    }
    return isTrue;
}

static bool HandleOptOverwrite(const RunningState& setValue)
{
    g_traceArgs.overwrite = false;
    return true;
}

static bool HandleOptRecord(const RunningState& setValue)
{
    bool isTrue = true;
    if (g_runningState == RECORDING_LONG_BEGIN) {
        g_runningState = RECORDING_LONG_BEGIN_RECORD;
    } else if (g_runningState == RECORDING_LONG_FINISH) {
        g_runningState = RECORDING_LONG_FINISH_RECORD;
    } else {
        ConsoleLog("error: \"--record\" is set incorrectly. eg: \"--trace_begin --record\","
                   " \"--trace_finish --record\".");
        isTrue = false;
    }
    return isTrue;
}

static bool HandleOptFilesize(const RunningState& setValue)
{
    if (optarg == nullptr) {
        return false;
    }
    bool isTrue = true;
    int fileSizeKB = 0;
    if (!StrToNum(optarg, fileSizeKB)) {
        ConsoleLog("error: file size is illegal input. eg: \"--file_size 102400\".");
        isTrue = false;
    } else if (fileSizeKB < MIN_FILE_SIZE || fileSizeKB > MAX_FILE_SIZE) {
        ConsoleLog("error: file size must be from 50 MB to 500 MB. eg: \"--file_size 102400\".");
        isTrue = false;
    }
    g_traceArgs.fileSize = fileSizeKB;
    return isTrue;
}

static bool HandleOptTracelevel(const RunningState& setValue)
{
    bool isTrue = true;
    isTrue = SetRunningState(setValue);
    if (!CheckTraceLevel(optarg)) {
        isTrue = false;
    }
    return isTrue;
}

static bool HandleOptOutput(const RunningState& setValue)
{
    bool isTrue = CheckOutputFile(optarg);
    return isTrue;
}

static bool ParseLongOpt(const std::string& cmd, int optionIndex)
{
    std::string str(LONG_OPTIONS[optionIndex].name);
    bool isTrue = true;
    auto it = COMMAND_TABLE.find(str);
    if (it != COMMAND_TABLE.end()) {
        isTrue = it->second(OPT_MAP[str]);
    }
    return isTrue;
}

static bool SetBufferSize()
{
    if (optarg == nullptr) {
        return false;
    }
    bool isTrue = true;
    int bufferSizeKB = 0;
    int maxBufferSizeKB = MAX_BUFFER_SIZE;
    if (IsHmKernel()) {
        maxBufferSizeKB = HM_MAX_BUFFER_SIZE;
    }
    if (!StrToNum(optarg, bufferSizeKB)) {
        ConsoleLog("error: buffer size is illegal input. eg: \"--buffer_size 18432\".");
        isTrue = false;
    } else if (bufferSizeKB < MIN_BUFFER_SIZE || bufferSizeKB > maxBufferSizeKB) {
        ConsoleLog("error: buffer size must be from 256 KB to " + std::to_string(maxBufferSizeKB / KB_PER_MB) +
        " MB. eg: \"--buffer_size 18432\".");
        isTrue = false;
    }
    g_traceArgs.bufferSize = bufferSizeKB / PAGE_SIZE_KB * PAGE_SIZE_KB;
    return isTrue;
}

static bool ParseOpt(int opt, char** argv, int optIndex)
{
    bool isTrue = true;
    switch (opt) {
        case 'b': {
            isTrue = SetBufferSize();
            break;
        }
        case 'h':
            isTrue = SetRunningState(SHOW_HELP);
            break;
        case 'l':
            isTrue = SetRunningState(SHOW_LIST_CATEGORY);
            break;
        case 't': {
            if (optarg == nullptr) {
                isTrue = false;
                break;
            }
            if (!StrToNum(optarg, g_traceArgs.duration)) {
                ConsoleLog("error: the time is illegal input. eg: \"--time 5\".");
                isTrue = false;
            } else if (g_traceArgs.duration < 1) {
                ConsoleLog("error: \"-t " + std::string(optarg) + "\" to be greater than zero. eg: \"--time 5\".");
                isTrue = false;
            }
            break;
        }
        case 'o': {
            isTrue = CheckOutputFile(optarg);
            break;
        }
        case 'z':
            g_traceArgs.isCompress = true;
            break;
        case 0: // long options
            isTrue = ParseLongOpt(argv[0], optIndex);
            break;
        case '?':
            isTrue = false;
            break;
        default:
            break;
    }
    return isTrue;
}

static bool AddTagItems(int argc, char** argv)
{
    auto traceTags = TraceJsonParser::Instance().GetAllTagInfos();
    for (int i = optind; i < argc; i++) {
        std::string tag = std::string(argv[i]);
        if (traceTags.find(tag) == traceTags.end()) {
            std::string errorInfo = "error: " + tag + " is not support category on this device.";
            ConsoleLog(errorInfo);
            return false;
        }
        if (i == optind) {
            g_traceArgs.tagsVec.clear();
            g_traceArgs.tags = tag;
        } else {
            g_traceArgs.tags += ("," + tag);
        }
        g_traceArgs.tagsVec.emplace_back(tag);
    }
    return true;
}

static bool HandleOpt(int argc, char** argv)
{
    bool isTrue = true;
    int opt = 0;
    int optionIndex = 0;
    std::string shortOption = "b:c:hlo:t:z";
    int argcSize = argc;
    while (isTrue && argcSize-- > 0) {
        opt = getopt_long(argc, argv, shortOption.c_str(), LONG_OPTIONS, &optionIndex);
        if (opt < 0 && (!AddTagItems(argc, argv))) {
            isTrue = false;
            break;
        }
        isTrue = ParseOpt(opt, argv, optionIndex);
    }

    return isTrue;
}

static void StopTrace()
{
    const int napTime = 10000;
    usleep(napTime);
    SetTraceTagsEnabled(0);
    SetFtraceEnabled(TRACING_ON_NODE, false);
}

static void DumpCompressedTrace(int traceFd, int outFd)
{
    z_stream zs { nullptr };
    int flush = Z_NO_FLUSH;
    ssize_t bytesWritten;
    ssize_t bytesRead;
    if (memset_s(&zs, sizeof(zs), 0, sizeof(zs)) != EOK) {
        ConsoleLog("error: zip stream buffer init failed.");
        return;
    }
    int ret = deflateInit(&zs, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        ConsoleLog("error: initializing zlib failed ret " + std::to_string(ret));
        return;
    }
    std::unique_ptr<uint8_t[]>  in = std::make_unique<uint8_t[]>(CHUNK_SIZE);
    std::unique_ptr<uint8_t[]>  out = std::make_unique<uint8_t[]>(CHUNK_SIZE);
    if (!in || !out) {
        ConsoleLog("error: couldn't allocate buffers.");
        return;
    }
    zs.next_out = reinterpret_cast<Bytef*>(out.get());
    zs.avail_out = CHUNK_SIZE;

    do {
        if (zs.avail_in == 0 && flush == Z_NO_FLUSH) {
            bytesRead = TEMP_FAILURE_RETRY(read(traceFd, in.get(), CHUNK_SIZE));
            if (bytesRead == 0) {
                flush = Z_FINISH;
            } else if (bytesRead == -1) {
                ConsoleLog("error: reading trace, errno " + std::to_string(errno));
                break;
            } else {
                zs.next_in = reinterpret_cast<Bytef*>(in.get());
                zs.avail_in = bytesRead;
            }
        }
        if (zs.avail_out == 0) {
            bytesWritten = TEMP_FAILURE_RETRY(write(outFd, out.get(), CHUNK_SIZE));
            if (bytesWritten < static_cast<ssize_t>(CHUNK_SIZE)) {
                ConsoleLog("error: writing deflated trace, errno " + std::to_string(errno));
                break;
            }
            zs.next_out = reinterpret_cast<Bytef*>(out.get());
            zs.avail_out = CHUNK_SIZE;
        }
        ret = deflate(&zs, flush);
        if (flush == Z_FINISH && ret == Z_STREAM_END) {
            size_t have = CHUNK_SIZE - zs.avail_out;
            bytesWritten = TEMP_FAILURE_RETRY(write(outFd, out.get(), have));
            if (bytesWritten < static_cast<ssize_t>(have)) {
                ConsoleLog("error: writing deflated trace, errno " + std::to_string(errno));
            }
            break;
        } else if (ret != Z_OK) {
            if (ret == Z_ERRNO) {
                ConsoleLog("error: deflate failed with errno " + std::to_string(errno));
            } else {
                ConsoleLog("error: deflate failed return " + std::to_string(ret));
            }
            break;
        }
    } while (ret == Z_OK);

    ret = deflateEnd(&zs);
    if (ret != Z_OK) {
        ConsoleLog("error: cleaning up zlib return " + std::to_string(ret));
    }
}

static void DumpTrace()
{
    std::string tracePath = g_traceRootPath + TRACE_NODE;
    std::string traceSpecPath = CanonicalizeSpecPath(tracePath.c_str());
    int traceFd = open(traceSpecPath.c_str(), O_RDONLY);
    if (traceFd == -1) {
        ConsoleLog("error: opening " + tracePath + ", errno: " + std::to_string(errno));
        g_traceSysEventParams.errorCode = OPEN_ROOT_PATH_FAILURE;
        g_traceSysEventParams.errorMessage = "error: opening " + tracePath + ", errno: " +
            std::to_string(errno);
        return;
    }

    int outFd = STDOUT_FILENO;
    if (g_traceArgs.output.size() > 0) {
        std::string outSpecPath = CanonicalizeSpecPath(g_traceArgs.output.c_str());
        outFd = open(outSpecPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }

    if (outFd == -1) {
        ConsoleLog("error: opening " + g_traceArgs.output + ", errno: " + std::to_string(errno));
        g_traceSysEventParams.errorCode = OPEN_FILE_PATH_FAILURE;
        g_traceSysEventParams.errorMessage = "error: opening " + g_traceArgs.output + ", errno: " +
            std::to_string(errno);
        close(traceFd);
        return;
    }

    ssize_t bytesWritten;
    ssize_t bytesRead;
    if (g_traceArgs.isCompress) {
        DumpCompressedTrace(traceFd, outFd);
    } else {
        const int blockSize = 4096;
        char buffer[blockSize];
        do {
            bytesRead = TEMP_FAILURE_RETRY(read(traceFd, buffer, blockSize));
            if ((bytesRead == 0) || (bytesRead == -1)) {
                break;
            }
            bytesWritten = TEMP_FAILURE_RETRY(write(outFd, buffer, bytesRead));
            if (bytesWritten > 0) {
                g_traceSysEventParams.fileSize += bytesWritten;
            }
        } while (bytesWritten > 0);
    }

    g_traceSysEventParams.fileSize = g_traceSysEventParams.fileSize / KB_PER_MB;
    if (outFd != STDOUT_FILENO) {
        ConsoleLog("trace read done, output: " + g_traceArgs.output);
        close(outFd);
    }
    close(traceFd);
}

static void ReloadTraceArgs(std::vector<std::string>& tagsVec, HiviewTraceParam& hiviewTraceParam)
{
    if (g_traceArgs.tags.size() == 0) {
        ConsoleLog("error: tag is empty, please add.");
        return;
    }
    std::string args = "tags:" + g_traceArgs.tags;
    tagsVec = g_traceArgs.tagsVec;
    if (g_traceArgs.bufferSize > 0) {
        hiviewTraceParam.bufferSize = static_cast<uint32_t>(g_traceArgs.bufferSize);
        args += (" bufferSize:" + std::to_string(g_traceArgs.bufferSize));
    } else {
        hiviewTraceParam.bufferSize = DEFAULT_BUFFER_SIZE;
        args += (" bufferSize:" + std::to_string(DEFAULT_BUFFER_SIZE));
    }

    if (g_traceArgs.clockType.size() > 0) {
        hiviewTraceParam.clockType = g_traceArgs.clockType;
        args += (" clockType:" + g_traceArgs.clockType);
    }

    if (g_traceArgs.overwrite) {
        args += " overwrite:";
        args += "1";
    } else {
        args += " overwrite:";
        args += "0";
    }
    hiviewTraceParam.isOverWrite = g_traceArgs.overwrite;

    if (g_traceArgs.fileSize > 0) {
        if (g_runningState == RECORDING_SHORT_RAW || g_runningState == RECORDING_LONG_BEGIN_RECORD) {
            hiviewTraceParam.fileSizeLimit = static_cast<uint32_t>(g_traceArgs.fileSize);
            args += (" fileSize:" + std::to_string(g_traceArgs.fileSize));
        } else {
            ConsoleLog("warning: The current state does not support specifying the file size, file size: " +
                std::to_string(g_traceArgs.fileSize) + " is invalid.");
        }
    }

    if (g_runningState != RECORDING_SHORT_TEXT) {
        ConsoleLog("args: " + args);
    }
}

static bool HandleRecordingShortRaw()
{
    std::vector<std::string> tags = {};
    HiviewTraceParam hiviewTraceParam = {
        .bufferSize = 0,
        .clockType = "boot",
        .isOverWrite = true,
        .fileSizeLimit = 0
    };
    ReloadTraceArgs(tags, hiviewTraceParam);
    if (g_traceArgs.output.size() > 0) {
        ConsoleLog("warning: The current state does not support specifying the output file path, " +
                   g_traceArgs.output + " is invalid.");
    }
    auto openRet = g_traceCollector->OpenTrace(tags, hiviewTraceParam, {});
    if (openRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: OpenRecording failed, errorCode(" + std::to_string(openRet.retCode) +")");
        return false;
    }

    auto recOnRet = g_traceCollector->RecordingOn();
    if (recOnRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: RecordingOn failed, errorCode(" + std::to_string(recOnRet.retCode) +")");
        g_traceCollector->Close();
        return false;
    }
    ConsoleLog("start capture, please wait " + std::to_string(g_traceArgs.duration) + "s ...");
    sleep(g_traceArgs.duration);

    auto recOffRet = g_traceCollector->RecordingOff();
    if (recOffRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: RecordingOff failed, errorCode(" + std::to_string(recOffRet.retCode) +")");
        g_traceCollector->Close();
        return false;
    }
    ConsoleLog("capture done, output files:");
    for (std::string item : recOffRet.data) {
        std::cout << "    " << item << std::endl;
    }
    auto closeRet = g_traceCollector->Close();
    if (closeRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: Trace Close failed, errorCode(" + std::to_string(closeRet.retCode) +")");
    }
    return true;
}

static bool HandleRecordingShortText()
{
    std::vector<std::string> tags = {};
    HiviewTraceParam hiviewTraceParam = {
        .bufferSize = 0,
        .clockType = "boot",
        .isOverWrite = true,
        .fileSizeLimit = 0
    };
    ReloadTraceArgs(tags, hiviewTraceParam);
    auto openRet = g_traceCollector->OpenTrace(tags, hiviewTraceParam, {});
    if (openRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: OpenRecording failed, errorCode(" + std::to_string(openRet.retCode) +")");
        return false;
    }
    ConsoleLog("start capture, please wait " + std::to_string(g_traceArgs.duration) + "s ...");
    sleep(g_traceArgs.duration);

    MarkClockSync(g_traceRootPath);
    StopTrace();

    if (g_traceArgs.output.size() > 0) {
        ConsoleLog("capture done, start to read trace.");
    }
    g_traceSysEventParams.opt = "DumpTextTrace";
    DumpTrace();

    auto closeRet = g_traceCollector->Close();
    if (closeRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: TraceFinish failed, errorCode(" + std::to_string(closeRet.retCode) +")");
    } else {
        ConsoleLog("TraceFinish done.");
    }
    return true;
}

static bool HandleRecordingLongBegin()
{
    std::vector<std::string> tags = {};
    HiviewTraceParam hiviewTraceParam = {
        .bufferSize = 0,
        .clockType = "boot",
        .isOverWrite = true,
        .fileSizeLimit = 0
    };
    ReloadTraceArgs(tags, hiviewTraceParam);
    if (g_traceArgs.output.size() > 0) {
        ConsoleLog("warning: The current state does not support specifying the output file path, " +
                   g_traceArgs.output + " is invalid.");
    }
    auto openRet = g_traceCollector->OpenTrace(tags, hiviewTraceParam, {});
    if (openRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: OpenRecording failed, errorCode(" + std::to_string(openRet.retCode) +")");
        return false;
    }
    ConsoleLog("OpenRecording done.");
    return true;
}

static bool HandleRecordingLongDump()
{
    g_traceSysEventParams.opt = "DumpTextTrace";
    if (!IsTracingOn(g_traceRootPath)) {
        g_traceSysEventParams.errorCode = TRACING_ON_CLOSED;
        g_traceSysEventParams.errorMessage = "Warning: tracing on is closed, no trace can be read.";
        ConsoleLog("Warning: tracing on is closed, no trace can be read.");
        return false;
    }
    MarkClockSync(g_traceRootPath);
    ConsoleLog("start to read trace.");
    DumpTrace();
    return true;
}

static bool HandleRecordingLongFinish()
{
    g_traceSysEventParams.opt = "DumpTextTrace";
    if (!IsTracingOn(g_traceRootPath)) {
        g_traceSysEventParams.errorCode = TRACING_ON_CLOSED;
        g_traceSysEventParams.errorMessage = "Warning: tracing on is closed, no trace can be read.";
        ConsoleLog("Warning: tracing on is closed, no trace can be read.");
        return false;
    }
    MarkClockSync(g_traceRootPath);
    StopTrace();
    ConsoleLog("start to read trace.");
    DumpTrace();
    auto closeRet = g_traceCollector->Close();
    if (closeRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: Trace Close failed, errorCode(" + std::to_string(closeRet.retCode) +")");
    } else {
        ConsoleLog("Trace Closed.");
    }
    return true;
}

static bool HandleRecordingLongFinishNodump()
{
    auto closeRet = g_traceCollector->Close();
    if (closeRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: Trace Close failed, errorCode(" + std::to_string(closeRet.retCode) +")");
    } else {
        ConsoleLog("end capture trace.");
    }

    return true;
}

static bool HandleRecordingLongBeginRecord()
{
    std::vector<std::string> tags = {};
    HiviewTraceParam hiviewTraceParam = {
        .bufferSize = 0,
        .clockType = "boot",
        .isOverWrite = true,
        .fileSizeLimit = 0
    };
    ReloadTraceArgs(tags, hiviewTraceParam);
    if (g_traceArgs.output.size() > 0) {
        ConsoleLog("warning: The current state does not support specifying the output file path, " +
                   g_traceArgs.output + " is invalid.");
    }
    auto openRet = g_traceCollector->OpenTrace(tags, hiviewTraceParam, {});
    if (openRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: OpenRecording failed, errorCode(" + std::to_string(openRet.retCode) +")");
        return false;
    }

    auto recOnRet = g_traceCollector->RecordingOn();
    if (recOnRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: RecordingOn failed, errorCode(" + std::to_string(recOnRet.retCode) +")");
        g_traceCollector->Close();
        return false;
    }
    ConsoleLog("trace capturing.");
    return true;
}

static bool HandleRecordingLongFinishRecord()
{
    auto recOffRet = g_traceCollector->RecordingOff();
    if (recOffRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: RecordingOff failed, errorCode(" + std::to_string(recOffRet.retCode) +")");
        return false;
    }
    ConsoleLog("capture done, output files:");
    for (std::string item : recOffRet.data) {
        std::cout << "    " << item << std::endl;
    }
    auto closeRet = g_traceCollector->Close();
    if (closeRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: Trace Close failed, errorCode(" + std::to_string(closeRet.retCode) +")");
    }
    return true;
}

static bool HandleOpenSnapshot()
{
    g_needSysEvent = false;
    const std::vector<std::string> tags = {
        "net", "dsched", "graphic", "multimodalinput", "dinput", "ark", "ace", "window",
        "zaudio", "daudio", "zmedia", "dcamera", "zcamera", "dhfwk", "app", "gresource",
        "ability", "power", "samgr", "ffrt", "nweb", "hdf", "virse", "workq", "ipa",
        "sched", "freq", "disk", "sync", "binder", "mmc", "membus", "load"
    };
    HiviewTraceParam hiviewTraceParam = {
        .bufferSize = 0,
        .clockType = "boot",
        .isOverWrite = true,
        .fileSizeLimit = DEFAULT_FILE_SIZE
    };
    auto openRet = g_traceCollector->OpenTrace(tags, hiviewTraceParam, {});
    if (openRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: OpenSnapshot failed, errorCode(" + std::to_string(openRet.retCode) +")");
        return false;
    }
    ConsoleLog("OpenSnapshot done.");
    return true;
}

static bool HandleDumpSnapshot()
{
    g_needSysEvent = false;
    bool isSuccess = true;
    auto dumpRet = g_traceCollector->DumpSnapshot(OHOS::HiviewDFX::UCollect::TraceClient::COMMAND);
    if (dumpRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: DumpSnapshot failed, errorCode(" + std::to_string(dumpRet.retCode) +")");
        isSuccess = false;
    } else {
        ConsoleLog("DumpSnapshot done, output:");
        for (std::string item : dumpRet.data) {
            std::cout << "    " << item << std::endl;
        }
    }
    return isSuccess;
}

static bool HandleCloseSnapshot()
{
    g_needSysEvent = false;
    bool isSuccess = true;
    auto closeRet = g_traceCollector->Close();
    if (closeRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: CloseSnapshot failed, errorCode(" + std::to_string(closeRet.retCode) +")");
        isSuccess = false;
    } else {
        ConsoleLog("CloseSnapshot done.");
    }
    return isSuccess;
}

static void InterruptExit(int signo)
{
    /**
     * trace reset.
    */
    _exit(-1);
}

static void RecordSysEvent()
{
    if (!g_needSysEvent) {
        return;
    }
    HiSysEventParam params[] = {
        {"OPT",           HISYSEVENT_STRING, {.s = const_cast<char*>(g_traceSysEventParams.opt.c_str())},          0},
        {"CALLER",        HISYSEVENT_STRING, {.s = const_cast<char*>(g_traceSysEventParams.caller.c_str())},       0},
        {"TRACE_TAG",     HISYSEVENT_STRING, {.s = const_cast<char*>(g_traceSysEventParams.tags.c_str())},         0},
        {"DURATION",      HISYSEVENT_INT32,  {.i32 = g_traceSysEventParams.duration},                              0},
        {"BUFFER_SIZE",   HISYSEVENT_INT32,  {.i32 = g_traceSysEventParams.bufferSize},                            0},
        {"FILE_LIMIT",    HISYSEVENT_INT32,  {.i32 = g_traceSysEventParams.fileLimit},                             0},
        {"FILE_SIZE",     HISYSEVENT_INT32,  {.i32 = g_traceSysEventParams.fileSize},                              0},
        {"CLOCK_TYPE",    HISYSEVENT_STRING, {.s = const_cast<char*>(g_traceSysEventParams.clockType.c_str())},    0},
        {"IS_COMPRESSED", HISYSEVENT_BOOL,   {.b = g_traceSysEventParams.isCompress},                              0},
        {"IS_RAW",        HISYSEVENT_BOOL,   {.b = g_traceSysEventParams.isRaw},                                   0},
        {"IS_OVERWRITE",  HISYSEVENT_BOOL,   {.b = g_traceSysEventParams.isOverwrite},                             0},
        {"ERROR_CODE",    HISYSEVENT_INT32,  {.i32 = g_traceSysEventParams.errorCode},                             0},
        {"ERROR_MESSAGE", HISYSEVENT_STRING, {.s = const_cast<char*>(g_traceSysEventParams.errorMessage.c_str())}, 0},
    };
    int ret = OH_HiSysEvent_Write("PROFILER", "HITRACE_USAGE",
        HISYSEVENT_BEHAVIOR, params, sizeof(params) / sizeof(params[0]));
    if (ret != 0) {
        HILOG_ERROR(LOG_CORE, "HiSysEventWrite failed, ret is %{public}d", ret);
    }
}

static bool InitAndCheckArgs(int argc, char**argv)
{
    if (!IsDeveloperMode()) {
        ConsoleLog("error: not in developermode, exit");
        return false;
    }

    if (argc < 0 || argc > 256) { // 256 : max input argument counts
        ConsoleLog("error: the number of input arguments exceeds the upper limit.");
        return false;
    }

    g_traceCollector = OHOS::HiviewDFX::UCollectClient::TraceCollector::Create();
    if (g_traceCollector == nullptr) {
        ConsoleLog("error: traceCollector create failed, exit.");
        return false;
    }

    (void)signal(SIGKILL, InterruptExit);
    (void)signal(SIGINT, InterruptExit);

    if (!IsTraceMounted(g_traceRootPath)) {
        ConsoleLog("error: trace isn't mounted, exit.");
        return false;
    }

    if (!HandleOpt(argc, argv)) {
        ConsoleLog("error: parsing args failed, exit.");
        return false;
    }

    if (g_runningState == STATE_NULL) {
        g_runningState = RECORDING_SHORT_TEXT;
    }
    if (g_runningState != RECORDING_SHORT_TEXT && g_runningState != RECORDING_LONG_DUMP &&
        g_runningState != RECORDING_LONG_FINISH) {
        ConsoleLog(std::string(argv[0]) + " enter, running_state is " + GetStateInfo(g_runningState));
    }

    SetTraceSysEventParams();
    return true;
}

#ifdef HITRACE_UNITTEST
int HiTraceCMDTestMain(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    if (!InitAndCheckArgs(argc, argv)) {
        return -1;
    }

    bool isSuccess = true;
    auto it = TASK_TABLE.find(g_runningState);
    if (it != TASK_TABLE.end()) {
        isSuccess = it->second();
    } else {
        switch (g_runningState) {
            case SHOW_HELP:
                ShowHelp(argv[0]);
                break;
            case SHOW_LIST_CATEGORY:
                ShowListCategory();
                break;
            default:
                ShowHelp(argv[0]);
                isSuccess = false;
                break;
        }
    }
    RecordSysEvent();
    return isSuccess ? 0 : -1;
}