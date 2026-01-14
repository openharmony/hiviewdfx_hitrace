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

use std::fs::{File};
use std::io::{BufRead, BufReader, Error};
use std::path::{Path};
use std::fs::OpenOptions;
use std::io::{self, Write};
use std::process::Command;
use std::sync::OnceLock;

const TRACE_FS_TRACE_FILE: &str = "/sys/kernel/tracing/";
const DEBUG_FS_TRACE_FILE: &str = "/sys/kernel/debug/tracing/";
const TRACE_MARKER: &str = "trace_marker";
const TRACE_NODE: &str = "trace";
const TRACING_ON: &str = "tracing_on";
const TRACE_TAG_ENABLE_FLAGS: &str = "debug.hitrace.tags.enableflags";

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

fn find_trace_file_path() -> Result<&'static str, Error> {
    static TRACE_PATH_LOCK: OnceLock<Option<&'static str>> = OnceLock::new();
    let ret = TRACE_PATH_LOCK.get_or_init(|| {
        let debug_trace_marker = format!("{}/{}", DEBUG_FS_TRACE_FILE, TRACE_MARKER);
        let debug_fs_path = Path::new(&debug_trace_marker);
        if matches!(debug_fs_path.try_exists(), Ok(true)) {
            return Some(TRACE_FS_TRACE_FILE);
        }
        let trace_marker = format!("{}/{}", TRACE_FS_TRACE_FILE, TRACE_MARKER);
        let trace_fs_path = Path::new(&trace_marker);
        if matches!(trace_fs_path.try_exists(), Ok(true)) {
            return Some(TRACE_FS_TRACE_FILE);
        }
        Option::None
    });
    ret.ok_or(Error::new(io::ErrorKind::NotFound, "failed find origin trace path"))
}

pub fn find_trace_content(target_str: &str) -> Result<bool, Error> {
    let file_path = find_trace_file_path()?;
    let file = File::open(file_path.to_string() + TRACE_NODE)?;
    let reader = BufReader::new(file);
    Ok(reader.lines().any(|line| match line {
        Ok(line) => {
            println!("read trace line：{}", line);
            line.contains(target_str)
        }
        Err(_) => false,
    }))
}

pub fn clear_trace_buf() -> Result<bool, Error> {
    let file_path = find_trace_file_path()?;
    let mut trace_node = OpenOptions::new()
        .write(true)
        .truncate(true)
        .create(false)
        .open(file_path.to_string() + TRACE_NODE)?;
    trace_node.write_all(b"")?;
    trace_node.flush()?;
    println!("success clear trace buffer");
    Ok(true)
}

pub fn switch_trace_on(trace_on : bool) -> Result<bool, Error> {
    let file_path = find_trace_file_path()?;
    let mut tracing_on_file = OpenOptions::new()
        .write(true)
        .open(file_path.to_string() + TRACING_ON)?;
    let content = if trace_on { "1" } else { "0" };
    tracing_on_file.write_all(content.as_bytes())?;
    tracing_on_file.flush()?;
    println!("success switch trace {}", trace_on);
    Ok(true)
}

pub fn set_trace_tag(tag: u64) -> Result<bool, Error> {
    set_params(TRACE_TAG_ENABLE_FLAGS, tag.to_string().as_str())
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