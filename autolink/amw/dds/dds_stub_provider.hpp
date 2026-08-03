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

#include <atomic>

#include "autolink/amw/provider.hpp"

namespace autolink {
namespace amw {

// Generic stub for any DDS vendor. Compiles without vendor SDKs.
// CreatePublisher / CreateSubscription return nullptr until real integration.
class DdsStubTransportProvider final : public ITransportProvider
{
public:
    explicit DdsStubTransportProvider(ProviderId id);

    ProviderId id() const override {
        return id_;
    }

    bool Init(const AmwContext& context) override;
    void Shutdown() override;

    std::shared_ptr<IPublisher> CreatePublisher(
        const EndpointDesc& endpoint) override;
    std::shared_ptr<ISubscription> CreateSubscription(
        const EndpointDesc& endpoint, const MessageCallback& callback) override;

    bool IsStub() const override {
        return true;
    }

private:
    ProviderId id_;
    std::atomic<bool> inited_{false};
};

class DdsStubDiscoveryProvider final : public IDiscoveryProvider
{
public:
    explicit DdsStubDiscoveryProvider(ProviderId id);

    ProviderId id() const override {
        return id_;
    }

    bool Start() override;
    int64_t Subscribe(proto::ChangeType type,
                      const ChangeCallback& callback) override;
    void Unsubscribe(int64_t subscription_id) override;
    bool PublishChange(const proto::ChangeMsg& msg) override;
    void Shutdown() override;

    bool IsStub() const override {
        return true;
    }

private:
    ProviderId id_;
    std::atomic<bool> started_{false};
};

void RegisterBuiltinDdsProviders(class ProviderRegistry* registry);

}  // namespace amw
}  // namespace autolink
