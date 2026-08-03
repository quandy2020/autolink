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

#include "autolink/tools/cli/cmd_monitor.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/init.hpp"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/tools/monitor/autolink_topology_message.hpp"
#include "autolink/tools/monitor/general_channel_message.hpp"
#include "autolink/tools/monitor/screen.hpp"

namespace {

void SigResizeHandle(int) {
    Screen::Instance()->Resize();
}
void SigCtrlCHandle(int) {
    Screen::Instance()->Stop();
}

void RunMonitor(const std::string& channel_filter) {
    autolink::Init("autolink");
    FLAGS_minloglevel = 3;
    FLAGS_alsologtostderr = 0;
    FLAGS_colorlogtostderr = 0;

    CyberTopologyMessage topology_msg(channel_filter);

    auto topology_callback =
        [&topology_msg](const autolink::proto::ChangeMsg& change_msg) {
            topology_msg.TopologyChanged(change_msg);
        };

    auto channel_manager =
        autolink::service_discovery::TopologyManager::Instance()
            ->channel_manager();
    channel_manager->AddChangeListener(topology_callback);

    std::vector<autolink::proto::RoleAttributes> role_vec;
    channel_manager->GetWriters(&role_vec);
    for (auto& role : role_vec) {
        topology_msg.AddReaderWriter(role, true);
    }

    role_vec.clear();
    channel_manager->GetReaders(&role_vec);
    for (auto& role : role_vec) {
        topology_msg.AddReaderWriter(role, false);
    }

    Screen* s = Screen::Instance();

    signal(SIGWINCH, SigResizeHandle);
    signal(SIGINT, SigCtrlCHandle);

    s->SetCurrentRenderMessage(&topology_msg);

    s->Init();
    s->Run();
}

}  // namespace

namespace autolink {
namespace tools {

void SetupMonitor(CLI::App& app) {
    auto* mon = app.add_subcommand("monitor", "Interactive topology monitor");
    auto channel = std::make_shared<std::string>();
    mon->add_option("-c,--channel", *channel, "Monitor only this channel");
    mon->callback([channel]() { RunMonitor(*channel); });
}

}  // namespace tools
}  // namespace autolink
