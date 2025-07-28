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

export default namespace hiTraceMeter {
    loadLibrary("hitrace_meter_ani");
    export enum HiTraceOutputLevel {
        DEBUG = 0,
        INFO = 1,
        CRITICAL = 2,
        COMMERCIAL = 3,
        MAX = 3
    }

    export native function startTrace(name: string, taskId: number): void;
    export native function finishTrace(name: string, taskId: number): void;
    export native function traceByValue(name: string, count: number): void;
    export native function startSyncTrace(level: HiTraceOutputLevel, name: string, customArgs?: string): void;
    export native function finishSyncTrace(level: HiTraceOutputLevel): void;
    export native function startAsyncTrace(level: HiTraceOutputLevel, name: string, taskId: number,
        customCategory: string, customArgs?: string): void;
    export native function finishAsyncTrace(level: HiTraceOutputLevel, name: string, taskId: number): void;
    export native function traceByValue(level: HiTraceOutputLevel, name: string, count: number): void;
    export native function isTraceEnabled(): boolean;
}
