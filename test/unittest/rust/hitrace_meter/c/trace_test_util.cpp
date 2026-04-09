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

#include "trace_test_util.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>

namespace {
constexpr auto DEBUGFS_TRACING_DIR = "/sys/kernel/debug/tracing/";
constexpr auto TRACEFS_DIR = "/sys/kernel/tracing/";
constexpr auto TRACE_MARKER_NODE = "trace_marker";
constexpr auto TRACE_NODE = "trace";
constexpr auto TRACING_ON_NODE = "tracing_on";

const std::string& GetTraceRootPath()
{
    static const std::string traceRootPath = [] {
        if (access((std::string(DEBUGFS_TRACING_DIR) + std::string(TRACE_MARKER_NODE)).c_str(), F_OK) != -1) {
            return DEBUGFS_TRACING_DIR;
        }
        if (access((std::string(TRACEFS_DIR) + std::string(TRACE_MARKER_NODE)).c_str(), F_OK) != -1) {
            return TRACEFS_DIR;
        }
        return "";
    }();
    return traceRootPath;
}
}

class TraceMonitor {
public:
    static TraceMonitor& GetInstance();
    TraceMonitor(const TraceMonitor&) = delete;
    TraceMonitor& operator=(const TraceMonitor&) = delete;
    bool FindTraceRecord(const std::string& content);
    bool WriteToFile(const std::string& filePath, const std::string& content, bool append = false);
private:
    TraceMonitor();
    ~TraceMonitor();
    std::mutex dataMutex_;
};

TraceMonitor& TraceMonitor::GetInstance()
{
    static TraceMonitor instance;
    return instance;
}

TraceMonitor::TraceMonitor()
{
    WriteToFile(TRACING_ON_NODE, "1");
    WriteToFile(TRACE_NODE, "");
}

TraceMonitor::~TraceMonitor()
{
    WriteToFile(TRACING_ON_NODE, "0");
}

bool TraceMonitor::FindTraceRecord(const std::string& content)
{
    std::lock_guard<std::mutex> lock(dataMutex_);
    std::ifstream ifs(GetTraceRootPath() + TRACE_NODE);
    if (!ifs.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        std::cout << "read trace line: " << line << std::endl;
        if (line.find(content) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool TraceMonitor::WriteToFile(const std::string& filePath, const std::string& content, bool append)
{
    std::ofstream ofs(GetTraceRootPath() + filePath, append ? std::ios::app : std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << content;
    ofs.close();
    return true;
}

bool SetTraceEventPid()
{
    return TraceMonitor::GetInstance().WriteToFile("set_event_pid", std::to_string(gettid()), true);
}

bool FindTraceRecord(const char* record)
{
    return TraceMonitor::GetInstance().FindTraceRecord(record);
}
