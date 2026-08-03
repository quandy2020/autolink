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

#include "autolink/tools/cli/cmd_node.hpp"

#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/init.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/specific_manager/channel_manager.hpp"
#include "autolink/service_discovery/specific_manager/node_manager.hpp"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/state.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"

namespace {

void InstallShutdownHandlers() {
    std::signal(SIGINT, [](int sig) { autolink::OnShutdown(sig); });
    std::signal(SIGTERM, [](int sig) { autolink::OnShutdown(sig); });
}

std::vector<std::string> GetNodes(uint8_t sleep_s = 2) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    std::vector<autolink::proto::RoleAttributes> nodes;
    topology->node_manager()->GetNodes(&nodes);
    std::vector<std::string> node_names;
    for (const auto& n : nodes) {
        node_names.push_back(n.node_name());
    }
    std::sort(node_names.begin(), node_names.end());
    return node_names;
}

bool GetNodeAttr(const std::string& node_name, uint8_t sleep_s,
                 autolink::proto::RoleAttributes* out) {
    if (!out)
        return false;
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    if (!topology->node_manager()->HasNode(node_name)) {
        std::cerr << "no node named: " << node_name << std::endl;
        return false;
    }
    std::vector<autolink::proto::RoleAttributes> nodes;
    topology->node_manager()->GetNodes(&nodes);
    for (const auto& attr : nodes) {
        if (attr.node_name() == node_name) {
            out->CopyFrom(attr);
            return true;
        }
    }
    return false;
}

std::vector<std::string> GetReadersOfNode(const std::string& node_name,
                                          uint8_t sleep_s = 0) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    std::vector<std::string> channels;
    if (!topology->node_manager()->HasNode(node_name)) {
        std::cerr << "no node named: " << node_name << std::endl;
        return channels;
    }
    std::vector<autolink::proto::RoleAttributes> readers;
    topology->channel_manager()->GetReadersOfNode(node_name, &readers);
    for (const auto& r : readers) {
        if (r.channel_name() != "param_event") {
            channels.push_back(r.channel_name());
        }
    }
    std::sort(channels.begin(), channels.end());
    return channels;
}

std::vector<std::string> GetWritersOfNode(const std::string& node_name,
                                          uint8_t sleep_s = 0) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    std::vector<std::string> channels;
    if (!topology->node_manager()->HasNode(node_name)) {
        std::cerr << "no node named: " << node_name << std::endl;
        return channels;
    }
    std::vector<autolink::proto::RoleAttributes> writers;
    topology->channel_manager()->GetWritersOfNode(node_name, &writers);
    for (const auto& w : writers) {
        if (w.channel_name() != "param_event") {
            channels.push_back(w.channel_name());
        }
    }
    std::sort(channels.begin(), channels.end());
    return channels;
}

void PrintNodeInfo(const std::string& node_name, uint8_t sleep_s = 2) {
    autolink::proto::RoleAttributes attr;
    if (!GetNodeAttr(node_name, sleep_s, &attr)) {
        return;
    }
    if (attr.node_name() != node_name) {
        std::cerr << "RoleAttributes node_name mismatch" << std::endl;
        return;
    }
    std::cout << "Node:    \t" << attr.node_name() << std::endl;
    std::cout << "ProcessId: \t" << attr.process_id() << std::endl;
    std::cout << "Hostname:\t" << attr.host_name() << std::endl;
    std::cout << std::endl;

    std::cout << "[Reading Channels]:" << std::endl;
    std::vector<std::string> reading = GetReadersOfNode(node_name, 0);
    for (const auto& ch : reading) {
        std::cout << ch << std::endl;
    }
    std::cout << std::endl;

    std::cout << "[Writing Channels]:" << std::endl;
    std::vector<std::string> writing = GetWritersOfNode(node_name, 0);
    for (const auto& ch : writing) {
        std::cout << ch << std::endl;
    }
    std::cout << std::endl;
}

void CmdList() {
    std::vector<std::string> nodes = GetNodes(2);
    std::cout << "Number of active nodes: " << nodes.size() << std::endl;
    for (const auto& name : nodes) {
        std::cout << name << std::endl;
    }
}

void CmdInfo(bool all_nodes, const std::vector<std::string>& node_names) {
    if (all_nodes) {
        std::vector<std::string> nodes = GetNodes(2);
        for (const auto& nodename : nodes) {
            PrintNodeInfo(nodename, 0);
        }
    } else {
        for (const auto& name : node_names) {
            PrintNodeInfo(name, 2);
        }
    }
}

}  // namespace

namespace autolink {
namespace tools {

void SetupNode(CLI::App& app) {
    auto* node = app.add_subcommand("node", "Introspect Autolink nodes");
    node->require_subcommand(1);

    node->add_subcommand("list", "List active nodes")->callback([]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        CmdList();
        autolink::Clear();
    });

    auto* info = node->add_subcommand("info", "Print node info");
    auto all = std::make_shared<bool>(false);
    auto names = std::make_shared<std::vector<std::string>>();
    info->add_flag("-a,--all", *all, "Show all nodes");
    info->add_option("nodes", *names, "Node name(s)");
    info->callback([all, names]() {
        if (*all && !names->empty()) {
            throw CLI::ValidationError(
                "info",
                "\"-a/--all\" option is expected to run w/o node name(s)");
        }
        if (!*all && names->empty()) {
            throw CLI::ValidationError("info", "No node name provided.");
        }
        InstallShutdownHandlers();
        autolink::Init("autolink");
        CmdInfo(*all, *names);
        autolink::Clear();
    });
}

}  // namespace tools
}  // namespace autolink
