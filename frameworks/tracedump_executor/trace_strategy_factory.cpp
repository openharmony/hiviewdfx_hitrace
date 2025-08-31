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

#include "trace_strategy_factory.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

TraceStrategyFactory::TraceStrategyFactory()
{
}

TraceStrategyFactory::~TraceStrategyFactory()
{
}

bool TraceStrategyFactory::Register(TraceDumpType type, Creator creator)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return registry_.emplace(static_cast<int>(type), std::move(creator)).second;
}

std::unique_ptr<ITraceDumpStrategy> TraceStrategyFactory::Create(TraceDumpType type)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registry_.find(static_cast<int>(type));
    if (it == registry_.end()) {
        return nullptr;
    }
    return (it->second)();
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS