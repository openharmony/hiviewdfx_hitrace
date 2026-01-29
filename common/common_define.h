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
constexpr uint64_t S_TO_NS = 1000000000;
constexpr uint64_t MS_TO_NS = 1000000;
constexpr uint64_t S_TO_MS = 1000;

constexpr int DEFAULT_FILE_SIZE = 100 * 1024;

static const char* const TRACE_TAG_ENABLE_FLAGS = "debug.hitrace.tags.enableflags";
static const char* const TRACE_KEY_APP_PID = "debug.hitrace.app_pid";
static const char* const TRACE_LEVEL_THRESHOLD = "persist.hitrace.level.threshold";

static const char* const DEBUGFS_TRACING_DIR = "/sys/kernel/debug/tracing/";
static const char* const TRACEFS_DIR = "/sys/kernel/tracing/";
static const char* const TRACING_ON_NODE = "tracing_on";
static const char* const TRACE_MARKER_NODE = "trace_marker";
static const char* const TRACE_NODE = "trace";
static const char* const TRACE_BUFFER_SIZE_NODE = "buffer_size_kb";

static const char* const TRACE_FILE_DEFAULT_DIR = "/data/log/hitrace/";
static const char* const TRACE_SAVED_EVENTS_FORMAT = "saved_events_format";
static const char* const CACHE_FILE_PREFIX = "cache_";
#endif // HITRACE_COMMON_DEFINE_H