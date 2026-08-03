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

#include "autolink/python/bridge/utils_bridge.hpp"

#include <unistd.h>

#include <algorithm>

#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/topology_manager.hpp"

namespace autolink {
namespace python_support {
namespace {

using autolink::proto::RoleAttributes;

void SleepSeconds(uint8_t sleep_s) {
    if (sleep_s > 0) {
        ::sleep(sleep_s);
    }
}

}  // namespace

std::string GetChannelMsgType(const std::string& channel_name,
                              uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::string msg_type;
    topology->channel_manager()->GetMsgType(channel_name, &msg_type);
    return msg_type;
}

std::vector<std::string> GetChannels(uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::vector<std::string> channels;
    topology->channel_manager()->GetChannelNames(&channels);
    return channels;
}

std::unordered_map<std::string, std::vector<std::string>> GetChannelsInfo(
    uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::vector<RoleAttributes> roles;
    topology->channel_manager()->GetWriters(&roles);
    std::unordered_map<std::string, std::vector<std::string>> info;
    for (auto& attr : roles) {
        std::string msgdata;
        attr.SerializeToString(&msgdata);
        info[attr.channel_name()].emplace_back(msgdata);
    }
    roles.clear();
    topology->channel_manager()->GetReaders(&roles);
    for (auto& attr : roles) {
        std::string msgdata;
        attr.SerializeToString(&msgdata);
        info[attr.channel_name()].emplace_back(msgdata);
    }
    return info;
}

std::vector<std::string> GetNodes(uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::vector<RoleAttributes> nodes;
    topology->node_manager()->GetNodes(&nodes);
    std::sort(nodes.begin(), nodes.end(),
              [](const RoleAttributes& a, const RoleAttributes& b) {
                  return a.node_name() <= b.node_name();
              });
    std::vector<std::string> names;
    for (auto& node : nodes) {
        names.emplace_back(node.node_name());
    }
    return names;
}

std::string GetNodeAttr(const std::string& node_name, uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    if (!topology->node_manager()->HasNode(node_name)) {
        return {};
    }
    std::vector<RoleAttributes> nodes;
    topology->node_manager()->GetNodes(&nodes);
    for (auto& node_attr : nodes) {
        if (node_attr.node_name() == node_name) {
            std::string msgdata;
            node_attr.SerializeToString(&msgdata);
            return msgdata;
        }
    }
    return {};
}

std::vector<std::string> GetReadersOfNode(const std::string& node_name,
                                          uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::vector<std::string> channels;
    if (!topology->node_manager()->HasNode(node_name)) {
        return channels;
    }
    std::vector<RoleAttributes> readers;
    topology->channel_manager()->GetReadersOfNode(node_name, &readers);
    for (auto& reader : readers) {
        if (reader.channel_name() == "param_event") {
            continue;
        }
        channels.emplace_back(reader.channel_name());
    }
    return channels;
}

std::vector<std::string> GetWritersOfNode(const std::string& node_name,
                                          uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::vector<std::string> channels;
    if (!topology->node_manager()->HasNode(node_name)) {
        return channels;
    }
    std::vector<RoleAttributes> writers;
    topology->channel_manager()->GetWritersOfNode(node_name, &writers);
    for (auto& writer : writers) {
        if (writer.channel_name() == "param_event") {
            continue;
        }
        channels.emplace_back(writer.channel_name());
    }
    return channels;
}

std::vector<std::string> GetServices(uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    std::vector<RoleAttributes> services;
    topology->service_manager()->GetServers(&services);
    std::sort(services.begin(), services.end(),
              [](const RoleAttributes& a, const RoleAttributes& b) {
                  return a.service_name() <= b.service_name();
              });
    std::vector<std::string> names;
    for (auto& service : services) {
        names.emplace_back(service.service_name());
    }
    return names;
}

std::string GetServiceAttr(const std::string& service_name, uint8_t sleep_s) {
    SleepSeconds(sleep_s);
    auto* topology = service_discovery::TopologyManager::Instance();
    if (!topology->service_manager()->HasService(service_name)) {
        return {};
    }
    std::vector<RoleAttributes> services;
    topology->service_manager()->GetServers(&services);
    for (auto& service : services) {
        if (service.service_name() == service_name) {
            std::string msgdata;
            service.SerializeToString(&msgdata);
            return msgdata;
        }
    }
    return {};
}

}  // namespace python_support
}  // namespace autolink
