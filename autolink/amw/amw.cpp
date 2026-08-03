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

#include "autolink/amw/amw.hpp"

#include <cstdlib>
#include <initializer_list>

#include "autolink/amw/dds/cyclonedds_provider.hpp"
#include "autolink/amw/dds/dds_stub_provider.hpp"
#include "autolink/amw/dds/fastdds_provider.hpp"
#include "autolink/amw/discovery/discovery_bridge.hpp"
#include "autolink/amw/plugin_loader.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"

namespace autolink {
namespace amw {

namespace {

const char* FirstNonEmptyEnv(std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const char* value = std::getenv(key);
        if (value != nullptr && value[0] != '\0') {
            return value;
        }
    }
    return nullptr;
}

}  // namespace

Amw::Amw() = default;

Amw::~Amw() {
    Shutdown();
}

bool Amw::Init() {
    if (inited_) {
        return true;
    }
    const bool env_selected = LoadConfig();
    RegisterDefaults();
    PreferReadyNetworkProviderIfNeeded(env_selected);
    if (local_provider_ && !local_provider_->Init(context_)) {
        AERROR << "AMW LocalProvider Init failed.";
        return false;
    }

    const ProviderId selected =
        ProviderIdFromName(context_.default_network_provider);
    auto transport = registry_.GetTransport(selected);
    network_init_ok_ = false;
    if (transport) {
        if (transport->Init(context_)) {
            network_init_ok_ = !transport->IsStub();
        } else {
            AWARN << "AMW network provider Init failed: "
                  << ProviderIdName(selected)
                  << " stub=" << transport->IsStub();
        }
    }

    inited_ = true;
    const bool network_ready = IsNetworkMiddlewareReady();
    auto discovery = registry_.GetDiscovery(selected);
    const char* domain_env = FirstNonEmptyEnv(
        {"AUTOLINK_DOMAIN_ID", "ROS_DOMAIN_ID"});
    const char* cyclone_uri = std::getenv("CYCLONEDDS_URI");
    AINFO << "AMW initialized: implementation=" << context_.implementation
          << " network_provider=" << context_.default_network_provider
          << " network_ready=" << network_ready
          << " transport_stub="
          << (transport ? transport->IsStub() : true) << " discovery_stub="
          << (discovery ? discovery->IsStub() : true)
          << " domain_id="
          << (domain_env != nullptr ? domain_env : "0")
          << " host_ip=" << common::GlobalData::Instance()->HostIp()
          << " CYCLONEDDS_URI="
          << ((cyclone_uri != nullptr && cyclone_uri[0] != '\0') ? "set"
                                                                 : "unset");
    if (!network_ready) {
        if (transport && !transport->IsStub()) {
            AERROR << "AMW real DDS provider Init failed — cross-host RTPS "
                      "will NOT work. Check domain_id, AUTOLINK_IP/HostIp NIC "
                      "match, firewall, and vendor env (CYCLONEDDS_URI / "
                      "FastDDS XML). provider="
                   << ProviderIdName(selected);
        } else {
            AERROR << "AMW network middleware is STUB — cross-host RTPS will "
                      "NOT work. Rebuild with -DAUTOLINK_ENABLE_FASTDDS=ON "
                      "and/or -DAUTOLINK_ENABLE_CYCLONEDDS=ON, then set "
                      "AUTOLINK_AMW_IMPLEMENTATION / RMW_IMPLEMENTATION.";
        }
    }
    return true;
}

void Amw::Shutdown() {
    if (!inited_) {
        return;
    }

    static const ProviderId kVendors[] = {
        ProviderId::kFastDds,
        ProviderId::kCycloneDds,
        ProviderId::kOpenDds,
        ProviderId::kConnextDds,
    };
    for (ProviderId id : kVendors) {
        auto transport = registry_.GetTransport(id);
        if (transport) {
            transport->Shutdown();
        }
        auto discovery = registry_.GetDiscovery(id);
        if (discovery) {
            discovery->Shutdown();
        }
    }

    if (local_provider_) {
        local_provider_->Shutdown();
    }
    auto local_disc = registry_.GetDiscovery(ProviderId::kLocal);
    if (local_disc) {
        local_disc->Shutdown();
    }
    registry_.Clear();
    local_provider_.reset();
    network_init_ok_ = false;
    inited_ = false;
}

ProviderRegistry* Amw::registry() {
    return &registry_;
}

Router* Amw::router() {
    return &router_;
}

std::shared_ptr<LocalProvider> Amw::local_provider() const {
    return local_provider_;
}

bool Amw::IsNetworkMiddlewareReady() const {
    // Defensive: some call sites check readiness before Transport exists.
    const_cast<Amw*>(this)->Init();
    const ProviderId id =
        ProviderIdFromName(context_.default_network_provider);
    auto transport = registry_.GetTransport(id);
    return network_init_ok_ && transport != nullptr && !transport->IsStub();
}

bool Amw::LoadConfig() {
    bool env_selected = false;
    const auto& conf = common::GlobalData::Instance()->Config();
    if (conf.has_transport_conf()) {
        const auto& transport = conf.transport_conf();
        if (transport.has_communication_mode()) {
            router_.SetCommunicationMode(transport.communication_mode());
        }
        if (transport.has_amw_conf()) {
            const auto& amw_conf = transport.amw_conf();
            if (!amw_conf.implementation().empty()) {
                context_.implementation = amw_conf.implementation();
            } else if (!amw_conf.default_network_provider().empty()) {
                context_.implementation = ImplementationName(ProviderIdFromName(
                    amw_conf.default_network_provider()));
            }
            if (!amw_conf.default_network_provider().empty()) {
                context_.default_network_provider =
                    amw_conf.default_network_provider();
            }
        }
    }

    if (const char* env = FirstNonEmptyEnv(
            {"AUTOLINK_AMW_IMPLEMENTATION", "RMW_IMPLEMENTATION",
             "AUTOLINK_AMW_NETWORK_PROVIDER"})) {
        context_.implementation = env;
        env_selected = true;
    }

    ProviderId network_id = ProviderId::kFastDds;
    if (!ResolveNetworkImplementation(context_.implementation, &network_id)) {
        AWARN << "Unknown AMW/RMW implementation '" << context_.implementation
              << "', falling back to amw_fastdds.";
        network_id = ProviderId::kFastDds;
        env_selected = false;
    }
    context_.implementation = ImplementationName(network_id);
    context_.default_network_provider = ProviderIdName(network_id);
    router_.SetDefaultNetworkProvider(network_id);
    return env_selected;
}

void Amw::PreferReadyNetworkProviderIfNeeded(bool env_selected) {
    // Env pin wins even if that vendor is stub (explicit developer choice).
    if (env_selected) {
        return;
    }
    const ProviderId selected =
        ProviderIdFromName(context_.default_network_provider);
    auto current = registry_.GetTransport(selected);
    if (current != nullptr && !current->IsStub()) {
        return;
    }

    static const ProviderId kPreferOrder[] = {
        ProviderId::kFastDds,
        ProviderId::kCycloneDds,
    };
    for (ProviderId id : kPreferOrder) {
        auto transport = registry_.GetTransport(id);
        if (transport == nullptr || transport->IsStub()) {
            continue;
        }
        AWARN << "Default AMW implementation '" << context_.implementation
              << "' is stub; auto-selecting '" << ImplementationName(id)
              << "'. Set AUTOLINK_AMW_IMPLEMENTATION to pin a vendor.";
        context_.implementation = ImplementationName(id);
        context_.default_network_provider = ProviderIdName(id);
        router_.SetDefaultNetworkProvider(id);
        return;
    }
}

void Amw::RegisterDefaults() {
    local_provider_ = std::make_shared<LocalProvider>();
    registry_.RegisterTransport(local_provider_);
    registry_.RegisterDiscovery(std::make_shared<LocalDiscoveryProvider>());

    // Prefer external plugin (ROS 2 rmw_implementation style) when present.
    LoadedPlugin plugin;
    if (TryLoadExternalPlugin(context_.implementation, &plugin)) {
        registry_.RegisterTransport(plugin.transport);
        registry_.RegisterDiscovery(plugin.discovery);
        AINFO << "Using external AMW implementation: " << plugin.identifier;
    } else {
        // Built-ins: FastDDS / Cyclone real-or-stub + remaining vendor stubs.
        registry_.RegisterTransport(CreateFastDdsTransportProvider());
        registry_.RegisterDiscovery(CreateFastDdsDiscoveryProvider());
        registry_.RegisterTransport(CreateCycloneDdsTransportProvider());
        registry_.RegisterDiscovery(CreateCycloneDdsDiscoveryProvider());
        static const ProviderId kStubVendors[] = {
            ProviderId::kOpenDds,
            ProviderId::kConnextDds,
        };
        for (ProviderId id : kStubVendors) {
            if (registry_.GetTransport(id) == nullptr) {
                registry_.RegisterTransport(
                    std::make_shared<DdsStubTransportProvider>(id));
            }
            if (registry_.GetDiscovery(id) == nullptr) {
                registry_.RegisterDiscovery(
                    std::make_shared<DdsStubDiscoveryProvider>(id));
            }
        }
    }

    // Ensure every builtin id has at least a stub for Router lookups.
    for (const auto& name : ListBuiltinImplementations()) {
        const ProviderId id = ProviderIdFromName(name);
        if (!IsNetworkProvider(id)) {
            continue;
        }
        if (registry_.GetTransport(id) == nullptr) {
            registry_.RegisterTransport(
                std::make_shared<DdsStubTransportProvider>(id));
        }
        if (registry_.GetDiscovery(id) == nullptr) {
            registry_.RegisterDiscovery(
                std::make_shared<DdsStubDiscoveryProvider>(id));
        }
    }
}

}  // namespace amw
}  // namespace autolink
