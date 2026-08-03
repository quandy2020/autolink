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

#include "autolink/amw/provider.hpp"
#include "autolink/service_discovery/topology_backend.hpp"

namespace autolink {
namespace amw {

// Adapts IDiscoveryProvider to the existing ITopologyBackend interface so
// TopologyManager can later swap discovery without changing Manager APIs.
class AmwDiscoveryBackend final : public service_discovery::ITopologyBackend
{
public:
    explicit AmwDiscoveryBackend(
        const std::shared_ptr<IDiscoveryProvider>& provider);

    bool Start() override;
    int64_t Subscribe(proto::ChangeType type,
                      const ChangeCallback& callback) override;
    void Unsubscribe(int64_t subscription_id) override;
    bool Publish(const proto::ChangeMsg& msg) override;
    void Shutdown() override;

    std::shared_ptr<IDiscoveryProvider> provider() const {
        return provider_;
    }

private:
    std::shared_ptr<IDiscoveryProvider> provider_;
};

// Wraps LocalTopologyBackend as an IDiscoveryProvider for Registry use.
class LocalDiscoveryProvider final : public IDiscoveryProvider
{
public:
    LocalDiscoveryProvider();

    ProviderId id() const override {
        return ProviderId::kLocal;
    }

    bool Start() override;
    int64_t Subscribe(proto::ChangeType type,
                      const ChangeCallback& callback) override;
    void Unsubscribe(int64_t subscription_id) override;
    bool PublishChange(const proto::ChangeMsg& msg) override;
    void Shutdown() override;

    service_discovery::LocalTopologyBackend* backend() {
        return backend_.get();
    }

private:
    std::unique_ptr<service_discovery::LocalTopologyBackend> backend_;
};

}  // namespace amw
}  // namespace autolink
