/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

class HiTraceIdAni implements hiTraceChain.HiTraceId {
    chainId: bigint = 0n;
    spanId?: int | undefined;
    parentSpanId?: int | undefined;
    flags?: int | undefined;
}

export default namespace hiTraceChain {
    loadLibrary("hitrace_chain_ani");
    export enum HiTraceCommunicationMode {
        DEFAULT = 0,
        THREAD = 1,
        PROCESS = 2,
        DEVICE = 3
    }

    export enum HiTraceFlag {
        DEFAULT = 0,
        INCLUDE_ASYNC = 1,
        DONOT_CREATE_SPAN = 1 << 1,
        TP_INFO = 1 << 2,
        NO_BE_INFO = 1 << 3,
        DISABLE_LOG = 1 << 4,
        FAILURE_TRIGGER = 1 << 5,
        D2D_TP_INFO = 1 << 6
    }

    export enum HiTraceTracepointType {
        CS = 0,
        CR = 1,
        SS = 2,
        SR = 3,
        GENERAL = 4
    }

    export interface HiTraceId {
        chainId: bigint;
        spanId?: int;
        parentSpanId?: int;
        flags?: int;
    }

    export native function begin(name: string, flags?: int): HiTraceId;
    export native function end(id: HiTraceId): void;
    export native function getId(): HiTraceId;
    export native function setId(id: HiTraceId): void;
    export native function clearId(): void;
    export native function createSpan(): HiTraceId;
    export native function tracepoint(mode: HiTraceCommunicationMode, type: HiTraceTracepointType, id: HiTraceId,
        msg?: string): void;
    export native function isValid(id: HiTraceId): boolean;
    export native function isFlagEnabled(id: HiTraceId, flag: HiTraceFlag): boolean;
    export native function enableFlag(id: HiTraceId, flag: HiTraceFlag): void;
}
