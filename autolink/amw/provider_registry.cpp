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

#include "autolink/amw/provider_registry.hpp"

namespace autolink {
namespace amw {

void ProviderRegistry::RegisterTransport(
    const std::shared_ptr<ITransportProvider>& provider) {
    if (provider == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    transports_[static_cast<int>(provider->id())] = provider;
}

void ProviderRegistry::RegisterDiscovery(
    const std::shared_ptr<IDiscoveryProvider>& provider) {
    if (provider == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    discoveries_[static_cast<int>(provider->id())] = provider;
}

std::shared_ptr<ITransportProvider> ProviderRegistry::GetTransport(
    ProviderId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transports_.find(static_cast<int>(id));
    if (it == transports_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<IDiscoveryProvider> ProviderRegistry::GetDiscovery(
    ProviderId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = discoveries_.find(static_cast<int>(id));
    if (it == discoveries_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<ITransportProvider> ProviderRegistry::GetTransportByName(
    const std::string& name) const {
    return GetTransport(ProviderIdFromName(name));
}

void ProviderRegistry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    transports_.clear();
    discoveries_.clear();
}

}  // namespace amw
}  // namespace autolink
