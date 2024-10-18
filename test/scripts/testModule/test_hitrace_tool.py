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

import os
import pytest
import subprocess
import re
import time

from datetime import datetime
from test_hitrace_cmd import *


def CheckFileExists(filePath):
    if os.path.exists(filePath):
        return filePath


def collect_binary_trace():
    get_shell_result("hdc shell rm -rf /data/log/hitrace/*")

    word_cmds = {
        "hdc shell hitrace --trace_begin --record ace ark app ohos ability graphic sched freq nweb workq pagecache binder irq disk memreclaim samgr sync mmc idle rpc zcamera zmedia zaudio ufs --file_size 153600 -b 102400",
        "hdc shell hitrace --trace_finish --record",
    }

    for word_cmd in word_cmds:
        get_shell_result(word_cmd)
        if word_cmd.find("--trace_begin") != -1:
            time.sleep(10)


def get_binary_trace():
    current_path = os.getcwd()
    trace_file_path = current_path + "\hitrace"

    if not os.path.exists(trace_file_path):
        os.makedirs(trace_file_path)

    get_shell_result("hdc file recv /data/log/hitrace/ ./")
    path_list = []
    for root, dirs, files in os.walk(trace_file_path):
        for file in files:
            if file.find("record_trace_") != -1:
                path_list.append(file)

    return path_list


class TestHitraceToolParse:
    @pytest.mark.L0
    def test_tool_generate_db(self):
        collect_binary_trace()
        trace_file = get_binary_trace()
        trace_file = list(set(trace_file))

        traceDb = "trace.db"
        trace_streamer_path = "trace_streamer_binary/trace_streamer.exe hitrace/"
        word_cmds = []
        for file in trace_file:
            date_str = ""
            match = re.search(r'\d{14}', file)
            if match:
                date_str = match.group()
            else:
                date_str = file.split("_trace_")[1].split("@")[0]
            trace_streamer_tool_cmd = trace_streamer_path + file + " -e " + date_str + traceDb
            word_cmds.append(trace_streamer_tool_cmd)

        for word_cmd in word_cmds:
            get_shell_result(word_cmd)
            time.sleep(3)
            assert CheckFileExists(traceDb)
