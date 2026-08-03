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

#include "autolink/tools/cli/cmd_param.hpp"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/autolink.hpp"
#include "autolink/init.hpp"
#include "autolink/parameter/parameter.hpp"
#include "autolink/parameter/parameter_client.hpp"
#include "autolink/state.hpp"

namespace {

void InstallShutdownHandlers() {
    std::signal(SIGINT, [](int sig) { autolink::OnShutdown(sig); });
    std::signal(SIGTERM, [](int sig) { autolink::OnShutdown(sig); });
}

autolink::Parameter ParseParamValue(const std::string& name,
                                    const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "true") {
        return autolink::Parameter(name, true);
    }
    if (lower == "false") {
        return autolink::Parameter(name, false);
    }
    if (value.find_first_of(".eE") == std::string::npos) {
        try {
            size_t idx = 0;
            long long v = std::stoll(value, &idx);
            if (idx == value.size()) {
                return autolink::Parameter(name, static_cast<int64_t>(v));
            }
        } catch (...) {
        }
    }
    try {
        size_t idx = 0;
        double v = std::stod(value, &idx);
        if (idx == value.size()) {
            return autolink::Parameter(name, v);
        }
    } catch (...) {
    }
    return autolink::Parameter(name, value);
}

}  // namespace

namespace autolink {
namespace tools {

void SetupParam(CLI::App& app) {
    auto* param = app.add_subcommand("param", "Get/set/list node parameters");
    param->require_subcommand(1);

    auto* list = param->add_subcommand("list", "List parameters of a node");
    auto list_node = std::make_shared<std::string>();
    list->add_option("node", *list_node, "Parameter server node name")
        ->required();
    list->callback([list_node]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        auto node = CreateNode("autolink_cli_param");
        if (!node) {
            throw CLI::RuntimeError(1);
        }
        ParameterClient client(node, *list_node);
        std::vector<Parameter> params;
        if (!client.ListParameters(&params)) {
            std::cerr << "ListParameters failed for node: " << *list_node
                      << std::endl;
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }
        std::cout << "parameters (" << params.size() << "):" << std::endl;
        for (const auto& p : params) {
            std::cout << p.DebugString() << std::endl;
        }
        autolink::Clear();
    });

    auto* get = param->add_subcommand("get", "Get one parameter");
    auto get_node = std::make_shared<std::string>();
    auto get_name = std::make_shared<std::string>();
    get->add_option("node", *get_node, "Parameter server node name")->required();
    get->add_option("name", *get_name, "Parameter name")->required();
    get->callback([get_node, get_name]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        auto node = CreateNode("autolink_cli_param");
        if (!node) {
            throw CLI::RuntimeError(1);
        }
        ParameterClient client(node, *get_node);
        Parameter parameter;
        if (!client.GetParameter(*get_name, &parameter)) {
            std::cerr << "GetParameter failed: " << *get_name << std::endl;
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }
        std::cout << parameter.DebugString() << std::endl;
        autolink::Clear();
    });

    auto* set = param->add_subcommand("set", "Set one parameter");
    auto set_node = std::make_shared<std::string>();
    auto set_name = std::make_shared<std::string>();
    auto set_value = std::make_shared<std::string>();
    set->add_option("node", *set_node, "Parameter server node name")->required();
    set->add_option("name", *set_name, "Parameter name")->required();
    set->add_option("value", *set_value, "Parameter value")->required();
    set->callback([set_node, set_name, set_value]() {
        InstallShutdownHandlers();
        autolink::Init("autolink");
        auto node = CreateNode("autolink_cli_param");
        if (!node) {
            throw CLI::RuntimeError(1);
        }
        ParameterClient client(node, *set_node);
        Parameter parameter = ParseParamValue(*set_name, *set_value);
        if (!client.SetParameter(parameter)) {
            std::cerr << "SetParameter failed: " << *set_name << std::endl;
            autolink::Clear();
            throw CLI::RuntimeError(1);
        }
        std::cout << "set OK" << std::endl;
        autolink::Clear();
    });
}

}  // namespace tools
}  // namespace autolink
