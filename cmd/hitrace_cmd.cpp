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
#include <cstdint>
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
#include "smart_fd.h"
#include "trace_file_utils.h"
#include "trace_collector_client.h"
#include "trace_json_parser.h"
#include "hitrace_dump.h"
#include "cJSON.h"

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
    CONFIG_BOOT_TRACE = 35,  // --boot_trace
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
static bool HandleBootTraceConfig();
static bool HandleOptBootTrace(const RunningState& setValue);
static bool HandleOptRepeat(const RunningState& setValue);

static bool HandleOptBuffersize(const RunningState& setValue);
static bool HandleOptTraceclock(const RunningState& setValue);
static bool HandleOptTime(const RunningState& setValue);
static bool HandleOptOutput(const RunningState& setValue);
static bool HandleOptOverwrite(const RunningState& setValue);
static bool HandleOptRecord(const RunningState& setValue);
static bool HandleOptFilesize(const RunningState& setValue);
static bool HandleOptTotalsize(const RunningState& setValue);
static bool HandleOptTracelevel(const RunningState& setValue);
static bool HandleOptBootFilePrefix(const RunningState& setValue);
static bool HandleOptBootIncrement(const RunningState& setValue);
static bool IsBootTraceActiveFlagOn();
static void ClearBootTraceActiveFlagIfNeeded(bool isActive);
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
    int64_t totalSize = 0;
    bool overwrite = true;
    std::string output;

    int duration = 0;
    bool isCompress = false;
    // --repeat for boot_trace, range [1, 100]
    int remainingCount = 1;
    std::string bootFilePrefix; // prefix for boot trace file name
    // --overwrite for boot_trace: overwrite previous file each time, default off
    bool bootTraceOverwrite = false;
    // --increment for boot_trace: append _<n> to output stem; cfg increment_index -1 = off
    bool bootTraceIncrement = false;
};

struct BootTraceConfig {
    int durationSec = 0;
    int bufferSizeKb = 0;
    int fileSizeKb = 0;
    int incrementIndex = -1; // -1 disabled; >=0 suffix index for output filename
    std::vector<std::string> kernelTags;
    std::vector<std::string> userTags;
    std::string clockType;
    std::string filePrefix;
    bool overwrite = false;
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
    { CONFIG_BOOT_TRACE, "CONFIG_BOOT_TRACE"},
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
    { "total_size",          required_argument, nullptr, 0 },
    { "trace_level",         required_argument, nullptr, 0 },
    { "get_level",           no_argument,       nullptr, 0 },
    { "boot_trace",          no_argument,       nullptr, 0 },
    { "repeat",              required_argument, nullptr, 0 },
    { "file_prefix",         required_argument, nullptr, 0 },
    { "increment",           no_argument,       nullptr, 0 },
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
    {GET_TRACE_LEVEL, GetTraceLevel},
    {CONFIG_BOOT_TRACE, HandleBootTraceConfig}
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
    {"get_level",  SetRunningState},
    {"total_size", HandleOptTotalsize},
    {"boot_trace", HandleOptBootTrace},
    {"repeat", HandleOptRepeat},
    {"file_prefix", HandleOptBootFilePrefix},
    {"increment", HandleOptBootIncrement}
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
    {"total_size", STATE_NULL},
    {"trace_level", SET_TRACE_LEVEL},
    {"get_level",  GET_TRACE_LEVEL},
    {"boot_trace", CONFIG_BOOT_TRACE},
    {"repeat", CONFIG_BOOT_TRACE},
    {"file_prefix", CONFIG_BOOT_TRACE},
    {"increment", CONFIG_BOOT_TRACE}
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
const int MAX_FILE_SIZE_MULTIPLIER = 10;
static const std::string TRACE_WRITABLE_PATH = "/data/local/tmp";

constexpr int BOOT_TRACE_DEFAULT_DURATION = 30; // 30 seconds
constexpr int BOOT_TRACE_REPEAT_MIN = 1;
constexpr int BOOT_TRACE_REPEAT_MAX = 100;
constexpr int BOOT_TRACE_EXIT_OK = 0;
constexpr int BOOT_TRACE_EXIT_DUPLICATE = 1;
constexpr int BOOT_TRACE_EXIT_CONFIG_ERROR = 2;
/** persist.hitrace.boot_trace.count: 0 = off; 1~100 = remaining boot trace count. */
constexpr const char* BOOT_TRACE_DEFAULT_PREFIX = "boot_trace";
constexpr int MIN_ARGS_FOR_BOOT_TRACE_SUBCOMMAND = 2;
std::string g_traceRootPath;
std::shared_ptr<OHOS::HiviewDFX::UCollectClient::TraceCollector> g_traceCollector;
TraceArgs g_traceArgs;
TraceSysEventParams g_traceSysEventParams;
bool g_needSysEvent = false;
RunningState g_runningState = STATE_NULL;

/* When boot_trace long opt is rejected for non-root euid, avoid duplicate "parsing args failed" line. */
bool g_suppressParsingArgsFailedLog = false;
}

