/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

use std::io::{Error};
use std::process::Command;

const TRACE_TAG_ENABLE_FLAGS: &str = "debug.hitrace.tags.enableflags";

extern "C" {
    // bool SetTraceEventPid();
    pub fn SetTraceEventPid() -> bool;

    // bool FindTraceRecord(const char* record);
    pub fn FindTraceRecord(record: *const i8) -> bool;
}

fn set_params(key: &str, value: &str) -> Result<bool, Error> {
    match Command::new("param").arg("set").args([key, value]).output() {
        Ok(out) if !out.status.success() => {
            println!( "failed execute command code：{:?}），err_msg：{}", out.status.code(),
                String::from_utf8_lossy(&out.stderr));
            Ok(false)
        }
        Ok(out) => {
            let output_str = String::from_utf8_lossy(&out.stdout).into_owned();
            println!( "execute command result: {}", output_str);
            Ok(output_str.contains("success"))
        }
        Err(err) => {
            Err(err)
        }
    }
}

pub fn find_trace_content(target_str: &str) -> Result<bool, Error> {
    let c_record = std::ffi::CString::new(target_str)?;
    let result = unsafe { FindTraceRecord(c_record.as_ptr() as *const i8) };
    Ok(result)
}
pub fn set_trace_tag(tag: u64) -> Result<bool, Error> {
    set_params(TRACE_TAG_ENABLE_FLAGS, tag.to_string().as_str())
}

pub fn set_event_pid() -> Result<bool, Error> {
    let result = unsafe { SetTraceEventPid() };
    Ok(result)
}

pub struct TraceInfo {
    pub(crate) trace_type: char,
    pub(crate) name: String,
    pub(crate) value: i32
}

pub fn create_trace_record(trace_info: &TraceInfo) -> Option<String> {
    match trace_info.trace_type {
        'B' => {
            Some(format!("B|{}|H:{}|", std::process::id(), trace_info.name))
        },
        'E' => {
            Some(format!("E|{}|", std::process::id()))
        },
        'S' | 'F' | 'C' => {
            Some(format!("{}|{}|H:{}|{}|", trace_info.trace_type,
                std::process::id(), trace_info.name, trace_info.value))
        }
        _ => {
            None
        }
    }
}