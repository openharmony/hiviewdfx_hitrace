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

extern crate hitracechain;

use hitracechain::HiTraceFlag as HiTraceFlag;

const HITRACE_ID_PERSIST_ARR_LEN: usize = 16;

#[test]
fn hitracechain_rust_unit_test_001() {
    let trace_id = hitracechain::begin!("hitracechain_rust_unit_test_001", HiTraceFlag::Default);
    assert!(hitracechain::get_id().get_chain_id() == trace_id.get_chain_id());
    assert!(hitracechain::get_id().is_valid());
    hitracechain::end(&trace_id);
    assert!(!hitracechain::get_id().is_valid());
}

#[test]
fn hitracechain_rust_unit_test_002() {
    let mut trace_id = hitracechain::begin!("hitracechain_rust_unit_test_002", HiTraceFlag::IncludeAsync);
    assert!(trace_id.is_flag_enabled(HiTraceFlag::IncludeAsync));
    assert!(!trace_id.is_flag_enabled(HiTraceFlag::DoNotCreateSpan));
    trace_id.enable_flag(HiTraceFlag::DoNotCreateSpan);
    assert!(trace_id.is_flag_enabled(HiTraceFlag::DoNotCreateSpan));
    hitracechain::end(&trace_id);
}

#[test]
fn hitracechain_rust_unit_test_003() {
    let mut trace_id = hitracechain::begin!("hitracechain_rust_unit_test_003", HiTraceFlag::Default);
    assert!(trace_id.is_valid());
    trace_id.set_flags(HiTraceFlag::IncludeAsync | HiTraceFlag::TpInfo);
    assert!(trace_id.get_flags() == (HiTraceFlag::IncludeAsync | HiTraceFlag::TpInfo));
    trace_id.set_chain_id(1024u64);
    assert!(trace_id.get_chain_id() == 1024u64);
    trace_id.set_span_id(1025u64);
    assert!(trace_id.get_span_id() == 1025u64);
    trace_id.set_parent_span_id(1026u64);
    assert!(trace_id.get_parent_span_id() == 1026u64);
    hitracechain::end(&trace_id);
}

#[test]
fn hitracechain_rust_unit_test_004() {
    let mut trace_id = hitracechain::begin!("hitracechain_rust_unit_test_004", HiTraceFlag::Default);
    assert!(trace_id.is_valid());
    hitracechain::clear_id();
    trace_id = hitracechain::get_id();
    assert!(!trace_id.is_valid());
}

#[test]
fn hitracechain_rust_unit_test_005() {
    let trace_id = hitracechain::begin!("hitracechain_rust_unit_test_005", HiTraceFlag::Default);
    assert!(trace_id.is_valid());
    let trace_span_id = hitracechain::create_span();
    assert!(trace_span_id.is_valid());
    assert!(trace_span_id.get_parent_span_id() == trace_id.get_span_id());
    hitracechain::end(&trace_id);
}

#[test]
fn hitracechain_rust_unit_test_006() {
    let trace_id = hitracechain::begin!("hitracechain_rust_unit_test_006", HiTraceFlag::Default);
    let mut persist_dest_arr = [0u8; HITRACE_ID_PERSIST_ARR_LEN];
    hitracechain::id_to_bytes(&trace_id, &mut persist_dest_arr);
    let trace_id_from_arr = hitracechain::bytes_to_id(&persist_dest_arr);
    assert!(trace_id_from_arr.get_chain_id() == trace_id.get_chain_id());
    hitracechain::end(&trace_id);
}

#[test]
fn hitracechain_rust_unit_test_007() {
    let trace_id = hitracechain::begin!("hitracechain_rust_unit_test_007",
        HiTraceFlag::IncludeAsync | HiTraceFlag::TpInfo);
    hitracechain::tracepoint!(hitracechain::HiTraceCommunicationMode::Default,
        hitracechain::HiTraceTracepointType::Cs,
        &trace_id,
        "chain id of current trace is {:#x}, span id is {:#x}, parent span id is {:#x}",
        trace_id.get_chain_id(),
        trace_id.get_span_id(),
        trace_id.get_parent_span_id());
    hitracechain::end(&trace_id);
}