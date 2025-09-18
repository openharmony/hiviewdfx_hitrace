#ifndef HITRACE_CMD_DEFINE_H
#define HITRACE_CMD_DEFINE_H

struct TraceArgs {
    std::string tags;
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

enum CmdErrorCode {
    OPEN_ROOT_PATH_FAILURE = 2001,
    OPEN_FILE_PATH_FAILURE = 2002,
    TRACING_ON_CLOSED = 2003,
};

using CommandFunc = std::function<bool(const RunningState)>;
using TaskFunc = std::function<bool()>;

bool HandleRecordingShortRaw();
bool HandleRecordingShortText();
bool HandleRecordingLongBegin();
bool HandleRecordingLongDump();
bool HandleRecordingLongFinish();
bool HandleRecordingLongFinishNodump();
bool HandleRecordingLongBeginRecord();
bool HandleRecordingLongFinishRecord();
bool HandleOpenSnapshot();
bool HandleDumpSnapshot();
bool HandleCloseSnapshot();
bool SetTraceLevel();
bool GetTraceLevel();

bool HandleOptBuffersize(const RunningState& setValue);
bool HandleOptTraceclock(const RunningState& setValue);
bool HandleOptTime(const RunningState& setValue);
bool HandleOptOutput(const RunningState& setValue);
bool HandleOptOverwrite(const RunningState& setValue);
bool HandleOptRecord(const RunningState& setValue);
bool HandleOptFilesize(const RunningState& setValue);
bool HandleOptTracelevel(const RunningState& setValue);
bool SetRunningState(const RunningState& setValue);
#endif