/** Same log as InitAndCheckArgs when HandleOpt fails; also used when boot-trace is denied (e.g. image). */
static constexpr const char K_PARSING_ARGS_FAILED_LOG[] = "error: parsing args failed, exit.";
static constexpr const char K_BOOT_TRACE_UNRECOGNIZED_SUBCMD[] = "error: unrecognized command 'boot-trace'.";
static constexpr const char K_BOOT_TRACE_UNRECOGNIZED_LONGOPT[] = "error: unrecognized option '--boot_trace'.";
static constexpr const char K_BOOT_TRACE_INLINE_EVENT_FMT_ENV[] = "HITRACE_BOOT_INLINE_EVENT_FMT";
#ifdef HITRACE_UNITTEST
static bool g_bootTraceForceRootForTest = true;
#endif

#ifdef HITRACE_UNITTEST
void SetBootTraceForceRootForTest(bool force)
{
    g_bootTraceForceRootForTest = force;
}
#endif

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
    g_bootTraceForceRootForTest = true;
    g_suppressParsingArgsFailedLog = false;
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

static bool IsBootTraceEuidRoot()
{
#ifdef HITRACE_UNITTEST
    /* force=false: simulate non-root for negative-path tests (e.g. HitraceCMDTest045). */
    if (!g_bootTraceForceRootForTest) {
        return false;
    }
#endif
    /* boot_trace / boot-trace: require root euid (init child runs as root). */
    return geteuid() == 0;
}

/*
 * Boot trace reads/writes cfg under /data/local/tmp; only allow on debuggable images (const.debuggable),
 * so user builds cannot capture trace from a manually planted cfg.
 */
static bool IsBootTraceAllowedByConstDebuggable()
{
#ifdef HITRACE_UNITTEST
    return true;
#else
    return IsRootVersion();
#endif
}

static int DenyBootTraceAsUnparsedArgs()
{
    ConsoleLog(K_PARSING_ARGS_FAILED_LOG);
    return -1;
}

