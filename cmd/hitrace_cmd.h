#ifndef HITRACE_CMD_DEFINE_H
#define HITRACE_CMD_DEFINE_H

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
#endif