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

//! Panic trigger for Rust.
extern crate hitrace_meter_rust;

use std::io::Error;
use std::sync::OnceLock;
const HITRACE_TAG_OHOS : u64 = 1 << 30;
const HITRACE_TASK_ID : i32 = 666;
mod trace_test;

fn before_all() -> bool {
    static TRACE_PATH_LOCK: OnceLock<Option<bool>> = OnceLock::new();
    let ret = TRACE_PATH_LOCK.get_or_init(|| {
        println!("before_all {}", HITRACE_TAG_OHOS);
        if let Err(err) = trace_test::set_trace_tag(HITRACE_TAG_OHOS) {
            println!("failed set trace tag for {}", err);
            return Some(false);
        }
        if let Err(err) = trace_test::switch_trace_on(true) {
            println!("failed enable trace for {}", err);
            return Some(false);
        }
        if let Err(err) = trace_test::clear_trace_buf() {
            println!("failed clear trace buffer for {}", err);
        }
        Some(true)
    });
    ret.is_some() && ret.unwrap()
}

fn test_body01() -> Result<bool, Error> {
    hitrace_meter_rust::start_trace(HITRACE_TAG_OHOS, "hitrace_meter_rust_unit_test_001");
    hitrace_meter_rust::finish_trace(HITRACE_TAG_OHOS);
    let start_record = trace_test::create_trace_record(&trace_test::TraceInfo {
        trace_type:'B',
        name: String::from("hitrace_meter_rust_unit_test_001"),
        value: HITRACE_TASK_ID
    });
    assert!(trace_test::find_trace_content(start_record.unwrap().as_str())?);
    let finish_record = trace_test::create_trace_record(&trace_test::TraceInfo {
        trace_type:'E',
        name: String::from("hitrace_meter_rust_unit_test_001"),
        value: HITRACE_TASK_ID
    });
    assert!(trace_test::find_trace_content(finish_record.unwrap().as_str())?);
    Ok(true)
}

fn test_body02() -> Result<bool, Error> {
    hitrace_meter_rust::start_trace_async(HITRACE_TAG_OHOS,
        "hitrace_meter_rust_unit_test_002", HITRACE_TASK_ID);
    hitrace_meter_rust::finish_trace_async(HITRACE_TAG_OHOS,
        "hitrace_meter_rust_unit_test_002", HITRACE_TASK_ID);
    let start_record = trace_test::create_trace_record(&trace_test::TraceInfo {
        trace_type:'S',
        name: String::from("hitrace_meter_rust_unit_test_002"),
        value: HITRACE_TASK_ID
    });
    assert!(start_record.is_some());

    assert!(trace_test::find_trace_content(start_record.unwrap().as_str())?);
    let finish_record = trace_test::create_trace_record(&trace_test::TraceInfo {
        trace_type:'F',
        name: String::from("hitrace_meter_rust_unit_test_002"),
        value: HITRACE_TASK_ID
    });
    assert!(trace_test::find_trace_content(finish_record.unwrap().as_str())?);
    Ok(true)
}

fn test_body03() -> Result<bool, Error> {
    const COUNT: i64 = 2;
    hitrace_meter_rust::count_trace(HITRACE_TAG_OHOS, "hitrace_meter_rust_unit_test_003", COUNT);
    let count_record = trace_test::create_trace_record(&trace_test::TraceInfo {
        trace_type:'C',
        name: String::from("hitrace_meter_rust_unit_test_003"),
        value: COUNT as i32
    });
    assert!(trace_test::find_trace_content(count_record.unwrap().as_str())?);
    Ok(true)
}

#[test]
fn hitrace_meter_rust_unit_test01() {
    assert!(before_all());
    match test_body01() {
        Ok(ret) => {
            assert!(ret)
        }
        Err(err) => {
            print!("hitrace_meter_rust_unit_test failed for {}", err);
            panic!()
        }
    }
}

#[test]
fn hitrace_meter_rust_unit_test02() {
    assert!(before_all());
    match test_body02() {
        Ok(ret) => {
            assert!(ret)
        }
        Err(err) => {
            print!("hitrace_meter_rust_unit_test failed for {}", err);
            panic!()
        }
    }
}

#[test]
fn hitrace_meter_rust_unit_test03() {
    assert!(before_all());
    match test_body03() {
        Ok(ret) => {
            assert!(ret)
        }
        Err(err) => {
            print!("hitrace_meter_rust_unit_test failed for {}", err);
            panic!()
        }
    }
}