static int DenyBootTraceNonRootEuidSubcommand()
{
    ConsoleLog(K_BOOT_TRACE_UNRECOGNIZED_SUBCMD);
    return -1;
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
           "  --total_size           Sets the total size of all traces (KB). The default total size is 2048*1024 KB.\n"
           "                         Only effective in raw trace mode.\n"
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

static bool HandleOptBootTrace(const RunningState& setValue)
{
    if (!IsBootTraceEuidRoot()) {
        g_suppressParsingArgsFailedLog = true;
        ConsoleLog(K_BOOT_TRACE_UNRECOGNIZED_LONGOPT);
        return false;
    }
    return SetRunningState(setValue);
}

static bool HandleOptRepeat(const RunningState& setValue)
{
    if (optarg == nullptr) {
        return false;
    }
    int val = 0;
    if (!StrToNum(optarg, val)) {
        ConsoleLog("error: repeat is illegal input. eg: \"--repeat 5\".");
        return false;
    }
    if (val < BOOT_TRACE_REPEAT_MIN || val > BOOT_TRACE_REPEAT_MAX) {
        ConsoleLog("error: --repeat must be from 1 to 100. eg: \"--repeat 5\".");
        return false;
    }
    g_traceArgs.remainingCount = val;
    return true;
}

static bool HandleOptBootIncrement(const RunningState& setValue)
{
    (void)setValue;
    if (g_runningState != CONFIG_BOOT_TRACE) {
        ConsoleLog("error: --increment only supports --boot_trace.");
        return false;
    }
    g_traceArgs.bootTraceIncrement = true;
    return true;
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

static bool EnsureDirExists(const std::string& dirPath)
{
    struct stat st {};
    if (stat(dirPath.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true;
        }
        ConsoleLog("error: " + dirPath + " is not a directory.");
        return false;
    }
    if (mkdir(dirPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) {
        ConsoleLog("error: cannot create directory " + dirPath + ", errno: " + std::to_string(errno));
        return false;
    }
    return true;
}

static bool IsBootTraceActiveFlagOn()
{
    // TRACE_BOOT_ACTIVE_FLAG is a non-persist flag indicating boot-trace capturing window.
    return GetPropertyInner(TRACE_BOOT_ACTIVE_FLAG, "0") == "1";
}

static void ClearBootTraceActiveFlagIfNeeded(bool isActive)
{
    if (!isActive) {
        return;
    }
    if (!SetProperty(TRACE_BOOT_ACTIVE_FLAG, "0")) {
        ConsoleLog("warning: failed to clear " + std::string(TRACE_BOOT_ACTIVE_FLAG) + " after boot-trace.");
    }
}

static std::string BuildKernelSectionJson(const std::vector<std::string>& kernelTags, int bufferSizeKb,
    const std::string& clockType)
{
    std::ostringstream oss;
    oss << "  \"kernel\": {\n";
    oss << "    \"enabled\": " << (kernelTags.empty() ? "false" : "true") << ",\n";
    oss << "    \"tags\": [";
    for (size_t i = 0; i < kernelTags.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << "\"" << kernelTags[i] << "\"";
    }
    oss << "],\n";
    oss << "    \"buffer_size_kb\": " << bufferSizeKb << ",\n";
    oss << "    \"clock\": \"" << clockType << "\"\n";
    oss << "  },\n";
    return oss.str();
}

static std::string BuildUserSpaceSectionJson(const std::vector<std::string>& userTags)
{
    std::ostringstream oss;
    oss << "  \"userspace\": {\n";
    oss << "    \"enabled\": " << (userTags.empty() ? "false" : "true") << ",\n";
    oss << "    \"tags\": [";
    for (size_t i = 0; i < userTags.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << "\"" << userTags[i] << "\"";
    }
    oss << "]\n";
    oss << "  }\n";
    return oss.str();
}

static std::string JoinBootTraceTagsForDisplay(const std::vector<std::string>& tags)
{
    std::ostringstream oss;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) {
            oss << ' ';
        }
        oss << tags[i];
    }
    return oss.str();
}

static void PrintBootTraceConfiguredArgs()
{
    if (!g_traceArgs.tagsVec.empty()) {
        ConsoleLog("tags: " + JoinBootTraceTagsForDisplay(g_traceArgs.tagsVec));
    }
    if (g_traceArgs.bufferSize > 0) {
        ConsoleLog("buffer_size: " + std::to_string(g_traceArgs.bufferSize));
    }
    if (g_traceArgs.duration > 0) {
        ConsoleLog("time: " + std::to_string(g_traceArgs.duration));
    }
    if (g_traceArgs.fileSize > 0) {
        ConsoleLog("file_size: " + std::to_string(g_traceArgs.fileSize));
    }
    if (g_traceArgs.remainingCount != 1) {
        ConsoleLog("repeat: " + std::to_string(g_traceArgs.remainingCount));
    }
    if (!g_traceArgs.bootFilePrefix.empty()) {
        ConsoleLog("file_prefix: " + g_traceArgs.bootFilePrefix);
    }
    if (g_traceArgs.bootTraceOverwrite) {
        ConsoleLog("overwrite: true");
    }
    if (g_traceArgs.bootTraceIncrement) {
        ConsoleLog("increment: true");
    }
}

static std::string BuildBootTraceOutputPathForPrefix(const std::string& filePrefix, int incrementIndex)
{
    const std::string stem = std::string(BOOT_TRACE_CONFIG_DIR) + filePrefix + "_default";
    if (incrementIndex >= 0) {
        return stem + "_" + std::to_string(incrementIndex) + ".sys";
    }
    return stem + ".sys";
}

static std::string BuildBootTraceConfigJson(const BootTraceConfig& config)
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"version\": 1,\n";
    oss << "  \"description\": \"Boot-time trace configuration\",\n";
    oss << "  \"duration_sec\": " << config.durationSec << ",\n";
    oss << "  \"output\": \"" << BuildBootTraceOutputPathForPrefix(config.filePrefix, config.incrementIndex) << "\",\n";
    oss << "  \"file_size_kb\": " << config.fileSizeKb << ",\n";
    oss << "  \"file_prefix\": \"" << config.filePrefix << "\",\n";
    oss << "  \"overwrite\": " << (config.overwrite ? "true" : "false") << ",\n";
    oss << "  \"inline_event_format\": true,\n";
    oss << "  \"increment_index\": " << config.incrementIndex << ",\n";

    oss << BuildKernelSectionJson(config.kernelTags, config.bufferSizeKb, config.clockType);
    oss << BuildUserSpaceSectionJson(config.userTags);
    oss << "}\n";
    return oss.str();
}

static bool ValidateBootTraceConfig(const std::string& configPath, cJSON* root)
{
    if (!cJSON_IsObject(root)) {
        ConsoleLog("error: boot_trace config is not a JSON object in " + configPath);
        return false;
    }
    cJSON* durationNode = cJSON_GetObjectItem(root, "duration_sec");
    if (!cJSON_IsNumber(durationNode) || durationNode->valueint <= 0) {
        ConsoleLog("error: duration_sec is missing or invalid in " + configPath);
        return false;
    }
    return true;
}

struct BootTraceCaptureConfig {
    int durationSec = 0;
    std::string outputPath;
    std::vector<std::string> tags;
    uint32_t bufferSizeKb = 0;
    std::string clockType = "boot";
    bool overwrite = true;
    uint32_t fileSizeLimitKb = 0;
    int incrementIndex = -1;
    bool inlineEventFormat = false;
};

static void AppendBootTraceSectionTags(cJSON* root, const char* section, std::vector<std::string>& out)
{
    cJSON* sec = cJSON_GetObjectItem(root, section);
    if (!cJSON_IsObject(sec)) {
        return;
    }
    cJSON* en = cJSON_GetObjectItem(sec, "enabled");
    if (cJSON_IsBool(en) && !cJSON_IsTrue(en)) {
        return;
    }
    cJSON* arr = cJSON_GetObjectItem(sec, "tags");
    if (!cJSON_IsArray(arr)) {
        return;
    }
    for (cJSON* t = arr->child; t != nullptr; t = t->next) {
        if (!cJSON_IsString(t) || t->valuestring == nullptr || t->valuestring[0] == '\0') {
            continue;
        }
        out.emplace_back(t->valuestring);
    }
}

