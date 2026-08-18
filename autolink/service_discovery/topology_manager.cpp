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

    const std::string host_ip = common::GlobalData::Instance()->HostIp();
    backend_ = std::make_unique<LocalTopologyBackend>();

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
        AERROR << "start LocalTopologyBackend failed.";
        init_.store(false);
        return false;
    }

    AINFO << "TopologyManager using LocalTopologyBackend host_ip=" << host_ip;

    // When a remote node appears, re-announce local channel/service roles.
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
