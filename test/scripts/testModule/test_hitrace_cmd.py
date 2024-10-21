#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import subprocess
import pytest


def get_shell_result(cmd, words=''):
    print(f"\nexecuting command: {cmd}")
    output = subprocess.check_output(cmd.split()).decode()
    print(f"\noutput: {output}")
    if len(words) > 0:
        assert words in output


class TestHitraceCmd:
    def prepare_steps(self):
        word_cmds = {
            "hdc shell hitrace --stop_bgsrv",
            "hdc shell hitrace --trace_finish_nodump",
            "hdc shell hitrace --trace_finish_nodump --record",
        }
        
        for word_cmd in word_cmds:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_help1(self):
        word_cmds = {
            'hdc shell hitrace -h -b 2048':'running_state is SHOW_HELP',
            'hdc shell hitrace -b 2048 -h':'running_state is SHOW_HELP',
            'hdc shell hitrace -h -b':'error: parsing args failed',
            'hdc shell hitrace -b -h': 'error: buffer size is illegal input.'
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_help2(self):
        word_cmds = {
            'hdc shell hitrace -h --trace_begin': 'cannot coexist.',
            'hdc shell hitrace --trace_begin app -h': 'cannot coexist.'
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_list1(self):
        word_cmds = {
            'hdc shell hitrace -l':'running_state is SHOW_LIST_CATEGORY',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_list2(self):
        word_cmds = {
            'hdc shell hitrace -b 102400 -l':'running_state is SHOW_LIST_CATEGORY',
            'hdc shell hitrace app -l':'running_state is SHOW_LIST_CATEGORY',
            'hdc shell hitrace --trace_begin -l': 'cannot coexist.',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_unsupported_tag(self):
        word_cmds = {
            'hdc shell hitrace --trace_begin appoo': 'error: appoo is not support category on this device.',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_capture_trace(self):
        word_cmds1 = {
            'hdc shell hitrace -b 256 -t 5 sched --trace_begin':'hitrace enter, running_state is RECORDING_LONG_BEGIN',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on':'1',
        }
        
        word_cmds2 = {
            "hdc shell hitrace --trace_finish_nodump",
        }

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))
        
        for word_cmd in word_cmds2:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_double_capture_trace(self):
        word_cmds1 = {
            'hdc shell hitrace -b 256 -t 5 sched --trace_begin': 'hitrace enter, running_state is RECORDING_LONG_BEGIN',
        }

        word_cmds2 = {
            'hdc shell hitrace -b 256 -t 5 sched --trace_begin': 'error: OpenRecording failed, errorCode(1002)',
        }

        word_cmds3 = {
            'hdc shell hitrace --trace_finish_nodump',
        }

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))
        
        for word_cmd in word_cmds2:
            get_shell_result(word_cmd, word_cmds2.get(word_cmd))

        for cmd in word_cmds3:
            get_shell_result(cmd)

    @pytest.mark.L0
    def test_capture_tags_trace(self):
        word_cmds1 = {
            'hdc shell hitrace ability accesscontrol accessibility account ace animation sched --trace_begin':'hitrace enter, running_state is RECORDING_LONG_BEGIN',
        }

        word_cmds2 = {
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on':'1',
        }

        word_cmds3 = {
            'hdc shell hitrace --trace_dump -o /data/local/tmp/trace.ftrace': 'trace read done',
        }

        word_cmds4 = [
            "hdc shell hitrace --trace_finish_nodump",
        ]

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))
        
        for word_cmd in word_cmds2:
            get_shell_result(word_cmd, word_cmds2.get(word_cmd))
        
        for word_cmd in word_cmds3:
            get_shell_result(word_cmd, word_cmds3.get(word_cmd))
        
        for word_cmd in word_cmds4:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_capture_trace_with_overwrite_and_traceclock(self):
        word_cmds1 = {
            'hdc shell hitrace sched --trace_begin --overwrite --trace_clock boot': 'clockType:boot overwrite:0',
        }
        
        word_cmds2 = {
            'hdc shell hitrace --trace_dump -o /data/local/tmp/trace.ftrace': 'trace read done, output: /data/local/tmp/trace.ftrace',
        }
        
        word_cmds3 = {
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
        }

        word_cmds4 = [
            "hdc shell hitrace --trace_finish_nodump",
        ]

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))
        
        for word_cmd in word_cmds2:
            get_shell_result(word_cmd, word_cmds2.get(word_cmd))
        
        for word_cmd in word_cmds3:
            get_shell_result(word_cmd, word_cmds3.get(word_cmd))
        
        for word_cmd in word_cmds4:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_raw_parameter(self):
        word_cmds = {
            'hdc shell hitrace sched --trace_begin --raw': 'RECORDING_LONG_BEGIN and RECORDING_SHORT_RAW cannot coexist',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
            'hdc shell hitrace sched --trace_begin --record --raw': 'RECORDING_LONG_BEGIN_RECORD and RECORDING_SHORT_RAW cannot coexist',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
            'hdc shell hitrace sched -t 10 --raw': 'hitrace enter, running_state is RECORDING_SHORT_RAW',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))
    
    @pytest.mark.L0
    def test_text_parameter(self):
        word_cmds = {
            'hdc shell hitrace sched --trace_begin --text': 'RECORDING_LONG_BEGIN and RECORDING_SHORT_TEXT cannot coexist',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
            'hdc shell hitrace sched --trace_begin --record --text': 'RECORDING_LONG_BEGIN_RECORD and RECORDING_SHORT_TEXT cannot coexist',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
            'hdc shell hitrace sched -t 10 --text': 'start capture, please wait 10s',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_capture_trace_with_filesize_and_filename(self):
        word_cmds1 = {
            'hdc shell hitrace sched --trace_begin --file_size 51200': 'The current state does not support specifying the file size',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
            'hdc shell hitrace sched --trace_dump -o /data/local/tmp/trace.ftrace': 'trace read done, output: /data/local/tmp/trace.ftrace',
        }

        word_cmds2 = [
            "hdc shell hitrace sched --trace_finish_nodump",
        ]

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))
        
        for word_cmd in word_cmds2:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_capture_trace_record_with_filesize_and_filename(self):
        word_cmds1 = {
            'hdc shell hitrace sched --trace_begin --record --file_size 51200': 'tags:sched bufferSize:18432 overwrite:1 fileSize:51200',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
            'hdc shell hitrace --trace_dump --record -o /data/local/tmp/trace.ftrace': 'error: "--record" is set incorrectly. eg: "--trace_begin --record", "--trace_finish --record"',
        }

        word_cmds2 = [
            "hdc shell hitrace --trace_finish_nodump --record",
        ]

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))
        
        for word_cmd in word_cmds2:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_capture_trace_with_buffersize_time_trackclock_and_overwrite(self):
        word_cmds1 = {
            'hdc shell hitrace sched --trace_begin -b 10240 -t 5 --trace_clock global --overwrite': 'tags:sched bufferSize:10240 clockType:global overwrite:0',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
            'hdc shell hitrace --trace_dump -o /data/local/tmp/trace.ftrace': 'trace read done, output: /data/local/tmp/trace.ftrace',
        }

        word_cmds2 = [
            "hdc shell hitrace --trace_finish_nodump",
        ]

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))

        for word_cmd in word_cmds2:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_capture_trace_with_wrong_time(self):
        word_cmds = {
            'hdc shell hitrace sched --trace_begin -b 10240 -t -1 --trace_clock global --overwrite': 'error: "-t -1" to be greater than zero. eg: "--time 5"',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_capture_trace_with_wrong_buffersize(self):
        word_cmds = {
            'hdc shell hitrace sched --trace_begin -b -10240 -t 5 --trace_clock global --overwrite': 'error: buffer size must be from 256 KB to 300 MB. eg: "--buffer_size 18432"',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
        }

        for word_cmd in word_cmds:
            get_shell_result(word_cmd, word_cmds.get(word_cmd))

    @pytest.mark.L0
    def test_capture_trace_with_max_and_min_buffersize(self):
        word_cmds1 = {
            'hdc shell hitrace sched --trace_begin -b 307200': 'hitrace enter, running_state is RECORDING_LONG_BEGIN',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
        }

        word_cmds2 = [
            "hdc shell hitrace --trace_finish_nodump",
        ]

        word_cmds4 = {
            'hdc shell hitrace sched --trace_begin -b 256': 'hitrace enter, running_state is RECORDING_LONG_BEGIN',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
        }

        word_cmds5 = [
            "hdc shell hitrace --trace_finish_nodump",
        ]

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))

        for word_cmd in word_cmds2:
            get_shell_result(word_cmd)

        for word_cmd in word_cmds4:
            get_shell_result(word_cmd, word_cmds4.get(word_cmd))

        for word_cmd in word_cmds5:
            get_shell_result(word_cmd)

    @pytest.mark.L0
    def test_wrong_command(self):
        word_cmds1 = {
            'hdc shell hitrace sched --trace_begin --trace_finish': 'the parameter is set incorrectly',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
        }
        
        word_cmds2 = {
            'hdc shell hitrace sched --trace_begin --trace_dump': 'the parameter is set incorrectly',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
        }

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))

        for word_cmd in word_cmds2:
            get_shell_result(word_cmd, word_cmds2.get(word_cmd))

    @pytest.mark.L0
    def test_start_and_stop_bgsrv(self):
        word_cmds1 = {
            'hdc shell hitrace --start_bgsrv': 'hitrace enter, running_state is SNAPSHOT_START',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
            'hdc shell hitrace --stop_bgsrv': 'hitrace enter, running_state is SNAPSHOT_STOP',
        }

        word_cmds2 = {
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
        }

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))

        for word_cmd in word_cmds2:
            get_shell_result(word_cmd, word_cmds2.get(word_cmd))

    @pytest.mark.L0
    def test_snapshot_with_wrong_command(self):
        word_cmds1 = {
            'hdc shell hitrace --start_bgsrv': 'hitrace enter, running_state is SNAPSHOT_START',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
        }

        word_cmds2 = {
            'hdc shell hitrace --start_bgsrv': 'error: OpenSnapshot failed, errorCode(1006)',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '1',
        }

        word_cmds3 = {
            'hdc shell hitrace --stop_bgsrv': 'hitrace enter, running_state is SNAPSHOT_STOP',
            'hdc shell cat /sys/kernel/debug/tracing/tracing_on': '0',
        }

        for word_cmd in word_cmds1:
            get_shell_result(word_cmd, word_cmds1.get(word_cmd))

        for word_cmd in word_cmds2:
            get_shell_result(word_cmd, word_cmds2.get(word_cmd))
        
        for word_cmd in word_cmds3:
            get_shell_result(word_cmd, word_cmds3.get(word_cmd))


