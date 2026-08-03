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

#include "autolink/tools/cli/cmd_service.hpp"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/autolink.hpp"
#include "autolink/init.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/specific_manager/service_manager.hpp"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/state.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"
#include "autolink/tools/cli/proto_json.hpp"

namespace {

void InstallShutdownHandlers() {
    std::signal(SIGINT, [](int sig) { autolink::OnShutdown(sig); });
    std::signal(SIGTERM, [](int sig) { autolink::OnShutdown(sig); });
}

std::vector<std::string> GetServices(uint8_t sleep_s = 2) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    std::vector<autolink::proto::RoleAttributes> servers;
    topology->service_manager()->GetServers(&servers);
    std::vector<std::string> names;
    for (const auto& s : servers) {
        names.push_back(s.service_name());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool GetServiceAttr(const std::string& service_name, uint8_t sleep_s,
                    autolink::proto::RoleAttributes* out) {
    if (!out)
        return false;
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscoveryIf(sleep_s);
    if (!topology->service_manager()->HasService(service_name)) {
        std::cerr << "no service: " << service_name << std::endl;
        return false;
    }
    std::vector<autolink::proto::RoleAttributes> servers;
    topology->service_manager()->GetServers(&servers);
    for (const auto& attr : servers) {
        if (attr.service_name() == service_name) {
            out->CopyFrom(attr);
            return true;
        }
    }
    return false;
}

void PrintServiceInfo(const std::string& service_name, uint8_t sleep_s = 2) {
    autolink::proto::RoleAttributes attr;
    if (!GetServiceAttr(service_name, sleep_s, &attr)) {
        return;
    }
    if (attr.service_name() != service_name) {
        std::cerr << "RoleAttributes service_name mismatch" << std::endl;
        return;
    }
    std::cout << attr.service_name() << std::endl;
    std::cout << "\tprocessid\t" << attr.process_id() << std::endl;
    std::cout << "\tnodename\t" << attr.node_name() << std::endl;
    std::cout << "\thostname\t" << attr.host_name() << std::endl;
    std::cout << std::endl;
}

void CmdList() {
    std::vector<std::string> services = GetServices(2);
    std::cout << "The number of services is: " << services.size() << std::endl;
    for (const auto& name : services) {
        std::cout << name << std::endl;
    }
}

void CmdInfo(bool all_services, const std::vector<std::string>& service_names) {
    if (all_services) {
        std::vector<std::string> services = GetServices(2);
        for (const auto& name : services) {
            PrintServiceInfo(name, 0);
        }
    } else {
        for (const auto& name : service_names) {
            PrintServiceInfo(name, 2);
        }
    }
}

}  // namespace

namespace autolink {
namespace tools {

void SetupService(CLI::App& app) {
    auto* service =
        app.add_subcommand("service", "Introspect Autolink services");
    service->require_subcommand(1);

    service->add_subcommand("list", "List active services")->callback([]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        CmdList();
        autolink::Clear();
    });

    auto* info = service->add_subcommand("info", "Print service info");
    auto all = std::make_shared<bool>(false);
    auto names = std::make_shared<std::vector<std::string>>();
    info->add_flag("-a,--all", *all, "Show all services");
    info->add_option("services", *names, "Service name(s)");
    info->callback([all, names]() {
        if (*all && !names->empty()) {
            throw CLI::ValidationError(
                "info",
                "\"-a/--all\" option is expected to run w/o service name(s)");
        }
        if (!*all && names->empty()) {
            throw CLI::ValidationError("info",
                                       "servicename must be specified");
        }
        if (!*all && names->size() > 1) {
            throw CLI::ValidationError(
                "info", "you may only specify one service name");
        }
        InstallShutdownHandlers();
        autolink::Init("autolink");
        CmdInfo(*all, *names);
        autolink::Clear();
    });

    auto* call =
        service->add_subcommand("call", "Call a service with JSON request");
    auto svc = std::make_shared<std::string>();
    auto json = std::make_shared<std::string>("{}");
    auto type = std::make_shared<std::string>();
    auto fdset = std::make_shared<std::string>();
    auto timeout = std::make_shared<int>(5);
    call->add_option("service", *svc, "Service name")->required();
    call->add_option("json", *json, "Request JSON (default {})");
    call->add_option("--type", *type, "Request protobuf type");
    call->add_option("--descriptor-set", *fdset,
                     "FileDescriptorSet from protoc --descriptor_set_out");
    call->add_option("--timeout", *timeout, "Timeout seconds");
    call->callback([svc, json, type, fdset, timeout]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");

        std::string err;
        if (!fdset->empty()) {
            if (!RegisterDescriptorSetFile(*fdset, &err)) {
                std::cerr << err << std::endl;
                autolink::Clear();
                throw CLI::RuntimeError(1);
            }
        }

        std::string req_type;
        std::string desc;
        if (!type->empty()) {
            req_type = *type;
            ResolveServiceType(*svc, nullptr, &desc);
            if (!RegisterProtoType(req_type, desc)) {
                std::cerr << "failed to register type: " << req_type
                          << std::endl;
                autolink::Clear();
                throw CLI::RuntimeError(1);
            }
        } else {
            if (!ResolveServiceType(*svc, &req_type, &desc) ||
                req_type.empty()) {
                std::cerr << "cannot resolve type; pass --type" << std::endl;
                autolink::Clear();
                throw CLI::RuntimeError(1);
            }
            if (!RegisterProtoType(req_type, desc)) {
                std::cerr << "failed to register type: " << req_type
                          << std::endl;
                autolink::Clear();
                throw CLI::RuntimeError(1);
            }
        }

        std::string req_bytes;
        if (!JsonToProtobufBytes(req_type, *json, &req_bytes, &err)) {
            std::cerr << err << std::endl;
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }

        auto node = CreateNode("autolink_cli_service_call");
        if (!node) {
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }
        auto client =
            node->CreateClient<message::RawMessage, message::RawMessage>(*svc);
        if (!client) {
            std::cerr << "CreateClient failed" << std::endl;
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }
        for (int i = 0; i < 20 && !client->ServiceIsReady(); ++i) {
            sleep(1);
        }
        auto req = std::make_shared<message::RawMessage>(req_bytes);
        auto resp = client->SendRequest(
            req, std::chrono::seconds(std::max(1, *timeout)));
        if (!resp) {
            std::cerr << "SendRequest failed or timed out" << std::endl;
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }
        std::string out;
        if (ProtobufBytesToJson(req_type, resp->message, &out, &err) ||
            ProtobufBytesToDebug(req_type, resp->message, &out, &err)) {
            std::cout << out << std::endl;
        } else {
            std::cout << "response bytes: " << resp->message.size() << std::endl;
        }
        autolink::Clear();
    });
}

}  // namespace tools
}  // namespace autolink
