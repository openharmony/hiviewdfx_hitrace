/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include <cstring>
#include <unistd.h>
#include <iostream>
#include <thread>

#include "hitrace_meter.h"

using namespace std;
namespace {
constexpr int SLEEP_ONE_SECOND = 1;
constexpr int SLEEP_TWO_SECOND = 2;
constexpr int CYCLE_TIMES = 5;
constexpr int32_t TASK_ID = 111;
constexpr uint64_t LABEL = HITRACE_TAG_OHOS;

void FuncA()
{
    cout << "funcA" << endl;
    sleep(SLEEP_ONE_SECOND);
}

void FuncB()
{
    cout << "funcB" << endl;
    sleep(SLEEP_TWO_SECOND);
}

void FuncC()
{
    cout << "funcC" << endl;
    int num = 0;
    for (int i = 0; i < CYCLE_TIMES; i++) {
        CountTrace(HITRACE_TAG_OHOS, "count number", ++num);
        sleep(SLEEP_ONE_SECOND);
    }
}

void ThreadFunc1()
{
    StartAsyncTrace(LABEL, "testAsync", TASK_ID);
    for (int i = 0; i < CYCLE_TIMES; ++i) {
        cout << "t1" << endl;
        sleep(SLEEP_ONE_SECOND);
    }
}

void ThreadFunc2()
{
    for (int i = 0; i < CYCLE_TIMES; ++i) {
        cout << "t2" << endl;
        sleep(SLEEP_ONE_SECOND);
    }
    FinishAsyncTrace(LABEL, "testAsync", TASK_ID);
}
} // namespace

int main()
{
    thread t1(ThreadFunc1);
    t1.join();

    StartTrace(LABEL, "testStart");
    sleep(SLEEP_ONE_SECOND);

    StartTrace(LABEL, "funcAStart", SLEEP_ONE_SECOND); // 打印起始点
    FuncA();
    FinishTrace(LABEL);
    sleep(SLEEP_TWO_SECOND);

    thread t2(ThreadFunc2);
    t2.join();

    StartTrace(LABEL, "funcBStart", SLEEP_TWO_SECOND);
    FuncB();
    FinishTrace(LABEL);
    sleep(SLEEP_TWO_SECOND);

    sleep(SLEEP_ONE_SECOND);
    FinishTrace(LABEL);
    FuncC();

    return 0;
}
