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
use std::sync::Once;
use std::thread;
use std::time::Duration;

const HITRACE_TAG_OHOS : u64 = 1 << 30;
const HITRACE_TASK_ID : i32 = 666;
mod trace_test;

fn before_each() -> bool {
    static TRACE_PATH_LOCK: std::sync::Once = Once::new();
    let init_function = || {
        if let Err(err) = trace_test::set_trace_tag(HITRACE_TAG_OHOS) {
            println!("failed set trace tag for {}", err);
        }
    };
    TRACE_PATH_LOCK.call_once(init_function);
    if let Err(err) = trace_test::set_event_pid() {
        println!("failed set trace tag for {}", err);
    }
    thread::sleep(Duration::from_millis(100));
    true
}

fn test_body01() -> Result<bool, Error> {
    hitrace_meter_rust::start_trace(HITRACE_TAG_OHOS, "hitrace_meter_rust_unit_test_001");
    hitrace_meter_rust::finish_trace(HITRACE_TAG_OHOS);
    thread::sleep(Duration::from_millis(100));
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
    thread::sleep(Duration::from_millis(100));
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
    thread::sleep(Duration::from_millis(100));
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
    before_each();
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
    before_each();
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
    before_each();
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