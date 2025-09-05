/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#ifndef TRACE_STRATEGY_FACTORY_H
#define TRACE_STRATEGY_FACTORY_H

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "hitrace_define.h"
#include "singleton.h"
#include "trace_dump_strategy.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#define REGISTER_TRACE_STRATEGY(traceType, strategy)           \
static const bool reg##strategy =                              \
    TraceStrategyFactory::GetInstance().Register((traceType),  \
        []() { return std::make_unique<strategy>(); })

class TraceStrategyFactory : public Singleton<TraceStrategyFactory> {
    DECLARE_SINGLETON(TraceStrategyFactory);

public:
    using Creator = std::function<std::unique_ptr<ITraceDumpStrategy>()>;
    bool Register(TraceDumpType type, Creator creator);
    std::unique_ptr<ITraceDumpStrategy> Create(TraceDumpType type);

private:
    std::mutex mutex_;
    std::unordered_map<int, Creator> registry_;
};
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
#endif // TRACE_STRATEGY_FACTORY_H