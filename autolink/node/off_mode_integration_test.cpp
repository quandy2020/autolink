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

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#include "autolink/autolink.hpp"
#include "autolink/init.hpp"
#include "autolink/proto/unit_test.pb.h"
#include "autolink/service_discovery/topology_manager.hpp"
#include "gtest/gtest.h"

namespace autolink {

TEST(OffModeIntegrationTest, TalkerListenerAndChannelList) {
    std::atomic<int> received_cnt{0};
    const std::string token = "off_mode_token_" + std::to_string(getpid());
    const std::string uniq_suffix =
        std::to_string(getpid()) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string channel =
        "/autolink/off_mode_integration/" + std::to_string(getpid());

    auto talker_node = CreateNode("off_mode_talker_" + uniq_suffix);
    auto listener_node = CreateNode("off_mode_listener_" + uniq_suffix);
    ASSERT_NE(talker_node, nullptr);
    ASSERT_NE(listener_node, nullptr);

    auto reader = listener_node->CreateReader<proto::UnitTest>(
        channel, [&received_cnt, &token](const std::shared_ptr<proto::UnitTest>& msg) {
            if (msg != nullptr && msg->case_name() == token) {
                ++received_cnt;
            }
        });
    auto writer = talker_node->CreateWriter<proto::UnitTest>(channel);
    ASSERT_NE(reader, nullptr);
    ASSERT_NE(writer, nullptr);

    auto msg = std::make_shared<proto::UnitTest>();
    msg->set_class_name("OffModeIntegrationTest");
    msg->set_case_name(token);

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(3) &&
           received_cnt.load() == 0) {
        writer->Write(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_GT(received_cnt.load(), 0);

    auto* topology = service_discovery::TopologyManager::Instance();
    ASSERT_NE(topology, nullptr);
    bool channel_found = false;
    const auto list_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - list_start <
               std::chrono::seconds(3) &&
           !channel_found) {
        std::vector<std::string> channels;
        topology->channel_manager()->GetChannelNames(&channels);
        channel_found =
            std::find(channels.begin(), channels.end(), channel) != channels.end();
        if (!channel_found) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    EXPECT_TRUE(channel_found);

    namespace fs = std::filesystem;
    const fs::path self_exe = fs::read_symlink("/proc/self/exe");
    const fs::path channel_tool = self_exe.parent_path() / "autolink_channel";
    if (access(channel_tool.c_str(), X_OK) != 0) {
        GTEST_SKIP() << "autolink_channel tool not found: " << channel_tool;
    }

    bool cli_found = false;
    const auto cli_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - cli_start < std::chrono::seconds(8) &&
           !cli_found) {
        const std::string cmd = channel_tool.string() + " list 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        ASSERT_NE(pipe, nullptr);
        std::string output;
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe) != nullptr) {
            output += buf;
        }
        (void)pclose(pipe);
        cli_found = output.find(channel) != std::string::npos;
        if (!cli_found) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    EXPECT_TRUE(cli_found);
}

}  // namespace autolink

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    autolink::Init(argv[0]);
    return RUN_ALL_TESTS();
}
