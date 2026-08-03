/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
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
 *****************************************************************************/

#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "autolink/amw/provider.hpp"
#include "autolink/amw/types.hpp"

namespace autolink {
namespace amw {

class ProviderRegistry
{
public:
    void RegisterTransport(const std::shared_ptr<ITransportProvider>& provider);
    void RegisterDiscovery(const std::shared_ptr<IDiscoveryProvider>& provider);

    std::shared_ptr<ITransportProvider> GetTransport(ProviderId id) const;
    std::shared_ptr<IDiscoveryProvider> GetDiscovery(ProviderId id) const;

    std::shared_ptr<ITransportProvider> GetTransportByName(
        const std::string& name) const;

    void Clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, std::shared_ptr<ITransportProvider>> transports_;
    std::unordered_map<int, std::shared_ptr<IDiscoveryProvider>> discoveries_;
};

}  // namespace amw
}  // namespace autolink