static std::string ResolveBootTraceFilePrefixFromJson(cJSON* root)
{
    std::string prefix = BOOT_TRACE_DEFAULT_PREFIX;
    cJSON* prefixNode = cJSON_GetObjectItem(root, "file_prefix");
    if (cJSON_IsString(prefixNode) && prefixNode->valuestring != nullptr && prefixNode->valuestring[0] != '\0') {
        prefix = prefixNode->valuestring;
    }
    return prefix;
}

static int ResolveBootTraceIncrementIndexFromJson(cJSON* root)
{
    cJSON* incNode = cJSON_GetObjectItem(root, "increment_index");
    if (cJSON_IsNumber(incNode)) {
        return incNode->valueint;
    }
    return -1;
}

static std::string ResolveBootTraceCaptureOutputPath(cJSON* root, const std::string& prefix, int incrementIndex)
{
    if (incrementIndex >= 0) {
        return BuildBootTraceOutputPathForPrefix(prefix, incrementIndex);
    }
    cJSON* outNode = cJSON_GetObjectItem(root, "output");
    if (cJSON_IsString(outNode) && outNode->valuestring != nullptr && outNode->valuestring[0] != '\0') {
        return outNode->valuestring;
    }
    return BuildBootTraceOutputPathForPrefix(prefix, -1);
}

static bool ValidateBootTraceCaptureTags(const std::string& configPath, cJSON* root, BootTraceCaptureConfig& cfg)
{
    cfg.tags.clear();
    AppendBootTraceSectionTags(root, "kernel", cfg.tags);
    AppendBootTraceSectionTags(root, "userspace", cfg.tags);
    if (cfg.tags.empty()) {
        ConsoleLog("error: boot_trace config has no tags under kernel/userspace in " + configPath);
        return false;
    }
    const auto& allTagInfos = TraceJsonParser::Instance().GetAllTagInfos();
    for (const auto& tag : cfg.tags) {
        if (allTagInfos.find(tag) == allTagInfos.end()) {
            ConsoleLog("error: boot_trace unsupported tag \"" + tag + "\" in " + configPath);
            return false;
        }
    }
    return true;
}

static void ApplyBootTraceKernelCaptureOptions(cJSON* root, BootTraceCaptureConfig& cfg)
{
    cJSON* kernel = cJSON_GetObjectItem(root, "kernel");
    if (!cJSON_IsObject(kernel)) {
        return;
    }
    cJSON* buf = cJSON_GetObjectItem(kernel, "buffer_size_kb");
    if (cJSON_IsNumber(buf) && buf->valueint > 0) {
        cfg.bufferSizeKb = static_cast<uint32_t>(buf->valueint);
    }
    cJSON* clk = cJSON_GetObjectItem(kernel, "clock");
    if (cJSON_IsString(clk) && clk->valuestring != nullptr && clk->valuestring[0] != '\0') {
        cfg.clockType = clk->valuestring;
    }
}

static void ApplyBootTraceFileCaptureOptions(cJSON* root, BootTraceCaptureConfig& cfg)
{
    cJSON* ow = cJSON_GetObjectItem(root, "overwrite");
    if (cJSON_IsBool(ow)) {
        cfg.overwrite = cJSON_IsTrue(ow);
    }
    cJSON* fs = cJSON_GetObjectItem(root, "file_size_kb");
    if (cJSON_IsNumber(fs) && fs->valueint > 0) {
        cfg.fileSizeLimitKb = static_cast<uint32_t>(fs->valueint);
    }
    cJSON* inlineFmt = cJSON_GetObjectItem(root, "inline_event_format");
    if (cJSON_IsBool(inlineFmt)) {
        cfg.inlineEventFormat = cJSON_IsTrue(inlineFmt);
    }
}

static bool FillBootTraceCaptureConfig(const std::string& configPath, cJSON* root, BootTraceCaptureConfig& cfg)
{
    cJSON* durationNode = cJSON_GetObjectItem(root, "duration_sec");
    cfg.durationSec = durationNode->valueint;

    std::string prefix = ResolveBootTraceFilePrefixFromJson(root);
    cfg.incrementIndex = ResolveBootTraceIncrementIndexFromJson(root);
    cfg.outputPath = ResolveBootTraceCaptureOutputPath(root, prefix, cfg.incrementIndex);

    if (!ValidateBootTraceCaptureTags(configPath, root, cfg)) {
        return false;
    }
    ApplyBootTraceKernelCaptureOptions(root, cfg);
    ApplyBootTraceFileCaptureOptions(root, cfg);
    return true;
}

