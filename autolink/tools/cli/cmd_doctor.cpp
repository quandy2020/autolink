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

#include "autolink/tools/cli/cmd_doctor.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/autolink.hpp"
#include "autolink/common/log.hpp"
#include "autolink/init.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"

namespace {

const char* EnvOr(const char* name) {
    const char* v = std::getenv(name);
    return (v != nullptr && v[0] != '\0') ? v : "<unset>";
}

}  // namespace

namespace autolink {
namespace tools {

void SetupDoctor(CLI::App& app) {
    app.add_subcommand("doctor", "Check Autolink runtime readiness")
        ->callback([]() {
            std::cout << "=== Environment ===\n";
            std::cout << "AUTOLINK_DOMAIN_ID=" << EnvOr("AUTOLINK_DOMAIN_ID")
                      << '\n';
            std::cout << "AUTOLINK_IP=" << EnvOr("AUTOLINK_IP") << '\n';

            FLAGS_minloglevel = 3;
            FLAGS_alsologtostderr = 0;
            if (!autolink::Init("autolink_doctor")) {
                std::cerr << "FAIL: autolink::Init failed\n";
                throw CLI::RuntimeError(ExitCode::kError);
            }

            WaitForDiscovery();
            auto* topology =
                service_discovery::TopologyManager::Instance();
            std::vector<std::string> channels;
            std::vector<proto::RoleAttributes> node_attrs;
            topology->channel_manager()->GetChannelNames(&channels);
            topology->node_manager()->GetNodes(&node_attrs);

            std::cout << "\n=== Topology (after --wait) ===\n";
            std::cout << "channels=" << channels.size() << '\n';
            std::cout << "nodes=" << node_attrs.size() << '\n';

            std::cout << "\n=== Summary ===\n";
            std::cout << "OK: Autolink initialized (local INTRA/SHM transport)\n";
            std::cout << "Tips: AUTOLINK_IP should be a real NIC when using SHM.\n";

            autolink::Clear();
        });
}

}  // namespace tools
}  // namespace autolink
