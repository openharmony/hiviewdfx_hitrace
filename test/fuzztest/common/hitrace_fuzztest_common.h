/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef FAULTLOGGERD_FUZZERTEST_COMMON_H
#define FAULTLOGGERD_FUZZERTEST_COMMON_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <hitrace_meter.h>
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
template<typename T>
void StreamToValueInfo(const uint8_t* data, T& value)
{
    do {
        errno_t err = memcpy_s(&(value), sizeof(value), (data), sizeof(value));
        if (err != 0) {
            std::cout << "memcpy_s return value is abnormal!" << std::endl;
        }
        (data) += sizeof(value);
    } while (0);
}

void GenerateTagStr(const uint64_t tags, std::string& tagStr)
{
    if (tags & HITRACE_TAG_SECURITY) {
        tagStr += " security";
    }
    if (tags & HITRACE_TAG_ANIMATION) {
        tagStr += " animation";
    }
    if (tags & HITRACE_TAG_MUSL) {
        tagStr += " musl";
    }
    if (tags & HITRACE_TAG_FFRT) {
        tagStr += " ffrt";
    }
    if (tags & HITRACE_TAG_OHOS) {
        tagStr += " ohos";
    }
    if (tags & HITRACE_TAG_APP) {
        tagStr += " app";
    }
    if (tags & HITRACE_TAG_HDCD) {
        tagStr += " hdcd";
    }
    if (tags & HITRACE_TAG_ACE) {
        tagStr += " ace";
    }
    if (tags & HITRACE_TAG_POWER) {
        tagStr += " power";
    }
}

void GenerateTagVec(const uint64_t tags, std::vector<std::string>& tagVec)
{
    if (tags & HITRACE_TAG_SECURITY) {
        tagVec.push_back("security");
    }
    if (tags & HITRACE_TAG_ANIMATION) {
        tagVec.push_back("animation");
    }
    if (tags & HITRACE_TAG_MUSL) {
        tagVec.push_back("musl");
    }
    if (tags & HITRACE_TAG_FFRT) {
        tagVec.push_back("ffrt");
    }
    if (tags & HITRACE_TAG_OHOS) {
        tagVec.push_back("ohos");
    }
    if (tags & HITRACE_TAG_APP) {
        tagVec.push_back("app");
    }
    if (tags & HITRACE_TAG_HDCD) {
        tagVec.push_back("hdcd");
    }
    if (tags & HITRACE_TAG_ACE) {
        tagVec.push_back("ace");
    }
    if (tags & HITRACE_TAG_POWER) {
        tagVec.push_back("power");
    }
}
}
}
#endif