static int ExecuteBootTraceCapture(const BootTraceCaptureConfig& cfg)
{
    if (cfg.inlineEventFormat) {
        setenv(K_BOOT_TRACE_INLINE_EVENT_FMT_ENV, "1", 1);
    } else {
        unsetenv(K_BOOT_TRACE_INLINE_EVENT_FMT_ENV);
    }
    using DumpTraceArgs = ::OHOS::HiviewDFX::Hitrace::TraceArgs;
    DumpTraceArgs args;
    args.tags = cfg.tags;
    args.bufferSize = cfg.bufferSizeKb;
    args.clockType = cfg.clockType;
    args.isOverWrite = cfg.overwrite;
    args.fileSizeLimit = cfg.fileSizeLimitKb;
    TraceErrorCode openRet = OpenTrace(args);
    if (openRet != TraceErrorCode::SUCCESS) {
        ConsoleLog("error: boot_trace OpenTrace failed, errorCode(" + std::to_string(static_cast<int>(openRet)) + ")");
        return -1;
    }

    ConsoleLog("boot_trace: capturing, duration_sec=" + std::to_string(cfg.durationSec) + ", output=" + cfg.outputPath);
    if (cfg.durationSec > 0) {
        sleep(static_cast<unsigned int>(cfg.durationSec));
    }

    uint32_t dumpWindow = 0;
    if (cfg.durationSec > 0) {
        dumpWindow = static_cast<uint32_t>(cfg.durationSec);
    }
    TraceRetInfo dumpRet = DumpTrace(dumpWindow, 0, cfg.outputPath);
    if (dumpRet.errorCode != TraceErrorCode::SUCCESS) {
        ConsoleLog("error: boot_trace DumpTrace failed, errorCode(" +
            std::to_string(static_cast<int>(dumpRet.errorCode)) + ")");
        (void)CloseTrace();
        return -1;
    }
    for (const auto& f : dumpRet.outputFiles) {
        ConsoleLog("boot_trace: wrote " + f);
    }

    TraceErrorCode closeRet = CloseTrace();
    if (closeRet != TraceErrorCode::SUCCESS) {
        ConsoleLog("warning: boot_trace CloseTrace errorCode(" + std::to_string(static_cast<int>(closeRet)) + ")");
        return -1;
    }
    return 0;
}

