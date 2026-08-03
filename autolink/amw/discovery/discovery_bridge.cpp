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

#include "autolink/amw/discovery/discovery_bridge.hpp"

#include "autolink/common/log.hpp"

namespace autolink {
namespace amw {

AmwDiscoveryBackend::AmwDiscoveryBackend(
    const std::shared_ptr<IDiscoveryProvider>& provider)
    : provider_(provider) {}

bool AmwDiscoveryBackend::Start() {
    if (!provider_) {
        AERROR << "AmwDiscoveryBackend has no provider.";
        return false;
    }
    if (provider_->IsStub()) {
        AERROR << "AmwDiscoveryBackend: refusing to start stub discovery ("
               << ProviderIdName(provider_->id()) << ").";
        return false;
    }
    if (!provider_->Start()) {
        AERROR << "AmwDiscoveryBackend: provider Start failed ("
               << ProviderIdName(provider_->id()) << ").";
        return false;
    }
    AINFO << "AmwDiscoveryBackend started provider="
          << ProviderIdName(provider_->id());
    return true;
}

int64_t AmwDiscoveryBackend::Subscribe(proto::ChangeType type,
                                       const ChangeCallback& callback) {
    if (!provider_) {
        return -1;
    }
    return provider_->Subscribe(type, callback);
}

void AmwDiscoveryBackend::Unsubscribe(int64_t subscription_id) {
    if (provider_) {
        provider_->Unsubscribe(subscription_id);
    }
}

bool AmwDiscoveryBackend::Publish(const proto::ChangeMsg& msg) {
    if (!provider_) {
        return false;
    }
    return provider_->PublishChange(msg);
}

void AmwDiscoveryBackend::Shutdown() {
    if (provider_) {
        provider_->Shutdown();
    }
}

LocalDiscoveryProvider::LocalDiscoveryProvider()
    : backend_(std::make_unique<service_discovery::LocalTopologyBackend>()) {}

bool LocalDiscoveryProvider::Start() {
    return backend_->Start();
}

int64_t LocalDiscoveryProvider::Subscribe(proto::ChangeType type,
                                          const ChangeCallback& callback) {
    return backend_->Subscribe(type, callback);
}

void LocalDiscoveryProvider::Unsubscribe(int64_t subscription_id) {
    backend_->Unsubscribe(subscription_id);
}

bool LocalDiscoveryProvider::PublishChange(const proto::ChangeMsg& msg) {
    return backend_->Publish(msg);
}

void LocalDiscoveryProvider::Shutdown() {
    backend_->Shutdown();
}

}  // namespace amw
}  // namespace autolink
