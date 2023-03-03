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

//! Rust macros defined in hitracechain.

/// tracepoint hitracechain information
///
/// # Example
///
/// ```
/// extern crate hitracechain;
///
/// let trace_id = hitracechain::get_id();
/// hitracechain::tracepoint!(hitracechain::HiTraceCommunicationMode::Default,
///     hitracechain::HiTraceTracepointType::Cs, &trace_id,
///     "chain id is {}, span id is {}, parent span id is {}",
///     trace_id.get_chain_id(), trace_id.get_span_id()
///     trace)id_get_parent_span_id());
/// hitracechain::end(trace_id);
/// ```
/// # Safty
/// 
/// Call C ffi border function, all risks are under control.
///
#[macro_export]
macro_rules! tracepoint {
    ($communication_mode:expr, $tracepoint_type:expr, $p_id:expr, $($arg:tt)*) => {
        let args = format!($($arg)*);
        unsafe {
            hitracechain::HiTraceChainTracepointExWrapper(
                $communication_mode as std::ffi::c_int,
                $tracepoint_type as std::ffi::c_int,
                $p_id as *const hitracechain::HiTraceId,
                std::ffi::CString::new(args).expect("").as_ptr() as *const std::ffi::c_char);
        }
    }
}

/// start a new trace
///
/// # Example
///
/// ```
/// extern crate hitracechain;
///
/// let trace_id = hitracechain::begin!("process name", hitracechain::HiTraceFlag::Default);
/// hitracechain::end(trace_id);
/// ```
///
#[macro_export]
macro_rules! begin {
    ($process_name:literal, $trace_flag:expr) => {
        hitracechain::begin($process_name, $trace_flag as i32);
    }
}