static bool LoadBootTraceCaptureConfig(const std::string& configPath, BootTraceCaptureConfig& cfg)
{
    struct stat st {};
    if (stat(configPath.c_str(), &st) != 0) {
        ConsoleLog("boot trace config not found: " + configPath);
        return false;
    }
    std::ifstream in(configPath, std::ios::in);
    if (!in.is_open()) {
        ConsoleLog("error: open " + configPath + " failed.");
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    if (content.empty()) {
        ConsoleLog("error: " + configPath + " is empty.");
        return false;
    }

    std::vector<char> contentBuffer(content.begin(), content.end());
    contentBuffer.push_back('\0');
    cJSON* root = cJSON_Parse(contentBuffer.data());
    if (root == nullptr) {
        ConsoleLog("error: parse " + configPath + " failed.");
        return false;
    }
    if (!ValidateBootTraceConfig(configPath, root)) {
        cJSON_Delete(root);
        return false;
    }
    if (!FillBootTraceCaptureConfig(configPath, root, cfg)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(root);
    return true;
}

static cJSON* LoadBootTraceConfigJsonForUpdate(const std::string& configPath)
{
    std::ifstream in(configPath, std::ios::in);
    if (!in.is_open()) {
        ConsoleLog("warning: open " + configPath + " failed, can not record result.");
        return nullptr;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    if (content.empty()) {
        ConsoleLog("warning: " + configPath + " is empty, can not record result.");
        return nullptr;
    }

    std::vector<char> contentBuffer(content.begin(), content.end());
    contentBuffer.push_back('\0');
    cJSON* root = cJSON_Parse(contentBuffer.data());
    if (!cJSON_IsObject(root)) {
        ConsoleLog("warning: parse " + configPath + " failed, can not record result.");
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        return nullptr;
    }
    return root;
}

static bool WriteBootTraceConfigJson(const std::string& configPath, cJSON* root)
{
    char* rendered = cJSON_Print(root);
    if (rendered == nullptr) {
        ConsoleLog("warning: serialize " + configPath + " failed, can not record result.");
        return false;
    }
    std::ofstream out(configPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        ConsoleLog("warning: open " + configPath + " for write failed, can not record result.");
        cJSON_free(rendered);
        return false;
    }
    out << rendered;
    cJSON_free(rendered);
    if (out.fail()) {
        out.close();
        ConsoleLog("warning: write result to " + configPath + " failed.");
        return false;
    }
    out.close();
    return true;
}

static void ApplyBootTraceIncrementBump(cJSON* root)
{
    cJSON* incNode = cJSON_GetObjectItem(root, "increment_index");
    if (!cJSON_IsNumber(incNode) || incNode->valueint < 0) {
        return;
    }
    const int newIdx = incNode->valueint + 1;
    cJSON_DeleteItemFromObject(root, "increment_index");
    (void)cJSON_AddItemToObject(root, "increment_index", cJSON_CreateNumber(newIdx));

    std::string prefix = BOOT_TRACE_DEFAULT_PREFIX;
    cJSON* fp = cJSON_GetObjectItem(root, "file_prefix");
    if (cJSON_IsString(fp) && fp->valuestring != nullptr && fp->valuestring[0] != '\0') {
        prefix = fp->valuestring;
    }
    const std::string newOut = BuildBootTraceOutputPathForPrefix(prefix, newIdx);
    cJSON_DeleteItemFromObject(root, "output");
    (void)cJSON_AddItemToObject(root, "output", cJSON_CreateString(newOut.c_str()));
}

static bool SaveBootTraceResultToConfig(const std::string& configPath, int resultCode)
{
    cJSON* root = LoadBootTraceConfigJsonForUpdate(configPath);
    if (root == nullptr) {
        return false;
    }
    cJSON_DeleteItemFromObject(root, "result");
    cJSON* resultNode = cJSON_CreateNumber(resultCode);
    if (resultNode == nullptr || !cJSON_AddItemToObject(root, "result", resultNode)) {
        if (resultNode != nullptr) {
            cJSON_Delete(resultNode);
        }
        cJSON_Delete(root);
        ConsoleLog("warning: update result field failed in " + configPath);
        return false;
    }
    if (resultCode == BOOT_TRACE_EXIT_OK) {
        ApplyBootTraceIncrementBump(root);
    }
    bool isSuccess = WriteBootTraceConfigJson(configPath, root);
    cJSON_Delete(root);
    return isSuccess;
}

static int RunBootTraceControl()
{
    const std::string configPath = std::string(BOOT_TRACE_CONFIG_DIR) + BOOT_TRACE_CONFIG_FILE;
    /*
     * debug.hitrace.boot_trace.active is owned by this process: set to 1 when entering capture,
     * clear when done. If already 1, another boot-trace instance holds the window — exit without work.
     * Init no longer sets active before fork+execl; hitrace sets it here so manual launch also works.
     */
    if (IsBootTraceActiveFlagOn()) {
        ConsoleLog("boot_trace: duplicate launch ignored (debug.hitrace.boot_trace.active already 1)");
        (void)SaveBootTraceResultToConfig(configPath, BOOT_TRACE_EXIT_DUPLICATE);
        return BOOT_TRACE_EXIT_DUPLICATE;
    }

    if (!SetProperty(TRACE_BOOT_ACTIVE_FLAG, "1")) {
        ConsoleLog("error: failed to set " + std::string(TRACE_BOOT_ACTIVE_FLAG) + " to 1.");
        (void)SaveBootTraceResultToConfig(configPath, BOOT_TRACE_EXIT_CONFIG_ERROR);
        return BOOT_TRACE_EXIT_CONFIG_ERROR;
    }

    struct BootTraceSessionGuard {
        bool on;
        explicit BootTraceSessionGuard(bool active) : on(active) {}
        ~BootTraceSessionGuard()
        {
            ClearBootTraceActiveFlagIfNeeded(on);
        }
    } sessionGuard(true);

    int resultCode = BOOT_TRACE_EXIT_CONFIG_ERROR;
    BootTraceCaptureConfig cfg;
    if (!LoadBootTraceCaptureConfig(configPath, cfg)) {
        (void)SaveBootTraceResultToConfig(configPath, resultCode);
        return resultCode;
    }
    ConsoleLog("boot_trace: active set; running capture.");
    if (ExecuteBootTraceCapture(cfg) == 0) {
        resultCode = BOOT_TRACE_EXIT_OK;
        ConsoleLog("boot_trace finished.");
    } else {
        ConsoleLog("error: boot_trace capture failed.");
    }
    (void)SaveBootTraceResultToConfig(configPath, resultCode);
    return resultCode;
}

static bool HandleBootTraceOff()
{
    if (!SetProperty(BOOT_TRACE_COUNT_PARAM, "0")) {
        ConsoleLog("error: failed to set " + std::string(BOOT_TRACE_COUNT_PARAM) + " to 0.");
        return false;
    }
    /* Spec: off only clears repeat counter; do not unlink under /data/local/tmp (avoid broad delete MAC). */
    ConsoleLog("boot_trace off success.");
    return true;
}

static bool ValidateBootTraceTags(std::vector<std::string>& kernelTags,
    std::vector<std::string>& userTags)
{
    if (g_traceArgs.tagsVec.empty()) {
        ConsoleLog("error: boot_trace requires at least one tag.");
        return false;
    }

    int remainingCount = g_traceArgs.remainingCount;
    if (remainingCount < BOOT_TRACE_REPEAT_MIN || remainingCount > BOOT_TRACE_REPEAT_MAX) {
        ConsoleLog("error: --repeat must be from 1 to 100. eg: \"--repeat 5\".");
        return false;
    }

    const auto& allTagInfos = TraceJsonParser::Instance().GetAllTagInfos();
    for (const auto& tag : g_traceArgs.tagsVec) {
        auto it = allTagInfos.find(tag);
        if (it == allTagInfos.end()) {
            std::string errorInfo = "error: " + tag + " is not support category on this device.";
            ConsoleLog(errorInfo);
            return false;
        }
        if (it->second.type == TraceType::KERNEL) {
            kernelTags.emplace_back(tag);
        } else {
            userTags.emplace_back(tag);
        }
    }
    return true;
}

static bool BuildAndWriteBootTraceConfig(const std::vector<std::string>& kernelTags,
    const std::vector<std::string>& userTags)
{
    int duration = (g_traceArgs.duration > 0) ? g_traceArgs.duration : BOOT_TRACE_DEFAULT_DURATION;
    int bufferSizeKb = (g_traceArgs.bufferSize > 0) ? g_traceArgs.bufferSize :
        static_cast<int>(DEFAULT_BUFFER_SIZE);
    int fileSizeKb = (g_traceArgs.fileSize >= MIN_FILE_SIZE && g_traceArgs.fileSize <= MAX_FILE_SIZE) ?
        g_traceArgs.fileSize : DEFAULT_FILE_SIZE;
    std::string clockType = g_traceArgs.clockType.empty() ? "boot" : g_traceArgs.clockType;
    std::string filePrefix = g_traceArgs.bootFilePrefix.empty() ?
        BOOT_TRACE_DEFAULT_PREFIX : g_traceArgs.bootFilePrefix;

    int remainingCount = g_traceArgs.remainingCount;
    std::string configDir = BOOT_TRACE_CONFIG_DIR;
    if (!EnsureDirExists(configDir)) {
        return false;
    }

    std::string configPath = configDir + std::string(BOOT_TRACE_CONFIG_FILE);
    std::ofstream out(configPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        ConsoleLog("error: open " + configPath + " failed.");
        return false;
    }

    BootTraceConfig config;
    config.durationSec = duration;
    config.bufferSizeKb = bufferSizeKb;
    config.fileSizeKb = fileSizeKb;
    config.kernelTags = kernelTags;
    config.userTags = userTags;
    config.clockType = clockType;
    config.filePrefix = filePrefix;
    config.overwrite = g_traceArgs.bootTraceOverwrite;
    config.incrementIndex = g_traceArgs.bootTraceIncrement ? 0 : -1;

    std::string configJson = BuildBootTraceConfigJson(config);
    out << configJson;
    if (out.fail()) {
        ConsoleLog("error: can not write " + configPath);
        out.close();
        return false;
    }
    out.close();

    if (!SetProperty(BOOT_TRACE_COUNT_PARAM, std::to_string(remainingCount))) {
        ConsoleLog("error: failed to set " + std::string(BOOT_TRACE_COUNT_PARAM));
        return false;
    }

    PrintBootTraceConfiguredArgs();
    ConsoleLog("boot_trace configuration success.");
    return true;
}

static bool HandleBootTraceConfig()
{
    if (!IsBootTraceAllowedByConstDebuggable()) {
        ConsoleLog(K_PARSING_ARGS_FAILED_LOG);
        return false;
    }
    // handle "hitrace --boot_trace off"
    if (g_traceArgs.tagsVec.size() == 1 && g_traceArgs.tagsVec[0] == "off") {
        return HandleBootTraceOff();
    }

    std::vector<std::string> kernelTags;
    std::vector<std::string> userTags;
    if (!ValidateBootTraceTags(kernelTags, userTags)) {
        return false;
    }
    return BuildAndWriteBootTraceConfig(kernelTags, userTags);
}

static bool CheckOutputFile(const char* path)
{
    struct stat buf;
    size_t len = strnlen(path, MAX_OUTPUT_LEN);
    if (len == MAX_OUTPUT_LEN || len < 1 ||
        (stat(path, &buf) == 0 && (buf.st_mode & S_IFDIR) && g_runningState != RECORDING_LONG_BEGIN_RECORD &&
            g_runningState != SNAPSHOT_DUMP)) {
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
    if (g_runningState == CONFIG_BOOT_TRACE) {
        g_traceArgs.bootTraceOverwrite = true;  // --overwrite: each boot trace overwrites previous
    } else {
        g_traceArgs.overwrite = false;  // recording mode: buffer not overwrite when full
    }
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

static bool HandleOptTotalsize(const RunningState& setValue)
{
    if (optarg == nullptr) {
        return false;
    }
    bool isTrue = true;
    int totalSizeKB = 0;
    if (!StrToNum(optarg, totalSizeKB)) {
        ConsoleLog("error: total size is illegal input. eg: \"--total_size 1024000\".");
        isTrue = false;
    } else if (totalSizeKB < MAX_FILE_SIZE || totalSizeKB > MAX_FILE_SIZE_MULTIPLIER * MAX_FILE_SIZE) {
        ConsoleLog("error: total size must be from 500 MB to 5000 MB. eg: \"--total_size 1024000\".");
        isTrue = false;
    }
    g_traceArgs.totalSize = totalSizeKB;
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

static bool HandleOptBootFilePrefix(const RunningState& setValue)
{
    if (setValue != CONFIG_BOOT_TRACE) {
        ConsoleLog("error: --file_prefix only supports --boot_trace.");
        return false;
    }
    if (optarg == nullptr || strlen(optarg) == 0) {
        ConsoleLog("error: file_prefix must not be empty.");
        return false;
    }
    g_traceArgs.bootFilePrefix = optarg;
    return true;
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
    // special case: "hitrace --boot_trace off"
    if (g_runningState == CONFIG_BOOT_TRACE && argc - optind == 1 &&
        std::string(argv[optind]) == "off") {
        g_traceArgs.tagsVec.clear();
        g_traceArgs.tags.clear();
        g_traceArgs.tagsVec.emplace_back("off");
        return true;
    }

    auto traceTags = TraceJsonParser::Instance().GetAllTagInfos();
    const auto &tagGroups = TraceJsonParser::Instance().GetTagGroups();
    for (int i = optind; i < argc; i++) {
        std::string tag = std::string(argv[i]);
        if (g_runningState == CONFIG_BOOT_TRACE && tagGroups.find(tag) != tagGroups.end()) {
            ConsoleLog("error: tag group is not supported in boot_trace. please use concrete tags.");
            return false;
        }
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

static void DumpKernelTraceToOutput()
{
    std::string tracePath = g_traceRootPath + TRACE_NODE;
    std::string traceSpecPath = CanonicalizeSpecPath(tracePath.c_str());
    auto traceFd = OHOS::HiviewDFX::SmartFd(open(traceSpecPath.c_str(), O_RDONLY));
    if (!traceFd) {
        ConsoleLog("error: opening " + tracePath + ", errno: " + std::to_string(errno));
        g_traceSysEventParams.errorCode = OPEN_ROOT_PATH_FAILURE;
        g_traceSysEventParams.errorMessage = "error: opening " + tracePath + ", errno: " +
            std::to_string(errno);
        return;
    }
    OHOS::HiviewDFX::SmartFd outFileFd;
    if (g_traceArgs.output.size() > 0) {
        std::string outSpecPath = CanonicalizeSpecPath(g_traceArgs.output.c_str());
        outFileFd = OHOS::HiviewDFX::SmartFd(
            open(outSpecPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
        if (!outFileFd) {
            ConsoleLog("error: opening " + g_traceArgs.output + ", errno: " + std::to_string(errno));
            g_traceSysEventParams.errorCode = OPEN_FILE_PATH_FAILURE;
            g_traceSysEventParams.errorMessage = "error: opening " + g_traceArgs.output + ", errno: " +
                std::to_string(errno);
            return;
        }
    }
    int outFd = outFileFd ? outFileFd.GetFd() : STDOUT_FILENO;
    ssize_t bytesWritten;
    ssize_t bytesRead;
    if (g_traceArgs.isCompress) {
        DumpCompressedTrace(traceFd.GetFd(), outFd);
    } else {
        const int blockSize = 4096;
        char buffer[blockSize];
        do {
            bytesRead = TEMP_FAILURE_RETRY(read(traceFd.GetFd(), buffer, blockSize));
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
    }
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
    if (g_traceArgs.totalSize > 0) {
        if (g_runningState == RECORDING_LONG_BEGIN_RECORD) {
            hiviewTraceParam.totalSize = static_cast<uint64_t>(g_traceArgs.totalSize);
            args += (" totalSize:" + std::to_string(g_traceArgs.totalSize));
        } else {
            ConsoleLog("warning: The current state does not support specifying the total size, total size: " +
                std::to_string(g_traceArgs.totalSize) + " is invalid.");
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
    DumpKernelTraceToOutput();

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
        .fileSizeLimit = 0,
        .totalSize = 0
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
    DumpKernelTraceToOutput();
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
    DumpKernelTraceToOutput();
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
        .fileSizeLimit = 0,
        .totalSize = 0
    };
    ReloadTraceArgs(tags, hiviewTraceParam);
    if (g_traceArgs.output.size() > 0 && !IsWritableDir(g_traceArgs.output)) {
        ConsoleLog("error: illegal path");
        return false;
    }
    auto openRet = g_traceCollector->OpenTrace(tags, hiviewTraceParam, {});
    if (openRet.retCode != OHOS::HiviewDFX::UCollect::UcError::SUCCESS) {
        ConsoleLog("error: OpenRecording failed, errorCode(" + std::to_string(openRet.retCode) +")");
        return false;
    }
    auto recOnRet = g_traceCollector->RecordingOn(g_traceArgs.output);
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
    if (g_traceArgs.output.size() > 0 && !IsWritable(g_traceArgs.output)) {
        ConsoleLog("error: illegal path");
        return false;
    }
    auto dumpRet = g_traceCollector->DumpSnapshot(OHOS::HiviewDFX::UCollect::TraceClient::COMMAND, g_traceArgs.output);
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

    g_suppressParsingArgsFailedLog = false;
    if (!HandleOpt(argc, argv)) {
        if (!g_suppressParsingArgsFailedLog) {
            ConsoleLog(K_PARSING_ARGS_FAILED_LOG);
        }
        g_suppressParsingArgsFailedLog = false;
        return false;
    }

    if (g_traceArgs.totalSize != 0 && !IsWritableDir(g_traceArgs.output)) {
        ConsoleLog("error: can't set totalsize when output path is't /data/local/tmp, exit.");
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
    if (argc >= MIN_ARGS_FOR_BOOT_TRACE_SUBCOMMAND && strcmp(argv[1], "boot-trace") == 0) {
        if (!IsBootTraceEuidRoot()) {
            return DenyBootTraceNonRootEuidSubcommand();
        }
        if (!IsBootTraceAllowedByConstDebuggable()) {
            return DenyBootTraceAsUnparsedArgs();
        }
        return RunBootTraceControl();
    }

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