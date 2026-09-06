/******************************************************************************
 * Copyright 2026 The Openbot Authors (duyongquan)
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

#include "autolink/transport/rtps/participant_hub.hpp"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "autolink/common/log.hpp"

namespace autolink {
namespace transport {
namespace {

std::string Trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> ParseDiscoveryServers() {
    const char* env = std::getenv("AUTOLINK_DISCOVERY_SERVER");
    if (env == nullptr || *env == '\0') {
        return {};
    }
    std::vector<std::string> servers;
    std::stringstream ss(env);
    std::string item;
    while (std::getline(ss, item, ',')) {
        auto trimmed = Trim(item);
        if (!trimmed.empty()) {
            servers.push_back(std::move(trimmed));
        }
    }
    return servers;
}

}  // namespace

RtpsParticipantHub& RtpsParticipantHub::Instance() {
    static RtpsParticipantHub instance;
    return instance;
}

bool RtpsParticipantHub::Init(const proto::RtpsParticipantAttr& attr) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (inited_) {
        return true;
    }

    const auto servers = ParseDiscoveryServers();
    auto topology = std::make_shared<Participant>(attr, ParticipantRole::kTopology,
                                                  servers);
    auto transport = std::make_shared<Participant>(
            attr, ParticipantRole::kTransport, servers);

    if (!topology->Init() || !transport->Init()) {
        AERROR << "RtpsParticipantHub Init failed";
        topology->Shutdown();
        transport->Shutdown();
        topology.reset();
        transport.reset();
        return false;
    }

    topology_ = std::move(topology);
    transport_ = std::move(transport);
    inited_ = true;
    AINFO << "RtpsParticipantHub ready"
          << (servers.empty() ? " discovery=SIMPLE"
                              : " discovery=CLIENT servers=" +
                                        std::to_string(servers.size()));
    return true;
}

ParticipantPtr RtpsParticipantHub::TopologyParticipant() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return topology_;
}

ParticipantPtr RtpsParticipantHub::TransportParticipant() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return transport_;
}

void RtpsParticipantHub::Shutdown() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!inited_ && topology_ == nullptr && transport_ == nullptr) {
        return;
    }
    if (topology_ != nullptr) {
        topology_->Shutdown();
        topology_.reset();
    }
    if (transport_ != nullptr) {
        transport_->Shutdown();
        transport_.reset();
    }
    inited_ = false;
}

}  // namespace transport
}  // namespace autolink
