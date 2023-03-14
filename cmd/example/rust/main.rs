/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

//! hitrace_meter example_crate for rust.
extern crate hitrace_meter_rust;
use hitrace_meter_rust::{start_trace, finish_trace, start_trace_async, finish_trace_async, count_trace};
const HITRACE_TAG_OHOS : u64 = 1 << 30;
const HITRACE_TASK_ID : i32 = 666;

/// example function A
fn func_a() {
    println!("funcA!!!");
}

/// example function B
fn func_b() {
    println!("funcB!!!");
}

/// trace example for rust
fn main() {
    start_trace(HITRACE_TAG_OHOS, "rust test start trace funcA!!!!!!");
    func_a();
    count_trace(HITRACE_TAG_OHOS, "rust test count trace funcA!!!!!!", 0);
    finish_trace(HITRACE_TAG_OHOS);

    start_trace_async(HITRACE_TAG_OHOS, "rust test start trace async funcB!!!!!!", HITRACE_TASK_ID);
    func_b();
    count_trace(HITRACE_TAG_OHOS, "rust test count trace funcB!!!!!!", 1);
    finish_trace_async(HITRACE_TAG_OHOS, "rust finish trace async funcB!!!!!!", HITRACE_TASK_ID);
}
