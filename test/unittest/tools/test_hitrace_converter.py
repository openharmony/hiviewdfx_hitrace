#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025 Huawei Device Co., Ltd.
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
#

import os
import sys
import unittest

current_dir = os.path.dirname(os.path.abspath(__file__))
hitrace_converter_dir = os.path.abspath(os.path.join(current_dir, "..", "..", "..", "tools", "hitrace_converter"))
test_trace_file = os.path.abspath(os.path.join(current_dir, "sample", "record_trace_20250320101116@2500-814308989.sys"))
test_trace_expect_file = os.path.abspath(os.path.join(current_dir, "sample", "record_trace_20250320101116@2500-814308989.systrace"))
test_trace_result_file = os.path.abspath(os.path.join(current_dir, "sample", "record_trace_20250320101116@2500-814308989.txt"))
sys.path.insert(0, hitrace_converter_dir)

import hitrace_converter

class TestUpdatePackage(unittest.TestCase):
    def setUp(self):
        print("set up")
        if os.path.exists(test_trace_result_file) is True:
            os.remove(test_trace_result_file)

    def tearDown(self):
        print("tear down")
        if os.path.exists(test_trace_result_file) is True:
            os.remove(test_trace_result_file)

    def _check_the_same_as_sample(self):
        expect_file = open(test_trace_expect_file, "r")
        result_file = open(test_trace_result_file, "r")
        expect_file_lines = expect_file.readlines()
        result_file_lines = result_file.readlines()
        
        expect_file.close()
        result_file.close()
        # check the same line
        self.assertTrue(len(result_file_lines) == len(expect_file_lines))

        # check the same content
        for row in range(len(expect_file_lines)):
            self.assertTrue(expect_file_lines[row].rstrip() == result_file_lines[row].rstrip())
        pass

    def test_parse_trace_to_systrace(self):
        print("test parser trace file")
        hitrace_converter.binary_file = test_trace_file
        hitrace_converter.out_file = test_trace_result_file
        hitrace_converter.parse_binary_trace_file()
        self._check_the_same_as_sample()
        print("test parser trace file finish")
        pass


if __name__ == '__main__':
    unittest.main()