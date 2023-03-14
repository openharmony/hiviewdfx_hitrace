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

//! hitrace_meter dylib_create for rust.
use std::ffi::{CString, c_char, c_int, c_longlong, c_ulonglong};

/// Track the beginning of a context
pub fn start_trace(label: u64, value: &str) {
    let value_raw_ptr = CString::new(value).unwrap();
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        StartTraceWrapper(label, value_raw_ptr.as_ptr() as *const c_char);
    }
}

/// Track the end of a context
pub fn finish_trace(label: u64) {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        FinishTrace(label);
    }
}

/// Track the beginning of an asynchronous event
pub fn start_trace_async(label: u64, value: &str, task_id: i32) {
    let value_raw_ptr = CString::new(value).unwrap();
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        StartAsyncTraceWrapper(label, value_raw_ptr.as_ptr() as *const c_char, task_id);
    }
}

/// Track the end of an asynchronous event
pub fn finish_trace_async(label: u64, value: &str, task_id: i32) {
    let value_raw_ptr = CString::new(value).unwrap();
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        FinishAsyncTraceWrapper(label, value_raw_ptr.as_ptr() as *const c_char, task_id);
    }
}

/// Track the 64-bit integer counter value
pub fn count_trace(label: u64, name: &str, count: i64) {
    let name_raw_ptr = CString::new(name).unwrap();
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        CountTraceWrapper(label, name_raw_ptr.as_ptr() as *const c_char, count);
    }
}

extern "C" {
    /// ffi border function -> start trace
    pub(crate) fn StartTraceWrapper(label: c_ulonglong, value: *const c_char);

    /// ffi border function -> finish trace
    pub(crate) fn FinishTrace(label: c_ulonglong);

    /// ffi border function -> start async trace
    pub(crate) fn StartAsyncTraceWrapper(label: c_ulonglong, value: *const c_char, taskId: c_int);

    /// ffi border function -> finish async trace
    pub(crate) fn FinishAsyncTraceWrapper(label: c_ulonglong, value: *const c_char, taskId: c_int);

    /// ffi border function -> count trace
    pub(crate) fn CountTraceWrapper(label: c_ulonglong, name: *const c_char, count: c_longlong);
}
