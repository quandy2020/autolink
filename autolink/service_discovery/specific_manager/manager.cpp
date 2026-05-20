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

#include "autolink/service_discovery/specific_manager/manager.hpp"

#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"
#include "autolink/message/message_traits.hpp"
#include "autolink/time/time.hpp"

namespace autolink {
namespace service_discovery {

std::mutex Manager::backend_mutex_;
ITopologyBackend* Manager::topology_backend_ = nullptr;

Manager::Manager()
    : is_shutdown_(false),
      is_discovery_started_(false),
      allowed_role_(0),
      change_type_(proto::ChangeType::CHANGE_PARTICIPANT),
      channel_name_("") {
    host_name_ = common::GlobalData::Instance()->HostName();
    process_id_ = common::GlobalData::Instance()->ProcessId();
}

Manager::~Manager() {
    Shutdown();
}

void Manager::SetTopologyBackend(ITopologyBackend* backend) {
    std::lock_guard<std::mutex> lock(backend_mutex_);
    topology_backend_ = backend;
}

bool Manager::StartDiscovery(RtpsParticipant* participant) {
    (void)participant;
    is_discovery_started_.store(true);
    ITopologyBackend* backend = nullptr;
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        backend = topology_backend_;
    }
    if (backend == nullptr) {
        AERROR << "topology backend is not set.";
        return false;
    }
    backend_subscription_id_ = backend->Subscribe(
        change_type_,
        [this](const ChangeMsg& msg) { HandleRemoteChange(msg); });
    if (backend_subscription_id_ < 0) {
        AERROR << "failed to subscribe topology backend.";
        return false;
    }
    return true;
}

void Manager::StopDiscovery() {
    if (!is_discovery_started_.exchange(false)) {
        return;
    }

    ITopologyBackend* backend = nullptr;
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        backend = topology_backend_;
    }
    if (backend != nullptr && backend_subscription_id_ >= 0) {
        backend->Unsubscribe(backend_subscription_id_);
    }
    backend_subscription_id_ = -1;
}

void Manager::Shutdown() {
    if (is_shutdown_.exchange(true)) {
        return;
    }

    StopDiscovery();
    signal_.DisconnectAllSlots();
}

bool Manager::Join(const RoleAttributes& attr, RoleType role,
                   bool need_publish) {
    if (is_shutdown_.load()) {
        ADEBUG << "the manager has been shut down.";
        return false;
    }
    RETURN_VAL_IF(!((1 << role) & allowed_role_), false);
    RETURN_VAL_IF(!Check(attr), false);
    ChangeMsg msg;
    Convert(attr, role, OperateType::OPT_JOIN, &msg);
    Dispose(msg);
    if (need_publish) {
        return Publish(msg);
    }
    return true;
}

bool Manager::Leave(const RoleAttributes& attr, RoleType role) {
    if (is_shutdown_.load()) {
        ADEBUG << "the manager has been shut down.";
        return false;
    }
    RETURN_VAL_IF(!((1 << role) & allowed_role_), false);
    RETURN_VAL_IF(!Check(attr), false);
    ChangeMsg msg;
    Convert(attr, role, OperateType::OPT_LEAVE, &msg);
    Dispose(msg);
    if (NeedPublish(msg)) {
        return Publish(msg);
    }
    return true;
}

Manager::ChangeConnection Manager::AddChangeListener(const ChangeFunc& func) {
    return signal_.Connect(func);
}

void Manager::RemoveChangeListener(const ChangeConnection& conn) {
    auto local_conn = conn;
    local_conn.Disconnect();
}

bool Manager::NeedPublish(const ChangeMsg& msg) const {
    (void)msg;
    return true;
}

void Manager::Convert(const RoleAttributes& attr, RoleType role,
                      OperateType opt, ChangeMsg* msg) {
    msg->set_timestamp(autolink::Time::Now().ToNanosecond());
    msg->set_change_type(change_type_);
    msg->set_operate_type(opt);
    msg->set_role_type(role);
    auto role_attr = msg->mutable_role_attr();
    role_attr->CopyFrom(attr);
    if (role_attr->host_name().empty()) {
        role_attr->set_host_name(host_name_);
    }
    if (role_attr->process_id() == 0) {
        role_attr->set_process_id(process_id_);
    }
}

void Manager::Notify(const ChangeMsg& msg) {
    signal_(msg);
}

void Manager::OnRemoteChange(const std::string& msg_str) {
    (void)msg_str;
}

void Manager::HandleRemoteChange(const ChangeMsg& msg) {
    if (is_shutdown_.load()) {
        ADEBUG << "the manager has been shut down.";
        return;
    }

    if (IsFromSameProcess(msg)) {
        return;
    }
    RETURN_IF(!Check(msg.role_attr()));
    Dispose(msg);
}

bool Manager::Publish(const ChangeMsg& msg) {
    if (!is_discovery_started_.load()) {
        ADEBUG << "discovery is not started.";
        return false;
    }
    ITopologyBackend* backend = nullptr;
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        backend = topology_backend_;
    }
    if (backend == nullptr) {
        AERROR << "topology backend is not set.";
        return false;
    }
    return backend->Publish(msg);
}

bool Manager::IsFromSameProcess(const ChangeMsg& msg) {
    auto& host_name = msg.role_attr().host_name();
    int process_id = msg.role_attr().process_id();

    if (process_id != process_id_ || host_name != host_name_) {
        return false;
    }
    return true;
}

}  // namespace service_discovery
}  // namespace autolink
