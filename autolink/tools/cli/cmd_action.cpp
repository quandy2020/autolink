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

#include "autolink/tools/cli/cmd_action.hpp"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/autolink.hpp"
#include "autolink/init.hpp"
#include "autolink/proto/action.pb.h"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/specific_manager/service_manager.hpp"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/state.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"
#include "autolink/tools/cli/proto_json.hpp"

namespace {

const std::string kSendGoalSuffix = "/send_goal";

void InstallShutdownHandlers() {
    std::signal(SIGINT, [](int sig) { autolink::OnShutdown(sig); });
    std::signal(SIGTERM, [](int sig) { autolink::OnShutdown(sig); });
}

std::vector<std::string> GetActionNames(uint8_t sleep_s = 2) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    std::vector<autolink::proto::RoleAttributes> servers;
    topology->service_manager()->GetServers(&servers);
    std::unordered_set<std::string> names_set;
    for (const auto& s : servers) {
        const std::string& svc = s.service_name();
        if (svc.size() > kSendGoalSuffix.size()) {
            size_t pos = svc.size() - kSendGoalSuffix.size();
            if (svc.compare(pos, kSendGoalSuffix.size(), kSendGoalSuffix) ==
                0) {
                names_set.insert(svc.substr(0, pos));
            }
        }
    }
    std::vector<std::string> names(names_set.begin(), names_set.end());
    std::sort(names.begin(), names.end());
    return names;
}

void PrintServerRole(const autolink::proto::RoleAttributes& attr) {
    std::cout << "\tprocessid\t" << attr.process_id() << std::endl;
    std::cout << "\tnodename\t" << attr.node_name() << std::endl;
    std::cout << "\thostname\t" << attr.host_name() << std::endl;
}

void CmdList() {
    std::vector<std::string> actions = GetActionNames(2);
    std::cout << "The number of actions is: " << actions.size() << std::endl;
    for (const auto& name : actions) {
        std::cout << name << std::endl;
    }
}

void CmdInfo(const std::string& action_name) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscovery();
    std::string send_goal_service = action_name + kSendGoalSuffix;
    if (!topology->service_manager()->HasService(send_goal_service)) {
        std::cerr << "Action '" << action_name
                  << "' has no server (no service '" << send_goal_service
                  << "')." << std::endl;
        return;
    }
    std::vector<autolink::proto::RoleAttributes> servers;
    topology->service_manager()->GetServers(&servers);
    std::cout << "Action: " << action_name << std::endl;
    bool found = false;
    for (const auto& attr : servers) {
        if (attr.service_name() == send_goal_service) {
            found = true;
            PrintServerRole(attr);
        }
    }
    if (!found) {
        std::cout << "\t(no server role attributes)" << std::endl;
    }
    std::cout << std::endl;
}

void CmdSendGoalUsage() {
    std::cout
        << "usage: autolink action send_goal <action_name> [json] [--type T]\n\n"
        << "  Send a goal to an action server. JSON is mapped to the goal\n"
        << "  protobuf type (topology or --type).\n";
}

void CmdSendGoal(const std::string& action_name, const std::string& json,
                 const std::string& type_flag,
                 const std::string& descriptor_set) {
    std::string err;
    if (!descriptor_set.empty()) {
        if (!autolink::tools::RegisterDescriptorSetFile(descriptor_set,
                                                        &err)) {
            std::cerr << err << std::endl;
            throw CLI::RuntimeError(1);
        }
    }
    std::string goal_type = type_flag;
    std::string desc;
    if (goal_type.empty()) {
        std::string inferred;
        if (autolink::tools::ResolveServiceType(action_name + "/send_goal",
                                                &inferred, &desc) &&
            !inferred.empty() &&
            inferred.find("SendGoal") == std::string::npos) {
            goal_type = inferred;
        }
    } else {
        autolink::tools::ResolveServiceType(action_name + "/send_goal", nullptr,
                                            &desc);
    }
    if (goal_type.empty()) {
        std::cerr << "goal --type required (could not infer goal message type)"
                  << std::endl;
        CmdSendGoalUsage();
        throw CLI::RuntimeError(1);
    }
    if (!autolink::tools::RegisterProtoType(goal_type, desc)) {
        std::cerr << "failed to register type: " << goal_type
                  << " (try --descriptor-set from protoc)" << std::endl;
        throw CLI::RuntimeError(1);
    }
    const std::string payload = json.empty() ? "{}" : json;
    std::string goal_bytes;
    if (!autolink::tools::JsonToProtobufBytes(goal_type, payload, &goal_bytes,
                                              &err)) {
        std::cerr << err << std::endl;
        throw CLI::RuntimeError(1);
    }

    autolink::proto::SendGoalRequest req;
    std::array<char, 16> uuid{};
    std::random_device rd;
    for (auto& b : uuid) {
        b = static_cast<char>(rd());
    }
    req.mutable_goal_id()->set_uuid(std::string(uuid.data(), uuid.size()));
    req.set_goal(goal_bytes);

    auto node = autolink::CreateNode("autolink_cli_action");
    if (!node) {
        throw CLI::RuntimeError(1);
    }
    auto client = node->template CreateClient<
        autolink::proto::SendGoalRequest, autolink::proto::SendGoalResponse>(
        action_name + "/send_goal");
    if (!client) {
        std::cerr << "CreateClient failed" << std::endl;
        throw CLI::RuntimeError(1);
    }
    for (int i = 0; i < 20 && !client->ServiceIsReady(); ++i) {
        sleep(1);
    }
    auto req_ptr = std::make_shared<autolink::proto::SendGoalRequest>(req);
    auto resp = client->SendRequest(req_ptr, std::chrono::seconds(5));
    if (!resp) {
        std::cerr << "SendRequest failed or timed out" << std::endl;
        throw CLI::RuntimeError(1);
    }
    std::cout << "Action: " << action_name << std::endl;
    std::cout << "accepted: " << (resp->accepted() ? "true" : "false")
              << std::endl;
    std::cout << "stamp_sec: " << resp->stamp_sec() << std::endl;
    std::cout << "stamp_nanosec: " << resp->stamp_nanosec() << std::endl;
}

}  // namespace

namespace autolink {
namespace tools {

void SetupAction(CLI::App& app) {
    auto* action =
        app.add_subcommand("action", "Introspect and interact with actions");
    action->require_subcommand(1);

    action->add_subcommand("list", "List active actions")->callback([]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        CmdList();
        autolink::Clear();
    });

    auto* info = action->add_subcommand("info", "Print action info");
    auto name = std::make_shared<std::string>();
    info->add_option("action_name", *name, "Action name")->required();
    info->callback([name]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        CmdInfo(*name);
        autolink::Clear();
    });

    auto* sg = action->add_subcommand("send_goal", "Send an action goal");
    auto sg_name = std::make_shared<std::string>();
    auto goal = std::make_shared<std::string>();
    auto goal_type = std::make_shared<std::string>();
    auto fdset = std::make_shared<std::string>();
    sg->add_option("action_name", *sg_name, "Action name")->required();
    sg->add_option("json", *goal, "Goal JSON (default {})");
    sg->add_option("--type", *goal_type, "Goal protobuf type");
    sg->add_option("--descriptor-set", *fdset,
                   "FileDescriptorSet from protoc --descriptor_set_out");
    sg->callback([sg_name, goal, goal_type, fdset]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        try {
            CmdSendGoal(*sg_name, *goal, *goal_type, *fdset);
        } catch (const CLI::RuntimeError&) {
            autolink::Clear();
            throw;
        }
        autolink::Clear();
    });
}

}  // namespace tools
}  // namespace autolink
