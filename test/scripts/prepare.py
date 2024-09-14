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
 
# 自动安装依赖
def install_dependencies(requirements_file):
    try:
        subprocess.check_call(["pip", "install", "-r", requirements_file])
        print(f"install requirements.txt success")
    except subprocess.CalledProcessError as e:
        print(f"install dependence fail: {str(e)}")
 
# # 自动生成依赖文件
# def generate_requirements_file(output_file):
#     try:
#         subprocess.check_call(["pip", "freeze"], text=True, stdout=open(output_file, 'w'))
#     except subprocess.CalledProcessError as e:
#         print(f"生成requirements.txt失败: {str(e)}")
 
# 使用方法
# generate_requirements_file("requirements.txt")
install_dependencies("requirements.txt")