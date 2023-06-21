/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef HIVIEWDFX_HITRACE_H
#define HIVIEWDFX_HITRACE_H
/**
 * @addtogroup Hitrace
 * @{
 *
 * @brief hiTraceMeter provides APIs for system performance trace.
 *
 * You can call the APIs provided by hiTraceMeter in your own service logic to effectively
 * track service processes and check the system performance.
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 *
 * @since 10
 */

/**
 * @file trace.h
 *
 * @brief Defines APIs of the HiTraceMeter module for performance trace.
 *
 * Sample code: \n
 * Synchronous timeslice trace event: \n
 *     OH_HiTrace_StartTrace("hitraceTest");\n
 *     OH_HiTrace_FinishTrace();\n
 * Output: \n
 *     <...>-1668    (-------) [003] ....   135.059377: tracing_mark_write: B|1668|H:hitraceTest \n
 *     <...>-1668    (-------) [003] ....   135.059415: tracing_mark_write: E|1668| \n
 * Asynchronous timeslice trace event:\n
 *     OH_HiTrace_StartAsyncTrace("hitraceTest", 123); \n
 *     OH_HiTrace_FinishAsyncTrace("hitraceTest", 123); \n
 * Output: \n
 *     <...>-2477    (-------) [001] ....   396.427165: tracing_mark_write: S|2477|H:hitraceTest 123 \n
 *     <...>-2477    (-------) [001] ....   396.427196: tracing_mark_write: F|2477|H:hitraceTest 123 \n
 * Integer value trace event:\n
 *     OH_HiTrace_CountTrace("hitraceTest", 500); \n
 * Output: \n
 *     <...>-2638    (-------) [002] ....   458.904382: tracing_mark_write: C|2638|H:hitraceTest 500 \n
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 * @since 10
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Marks the start of a synchronous trace task.
 *
 * The <b>OH_HiTrace_StartTrace</b> and <b>OH_HiTrace_FinishTrace</b> APIs must be used in pairs.
 * The two APIs can be used in nested mode. The stack data structure is used for matching during trace data parsing.
 *
 * @param name Name of a trace task.
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 * @since 10
 */
void OH_HiTrace_StartTrace(const char *name);

/**
 * @brief Marks the end of a synchronous trace task.
 *
 * This API must be used with <b>OH_HiTrace_StartTrace</b> in pairs. During trace data parsing, the system matches
 * it with the <b>OH_HiTrace_StartTrace</b> API recently invoked in the service process.
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 * @since 10
 */
void OH_HiTrace_FinishTrace(void);

/**
 * @brief Marks the start of an asynchronous trace task.
 *
 * This API is called to implement performance trace in asynchronous manner. The start and end of an asynchronous
 * trace task do not occur in sequence. Therefore, a unique <b>taskId</b> is required to ensure proper data parsing.
 * It is passed as an input parameter for the asynchronous API.
 * This API is used with <b>OH_HiTrace_FinishAsyncTrace</b> in pairs. The two APIs that have the same name and
 * task ID together form an asynchronous timeslice trace task.
 * If multiple trace tasks with the same name need to be performed at the same time or a trace task needs to be
 * performed multiple times concurrently, different task IDs must be specified in <b>OH_HiTrace_StartTrace</b>.
 * If the trace tasks with the same name are not performed at the same time, the same taskId can be used.
 *
 * @param name Name of the asynchronous trace task.
 * @param taskId ID of the asynchronous trace task. The start and end of an asynchronous trace task do not occur in
 * sequence. Therefore, the start and end of an asynchronous trace need to be matched based on the task name and the
 * unique task ID together.
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 * @since 10
 */
void OH_HiTrace_StartAsyncTrace(const char *name, int32_t taskId);

/**
 * @brief Marks the end of an asynchronous trace task.
 *
 * This API is called in the callback function after an asynchronous trace is complete.
 * It is used with <b>OH_HiTrace_StartAsyncTrace</b> in pairs. Its name and task ID must be the same as those of
 * <b>OH_HiTrace_StartAsyncTrace</b>.
 *
 * @param name Name of the asynchronous trace task.
 * @param taskId ID of the asynchronous trace task. The start and end of an asynchronous trace task do not occur in
 * sequence. Therefore, the start and end of an asynchronous trace need to be matched based on the task name and the
 * unique task ID together.
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 * @since 10
 */
void OH_HiTrace_FinishAsyncTrace(const char *name, int32_t taskId);

/**
 * @brief Traces the value change of an integer variable based on its name.
 *
 * This API can be executed for multiple times to trace the value change of a given integer variable at different
 * time points.
 *
 * @param name Name of the integer variable. It does not need to be the same as the real variable name.
 * @param count Integer value. Generally, an integer variable can be passed.
 *
 * @syscap SystemCapability.HiviewDFX.HiTrace
 * @since 10
 */
void OH_HiTrace_CountTrace(const char *name, int64_t count);

#ifdef __cplusplus
}
#endif
#endif // HIVIEWDFX_HITRACE_H
