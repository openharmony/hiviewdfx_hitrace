/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HITRACE_COMMON_DEFINE_H
#define HITRACE_COMMON_DEFINE_H

#include <string>

#define EXPECTANTLY(exp) (__builtin_expect(!!(exp), true))
#define UNEXPECTANTLY(exp) (__builtin_expect(!!(exp), false))
#define UNUSED_PARAM __attribute__((__unused__))

#ifndef PAGE_SIZE
constexpr size_t PAGE_SIZE = 4096;
#endif

const std::string TRACE_TAG_ENABLE_FLAGS = "debug.hitrace.tags.enableflags";
const std::string TRACE_KEY_APP_PID = "debug.hitrace.app_pid";
const std::string TRACE_LEVEL_THRESHOLD = "persist.hitrace.level.threshold";

const std::string DEBUGFS_TRACING_DIR = "/sys/kernel/debug/tracing/";
const std::string TRACEFS_DIR = "/sys/kernel/tracing/";
const std::string TRACING_ON_NODE = "tracing_on";
const std::string TRACE_MARKER_NODE = "trace_marker";
const std::string TRACE_NODE = "trace";

const std::string TRACE_FILE_DEFAULT_DIR = "/data/log/hitrace/";
const std::string TRACE_SAVED_EVENTS_FORMAT = "saved_events_format";


#endif // HITRACE_COMMON_DEFINE_H