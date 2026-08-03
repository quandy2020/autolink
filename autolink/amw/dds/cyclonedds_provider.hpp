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
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "autolink/amw/provider.hpp"

namespace autolink {
namespace amw {

#ifdef AUTOLINK_ENABLE_CYCLONEDDS

class CycloneDdsTransportProvider final : public ITransportProvider
{
public:
    ProviderId id() const override {
        return ProviderId::kCycloneDds;
    }

    bool Init(const AmwContext& context) override;
    void Shutdown() override;

    std::shared_ptr<IPublisher> CreatePublisher(
        const EndpointDesc& endpoint) override;
    std::shared_ptr<ISubscription> CreateSubscription(
        const EndpointDesc& endpoint, const MessageCallback& callback) override;

    bool IsStub() const override {
        return false;
    }

    int32_t participant_entity() const {
        return participant_;
    }

private:
    std::atomic<bool> inited_{false};
    int32_t participant_ = 0;
};

class CycloneDdsDiscoveryProvider final : public IDiscoveryProvider
{
public:
    ProviderId id() const override {
        return ProviderId::kCycloneDds;
    }

    bool Start() override;
    int64_t Subscribe(proto::ChangeType type,
                      const ChangeCallback& callback) override;
    void Unsubscribe(int64_t subscription_id) override;
    bool PublishChange(const proto::ChangeMsg& msg) override;
    void Shutdown() override;

    bool IsStub() const override {
        return false;
    }

private:
    void OnTopologyRaw(const std::shared_ptr<message::RawMessage>& raw,
                       const transport::MessageInfo& info,
                       const proto::RoleAttributes& attr);

    std::atomic<bool> started_{false};
    std::shared_ptr<IPublisher> publisher_;
    std::shared_ptr<ISubscription> subscription_;
    std::mutex mutex_;
    int64_t next_id_ = 1;
    std::unordered_map<int64_t, std::pair<proto::ChangeType, ChangeCallback>>
        subscribers_;
};

#endif  // AUTOLINK_ENABLE_CYCLONEDDS

std::shared_ptr<ITransportProvider> CreateCycloneDdsTransportProvider();
std::shared_ptr<IDiscoveryProvider> CreateCycloneDdsDiscoveryProvider();

}  // namespace amw
}  // namespace autolink
