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

//! Interface definition for rust APIs of hitracechain.

#[macro_use]
pub mod macros;

use std::ffi::{CString, c_char, c_int, c_ulonglong};
use std::ops::BitOr;

/// Enumerate trace flag
#[non_exhaustive]
pub enum HiTraceFlag {
    /// DEFAULT: default value.
    Default = 0,

    /// trace sync and async call. default: trace sync call only.
    IncludeAsync = 1 << 0,

    /// do not create child span. default: create child span.
    DoNotCreateSpan = 1 << 1,

    /// output tracepoint info in span. default: do not output tracepoint info.
    TpInfo = 1 << 2,

    /// do not output begin and end info. default: output begin and end info.
    NoBeInfo = 1 << 3,

    /// do not add id to log. default: add id to log.
    DoNotEnableLog = 1 << 4,

    /// the trace is triggered by fault.
    FaultTigger = 1 << 5,

    /// output device-to-device tracepoint info in span only. default: do not output device-to-device tracepoint info.
    D2dTpInfo = 1 << 6,
}

/// Add support for bitor operating for HiTraceFlag
impl BitOr for HiTraceFlag {
    type Output = i32;

    fn bitor(self, rhs: Self) -> Self::Output {
        (self as i32) | (rhs as i32)
    }
}

/// Enumerate trace point type
#[non_exhaustive]
pub enum HiTraceTracepointType {

    /// client send
    Cs = 0,

    /// client receive
    Cr = 1,

    /// server send
    Ss = 2,

    /// server receive
    Sr = 3,

    /// general info
    General = 4,
}

/// Enumerate trace communication mode
#[non_exhaustive]
pub enum HiTraceCommunicationMode {
    /// unspecified communication mode
    Default = 0,

    /// thread-to-thread communication mode
    Thread = 1,

    /// process-to-process communication mode
    Process = 2,

    /// device-to-device communication mode
    Device = 3,
}

/// This type represent to HiTraceIdStruct defined in C.
#[repr(C)]
pub struct HiTraceIdStruct {
    trace_details: [u64;2],
}

/// Alias for HiTraceIdStruct
pub type HiTraceId = HiTraceIdStruct;

impl HiTraceId {
    /// Judge whether the trace id is valid or not.
    pub fn is_valid(&self) -> bool {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainIsValidWrapper(self as *const HiTraceId)
        }
    }

    /// Judge whether the trace id has enabled a trace flag or not.
    pub fn is_flag_enabled(&self, flag: HiTraceFlag) -> bool {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainIsFlagEnabledWrapper(self as *const HiTraceId, flag as i32)
        }
    }

    /// Enable the designative trace flag for the trace id.
    pub fn enable_flag(&mut self, flag: HiTraceFlag) {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainEnableFlagWrapper(self as *mut HiTraceId, flag as i32);
        }
    }

    /// Set trace flags for the trace id.
    pub fn set_flags(&mut self, flags: i32) {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainSetFlagsWrapper(self as *mut HiTraceId, flags);
        }
    }

    /// Get trace flags of the trace id.
    pub fn get_flags(&self) -> i32 {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainGetFlagsWrapper(self as *const HiTraceId)
        }
    }

    /// Set chain id of the trace id.
    pub fn set_chain_id(&mut self, chain_id: u64) {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainSetChainIdWrapper(self as *mut HiTraceId, chain_id);
        }
    }

    /// Get chain id of the trace id.
    pub fn get_chain_id(&self) -> u64 {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainGetChainIdWrapper(self as *const HiTraceId)
        }
    }

    /// Set span id of the trace id.
    pub fn set_span_id(&mut self, span_id: u64) {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainSetSpanIdWrapper(self as *mut HiTraceId, span_id);
        }
    }

    /// Get span id of the trace id.
    pub fn get_span_id(&self)-> u64 {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainGetSpanIdWrapper(self as *const HiTraceId)
        }
    }

     /// Set parent span id of the trace id.
    pub fn set_parent_span_id(&mut self, parent_span_id: u64) {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainSetParentSpanIdWrapper(self as *mut HiTraceId, parent_span_id);
        }
    }

    /// Get parent span id of the trace id.
    pub fn get_parent_span_id(&self) -> u64 {
        // Safty: call C ffi border function, all risks are under control.
        unsafe {
            HiTraceChainGetParentSpanIdWrapper(self as *const HiTraceId)
        }
    }
}

/// Start tracing a process impl.
pub fn begin(name: &str, flags: i32) -> HiTraceId {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        let name = CString::new(name).unwrap();
        HiTraceChainBegin(name.as_ptr() as *const c_char, flags)
    }
}

