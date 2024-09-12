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
import pkg_resources
import pytest
import os
import time

""" Global Parameters
customize here !!!
"""
get_version = "V1.0.0"

def check_library_installation(library_name):
    try:
        pkg_resources.get_distribution(library_name)
        return 0
    except pkg_resources.DistributionNotFound:
        print(f"\n\n{library_name} is not installed.\n\n")
        print(f"try to use command below:")
        print(f"pip install {library_name}")
        return 1

 
if __name__  == '__main__':
    if check_library_installation("pytest"):
        subprocess.check_call(["pip", "install", "-r", "requirements.txt"])
        if check_library_installation("pytest"):
            exit(1)

    start_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
    pytest.main()
    end_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
    report_time = time.strftime('%Y-%m-%d_%H_%M_%S', time.localtime(time.time()))
    report_dir = os.path.join(os.getcwd(), "reports")
    report_file = os.path.join(report_dir, f"{report_time}report.html")
    print(f"Test over, the script version is {get_version},"
        f" start at {start_time}, end at {end_time} \n"
        f"=======>{report_file} is saved. \n"
    )
    input("=======>press [Enter] key to Show logs.")


    # os.system("allure generate temp -o reports")
