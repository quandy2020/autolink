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
    backend_ = std::make_unique<LocalTopologyBackend>();
    if (!backend_->Start()) {
        AERROR << "start local topology backend failed.";
        init_.store(false);
        return false;
    }
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

}  // namespace service_discovery
}  // namespace autolink