/// Stop process tracing and clear trace id of current thread if the given trace
/// id is valid, otherwise do nothing.
pub fn end(id: &HiTraceId) {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        HiTraceChainEnd(id as *const HiTraceId);
    }
}

/// Get trace id of current thread, and return a invalid trace id if no
/// trace id belong to current thread
pub fn get_id() -> HiTraceId {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        HiTraceChainGetId()
    }
}

/// Set id as trace id of current thread. Do nothing if id is invalid.
pub fn set_id(id: &HiTraceId) {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        HiTraceChainSetId(id as *const HiTraceId);
    }
}

/// Clear trace id of current thread and set it invalid.
pub fn clear_id() {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        HiTraceChainClearId();
    }
}

/// Create a new span id according to the trace id of current thread.
pub fn create_span() -> HiTraceId {
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        HiTraceChainCreateSpan()
    }
}

/// Persist a trace id into a uint8_t array
pub fn id_to_bytes(p_id: &HiTraceId, p_id_array: &mut [u8]) -> i32 {
    let arr_len = p_id_array.len();
    let mut id_array_wrapper: Vec<u8> = vec![];
    for &item in p_id_array.iter().take(arr_len) {
        id_array_wrapper.push(item);
    }
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        let ret = HiTraceChainIdToBytesWrapper(p_id as *const HiTraceId,
            id_array_wrapper.as_mut_ptr(),
            arr_len as c_int);
        p_id_array[..arr_len].copy_from_slice(&id_array_wrapper[..arr_len]);
        ret
    }
}

/// Build a trace id form a uint8_t array
pub fn bytes_to_id(p_id_array: &[u8]) -> HiTraceId {
    let mut id_array_wrapper: Vec<u8> = vec![];
    for &item in p_id_array {
        id_array_wrapper.push(item);
    }
    // Safty: call C ffi border function, all risks are under control.
    unsafe {
        HiTraceChainBytesToIdWrapper(id_array_wrapper.as_mut_ptr(), p_id_array.len() as c_int)
    }
}

extern "C" {
    /// ffi border function
    fn HiTraceChainBegin(name: *const c_char, _flags: c_int) -> HiTraceId;

    /// ffi border function
    fn HiTraceChainEnd(id: *const HiTraceId);

    /// ffi border function
    fn HiTraceChainGetId() -> HiTraceId;

    /// ffi border function
    fn HiTraceChainSetId(id: *const HiTraceId);

    /// ffi border function
    fn HiTraceChainClearId();

    /// ffi border function
    fn HiTraceChainCreateSpan() -> HiTraceId;

    /// ffi border function
    fn HiTraceChainIdToBytesWrapper(id: *const HiTraceId, p_id_array: *const u8, len: c_int) -> c_int;

    /// ffi border function
    fn HiTraceChainBytesToIdWrapper(p_id_array: *const u8, len: c_int) -> HiTraceId;

    /// ffi border function
    #[allow(dead_code)]
    pub fn HiTraceChainTracepointExWrapper(communication_mode: c_int, trace_type: c_int,
        p_id: *const HiTraceId, fmt: *const c_char, ...);

    /// ffi border function
    fn HiTraceChainIsValidWrapper(p_id: *const HiTraceId) -> bool;

    /// ffi border function
    fn HiTraceChainIsFlagEnabledWrapper(p_id: *const HiTraceId, flag: c_int) -> bool;

    /// ffi border function
    fn HiTraceChainEnableFlagWrapper(p_id: *mut HiTraceId, flag: c_int);

    /// ffi border function
    fn HiTraceChainSetFlagsWrapper(p_id: *mut HiTraceId, flags: c_int);

    /// ffi border function
    fn HiTraceChainGetFlagsWrapper(p_id: *const HiTraceId) -> c_int;

    /// ffi border function
    fn HiTraceChainSetChainIdWrapper(p_id: *mut HiTraceId, chain_id: c_ulonglong);

    /// ffi border function
    fn HiTraceChainGetChainIdWrapper(p_id: *const HiTraceId) -> c_ulonglong;

    /// ffi border function
    fn HiTraceChainSetSpanIdWrapper(p_id: *mut HiTraceId, span_id: c_ulonglong);

    /// ffi border function
    fn HiTraceChainGetSpanIdWrapper(p_id: *const HiTraceId) -> c_ulonglong;

    /// ffi border function
    fn HiTraceChainSetParentSpanIdWrapper(p_id: *mut HiTraceId,
        parent_span_id: c_ulonglong);

    /// ffi border function
    fn HiTraceChainGetParentSpanIdWrapper(p_id: *const HiTraceId) -> c_ulonglong;
}
