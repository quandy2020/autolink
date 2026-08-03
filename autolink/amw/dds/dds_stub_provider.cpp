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

#include "autolink/amw/dds/dds_stub_provider.hpp"

#include <memory>

#include "autolink/amw/dds/cyclonedds_provider.hpp"
#include "autolink/amw/dds/fastdds_provider.hpp"
#include "autolink/amw/provider_registry.hpp"
#include "autolink/common/log.hpp"

namespace autolink {
namespace amw {

DdsStubTransportProvider::DdsStubTransportProvider(ProviderId id) : id_(id) {}

bool DdsStubTransportProvider::Init(const AmwContext& context) {
    (void)context;
    inited_.store(true);
    AINFO << "DDS stub transport initialized: " << ImplementationName(id_)
          << " (" << ProviderIdName(id_) << ").";
    return true;
}

void DdsStubTransportProvider::Shutdown() {
    inited_.store(false);
}

std::shared_ptr<IPublisher> DdsStubTransportProvider::CreatePublisher(
    const EndpointDesc& endpoint) {
    AWARN << ImplementationName(id_)
          << " stub: CreatePublisher not implemented (channel="
          << endpoint.attr.channel_name() << ").";
    return nullptr;
}

std::shared_ptr<ISubscription> DdsStubTransportProvider::CreateSubscription(
    const EndpointDesc& endpoint, const MessageCallback& callback) {
    (void)callback;
    AWARN << ImplementationName(id_)
          << " stub: CreateSubscription not implemented (channel="
          << endpoint.attr.channel_name() << ").";
    return nullptr;
}

DdsStubDiscoveryProvider::DdsStubDiscoveryProvider(ProviderId id) : id_(id) {}

bool DdsStubDiscoveryProvider::Start() {
    started_.store(false);
    AWARN << ImplementationName(id_)
          << " discovery stub: Start refused (no network discovery).";
    return false;
}

int64_t DdsStubDiscoveryProvider::Subscribe(proto::ChangeType type,
                                            const ChangeCallback& callback) {
    (void)type;
    (void)callback;
    AWARN << ImplementationName(id_) << " discovery stub: Subscribe is a no-op.";
    return -1;
}

void DdsStubDiscoveryProvider::Unsubscribe(int64_t subscription_id) {
    (void)subscription_id;
}

bool DdsStubDiscoveryProvider::PublishChange(const proto::ChangeMsg& msg) {
    (void)msg;
    AWARN << ImplementationName(id_)
          << " discovery stub: PublishChange is a no-op.";
    return false;
}

void DdsStubDiscoveryProvider::Shutdown() {
    started_.store(false);
}

void RegisterBuiltinDdsProviders(ProviderRegistry* registry) {
    if (registry == nullptr) {
        return;
    }
    // Fill missing FastDDS/Cyclone via factories (real or stub per CMake).
    // Never overwrite an already-registered provider.
    if (registry->GetTransport(ProviderId::kFastDds) == nullptr) {
        registry->RegisterTransport(CreateFastDdsTransportProvider());
    }
    if (registry->GetDiscovery(ProviderId::kFastDds) == nullptr) {
        registry->RegisterDiscovery(CreateFastDdsDiscoveryProvider());
    }
    if (registry->GetTransport(ProviderId::kCycloneDds) == nullptr) {
        registry->RegisterTransport(CreateCycloneDdsTransportProvider());
    }
    if (registry->GetDiscovery(ProviderId::kCycloneDds) == nullptr) {
        registry->RegisterDiscovery(CreateCycloneDdsDiscoveryProvider());
    }
    static const ProviderId kStubOnly[] = {
        ProviderId::kOpenDds,
        ProviderId::kConnextDds,
    };
    for (ProviderId id : kStubOnly) {
        if (registry->GetTransport(id) == nullptr) {
            registry->RegisterTransport(
                std::make_shared<DdsStubTransportProvider>(id));
        }
        if (registry->GetDiscovery(id) == nullptr) {
            registry->RegisterDiscovery(
                std::make_shared<DdsStubDiscoveryProvider>(id));
        }
    }
}

}  // namespace amw
}  // namespace autolink
