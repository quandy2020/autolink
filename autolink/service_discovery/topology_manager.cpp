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

#include "autolink/service_discovery/topology_manager.hpp"

#include "autolink/amw/amw.hpp"
#include "autolink/amw/discovery/discovery_bridge.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"

namespace autolink {
namespace service_discovery {

TopologyManager::TopologyManager()
    : init_(false),
      node_manager_(nullptr),
      channel_manager_(nullptr),
      service_manager_(nullptr) {
    Init();
}

TopologyManager::~TopologyManager() {
    Shutdown();
}

void TopologyManager::Shutdown() {
    ADEBUG << "topology shutdown.";
    // avoid shutdown twice
    if (!init_.exchange(false)) {
        return;
    }

    if (node_manager_) {
        node_manager_->RemoveChangeListener(node_change_conn_);
    }
    node_manager_->Shutdown();
    channel_manager_->Shutdown();
    service_manager_->Shutdown();
    if (backend_ != nullptr) {
        backend_->Shutdown();
        backend_ = nullptr;
    }
    Manager::SetTopologyBackend(nullptr);

    change_signal_.DisconnectAllSlots();
}

TopologyManager::ChangeConnection TopologyManager::AddChangeListener(
    const ChangeFunc& func) {
    return change_signal_.Connect(func);
}

void TopologyManager::RemoveChangeListener(const ChangeConnection& conn) {
    auto local_conn = conn;
    local_conn.Disconnect();
}

bool TopologyManager::Init() {
    if (init_.exchange(true)) {
        return true;
    }

    node_manager_ = std::make_shared<NodeManager>();
    channel_manager_ = std::make_shared<ChannelManager>();
    service_manager_ = std::make_shared<ServiceManager>();

    // Align with ROS 2: when a real network AMW is selected, topology rides DDS.
    // Otherwise keep the local file backend (single-host).
    amw::Amw::Instance()->Init();
    const auto& amw_ctx = amw::Amw::Instance()->context();
    const auto network_id = amw::Amw::Instance()->selected_network_provider();
    const bool network_ready = amw::Amw::Instance()->IsNetworkMiddlewareReady();
    const std::string host_ip = common::GlobalData::Instance()->HostIp();

    bool use_network = false;
    const char* fallback_reason = nullptr;
    if (!network_ready) {
        fallback_reason = "network AMW is STUB / not linked";
        AERROR << "TopologyManager: network middleware not ready "
                  "(implementation="
               << amw_ctx.implementation << " provider="
               << amw::ProviderIdName(network_id)
               << " host_ip=" << host_ip
               << "). Cross-host discovery/RTPS disabled; "
                  "using LocalTopologyBackend. Rebuild with "
                  "AUTOLINK_ENABLE_FASTDDS/CYCLONEDDS and set "
                  "AUTOLINK_AMW_IMPLEMENTATION.";
    } else {
        auto discovery =
            amw::Amw::Instance()->registry()->GetDiscovery(network_id);
        if (!discovery) {
            fallback_reason = "discovery provider missing";
            AERROR << "TopologyManager: network AMW selected ("
                   << amw::ProviderIdName(network_id)
                   << ") but discovery provider is missing; "
                      "falling back to LocalTopologyBackend. host_ip="
                   << host_ip;
        } else if (discovery->IsStub()) {
            fallback_reason = "discovery provider is stub";
            AERROR << "TopologyManager: discovery provider is STUB ("
                   << amw::ProviderIdName(network_id)
                   << "); falling back to LocalTopologyBackend. host_ip="
                   << host_ip;
        } else {
            backend_ = std::make_unique<amw::AmwDiscoveryBackend>(discovery);
            use_network = true;
        }
    }
    if (!use_network) {
        backend_ = std::make_unique<LocalTopologyBackend>();
        AWARN << "TopologyManager using LocalTopologyBackend"
              << (fallback_reason ? std::string(" (") + fallback_reason + ")"
                                  : "")
              << ". Same-host only; DIFF_HOST RTPS peers will not discover "
                 "via /autolink/topology.";
    }

    // Subscribe managers BEFORE Start so TRANSIENT_LOCAL topology history is
    // not taken and dropped with zero subscribers.
    Manager::SetTopologyBackend(backend_.get());
    bool result =
        InitNodeManager() && InitChannelManager() && InitServiceManager();
    if (!result) {
        AERROR << "init manager failed.";
        if (backend_ != nullptr) {
            backend_->Shutdown();
            backend_ = nullptr;
        }
        Manager::SetTopologyBackend(nullptr);
        node_manager_ = nullptr;
        channel_manager_ = nullptr;
        service_manager_ = nullptr;
        init_.store(false);
        return false;
    }

    if (!backend_->Start()) {
        if (use_network) {
            AERROR << "TopologyManager: AMW network discovery Start failed ("
                   << amw::ProviderIdName(network_id)
                   << "); falling back to LocalTopologyBackend. host_ip="
                   << host_ip;
            backend_->Shutdown();
            backend_ = std::make_unique<LocalTopologyBackend>();
            Manager::SetTopologyBackend(backend_.get());
            // Re-subscribe managers to the local backend.
            node_manager_->StopDiscovery();
            channel_manager_->StopDiscovery();
            service_manager_->StopDiscovery();
            if (!(InitNodeManager() && InitChannelManager() &&
                  InitServiceManager()) ||
                !backend_->Start()) {
                AERROR << "start LocalTopologyBackend failed.";
                init_.store(false);
                return false;
            }
            use_network = false;
        } else {
            AERROR << "start LocalTopologyBackend failed.";
            init_.store(false);
            return false;
        }
    }

    if (use_network) {
        AINFO << "TopologyManager using AMW network discovery: "
              << amw::ProviderIdName(network_id)
              << " implementation=" << amw_ctx.implementation
              << " host_ip=" << host_ip;
    } else {
        AINFO << "TopologyManager using LocalTopologyBackend host_ip="
              << host_ip;
    }

    // When a remote node appears, re-announce local channel/service roles.
    // Compensates for incomplete TRANSIENT_LOCAL topology history on DDS.
    node_change_conn_ = node_manager_->AddChangeListener(
        [this](const ChangeMsg& msg) { OnRemoteNodeJoin(msg); });
    return true;
}

bool TopologyManager::InitNodeManager() {
    return node_manager_->StartDiscovery(nullptr);
}

bool TopologyManager::InitChannelManager() {
    return channel_manager_->StartDiscovery(nullptr);
}

bool TopologyManager::InitServiceManager() {
    return service_manager_->StartDiscovery(nullptr);
}

void TopologyManager::OnRemoteNodeJoin(const ChangeMsg& change_msg) {
    if (change_msg.operate_type() != OperateType::OPT_JOIN ||
        change_msg.role_type() != RoleType::ROLE_NODE) {
        return;
    }
    const auto& attr = change_msg.role_attr();
    // Ignore echoes of our own node announcements.
    if (attr.process_id() == common::GlobalData::Instance()->ProcessId() &&
        attr.host_name() == common::GlobalData::Instance()->HostName()) {
        return;
    }
    AINFO << "TopologyManager: remote node join name=" << attr.node_name()
          << " host_ip=" << attr.host_ip()
          << "; republishing local channel/service roles.";
    if (channel_manager_) {
        channel_manager_->RepublishLocalRoles();
    }
    if (service_manager_) {
        service_manager_->RepublishLocalRoles();
    }
}

}  // namespace service_discovery
}  // namespace autolink
