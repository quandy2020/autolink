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

#include <CLI/CLI.hpp>

#include "autolink/tools/cli/cmd_action.hpp"
#include "autolink/tools/cli/cmd_channel.hpp"
#include "autolink/tools/cli/cmd_completion.hpp"
#include "autolink/tools/cli/cmd_doctor.hpp"
#include "autolink/tools/cli/cmd_launch.hpp"
#include "autolink/tools/cli/cmd_monitor.hpp"
#include "autolink/tools/cli/cmd_node.hpp"
#include "autolink/tools/cli/cmd_param.hpp"
#include "autolink/tools/cli/cmd_recorder.hpp"
#include "autolink/tools/cli/cmd_service.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"

int main(int argc, char** argv) {
    CLI::App app{"Autolink command line"};
    app.require_subcommand(1);
    app.add_option("--wait", autolink::tools::DiscoveryWaitSeconds(),
                   "Seconds to wait for topology discovery")
        ->capture_default_str();

    autolink::tools::SetupChannel(app);
    autolink::tools::SetupNode(app);
    autolink::tools::SetupService(app);
    autolink::tools::SetupAction(app);
    autolink::tools::SetupParam(app);
    autolink::tools::SetupRecorder(app);
    autolink::tools::SetupLaunch(app);
    autolink::tools::SetupMonitor(app);
    autolink::tools::SetupDoctor(app);
    autolink::tools::SetupCompletion(app);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
