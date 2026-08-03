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

#include "autolink/amw/local/local_provider.hpp"
#include "autolink/amw/network_adapter.hpp"
#include "autolink/amw/provider_registry.hpp"
#include "autolink/amw/router.hpp"
#include "autolink/amw/types.hpp"
#include "autolink/common/log.hpp"
#include "autolink/common/macros.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/proto/transport_conf.pb.h"
#include "autolink/transport/receiver/receiver.hpp"
#include "autolink/transport/transmitter/transmitter.hpp"

namespace autolink {
namespace amw {

class Amw
{
public:
    ~Amw();

    bool Init();
    void Shutdown();

    ProviderRegistry* registry();
    Router* router();
    const AmwContext& context() const {
        return context_;
    }
    std::shared_ptr<LocalProvider> local_provider() const;

    // True when selected network provider is real (non-stub), e.g. FastDDS ON.
    bool IsNetworkMiddlewareReady() const;

    // Backward-compatible alias.
    bool IsFastDdsEnabled() const {
        return IsNetworkMiddlewareReady() &&
               ProviderIdFromName(context_.default_network_provider) ==
                   ProviderId::kFastDds;
    }

    ProviderId selected_network_provider() const {
        return ProviderIdFromName(context_.default_network_provider);
    }

    template <typename M>
    std::shared_ptr<transport::Transmitter<M>> CreateTransmitter(
        const proto::RoleAttributes& attr, proto::OptionalMode mode);

    template <typename M>
    std::shared_ptr<transport::Receiver<M>> CreateReceiver(
        const proto::RoleAttributes& attr,
        const typename transport::Receiver<M>::MessageListener& listener,
        proto::OptionalMode mode);

private:
    // Returns true if env explicitly selected an implementation.
    bool LoadConfig();
    void RegisterDefaults();
    // When chosen provider is stub and env did not pin it, prefer a real builtin.
    void PreferReadyNetworkProviderIfNeeded(bool env_selected);

    bool inited_ = false;
    bool network_init_ok_ = false;
    AmwContext context_;
    ProviderRegistry registry_;
    Router router_;
    std::shared_ptr<LocalProvider> local_provider_;

    DECLARE_SINGLETON(Amw)
};

template <typename M>
std::shared_ptr<transport::Transmitter<M>> Amw::CreateTransmitter(
    const proto::RoleAttributes& attr, proto::OptionalMode mode) {
    if (!inited_ && !Init()) {
        return nullptr;
    }

    // HYBRID is created by Transport factory (avoids circular includes).
    if (mode == proto::OptionalMode::HYBRID) {
        AERROR << "Amw::CreateTransmitter(HYBRID) should be handled by "
                  "Transport.";
        return nullptr;
    }

    auto provider_id = router_.ProviderForMode(mode);
    if (provider_id == ProviderId::kLocal) {
        if (!local_provider_) {
            return nullptr;
        }
        auto local_mode = mode;
        if (local_mode != proto::OptionalMode::INTRA &&
            local_mode != proto::OptionalMode::SHM) {
            local_mode = proto::OptionalMode::SHM;
        }
        return local_provider_->CreateTransmitter<M>(attr, local_mode);
    }

    auto provider = registry_.GetTransport(provider_id);
    if (!provider) {
        AWARN << "AMW transport provider not registered: "
              << ProviderIdName(provider_id);
        return nullptr;
    }
    EndpointDesc endpoint;
    endpoint.attr = attr;
    endpoint.local_mode = mode;
    auto publisher = provider->CreatePublisher(endpoint);
    if (!publisher) {
        AWARN << "AMW provider " << ProviderIdName(provider_id)
              << " failed to create publisher for channel "
              << attr.channel_name();
        return nullptr;
    }
    return std::make_shared<NetworkTransmitter<M>>(attr, publisher);
}

template <typename M>
std::shared_ptr<transport::Receiver<M>> Amw::CreateReceiver(
    const proto::RoleAttributes& attr,
    const typename transport::Receiver<M>::MessageListener& listener,
    proto::OptionalMode mode) {
    if (!inited_ && !Init()) {
        return nullptr;
    }

    if (mode == proto::OptionalMode::HYBRID) {
        AERROR << "Amw::CreateReceiver(HYBRID) should be handled by Transport.";
        return nullptr;
    }

    auto provider_id = router_.ProviderForMode(mode);
    if (provider_id == ProviderId::kLocal) {
        if (!local_provider_) {
            return nullptr;
        }
        auto local_mode = mode;
        if (local_mode != proto::OptionalMode::INTRA &&
            local_mode != proto::OptionalMode::SHM) {
            local_mode = proto::OptionalMode::SHM;
        }
        return local_provider_->CreateReceiver<M>(attr, listener, local_mode);
    }

    auto provider = registry_.GetTransport(provider_id);
    if (!provider) {
        AWARN << "AMW transport provider not registered: "
              << ProviderIdName(provider_id);
        return nullptr;
    }
    return std::make_shared<NetworkReceiver<M>>(attr, listener, provider);
}

}  // namespace amw
}  // namespace autolink
