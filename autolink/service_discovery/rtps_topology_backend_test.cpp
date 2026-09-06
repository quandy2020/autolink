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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>

#include "autolink/init.hpp"
#include "autolink/proto/topology_change.pb.h"
#include "gtest/gtest.h"

#if AUTOLINK_ENABLE_FASTDDS
#include "autolink/service_discovery/rtps_topology_backend.hpp"
#include "autolink/transport/rtps/participant_hub.hpp"
#endif

namespace autolink {
namespace service_discovery {

#if !AUTOLINK_ENABLE_FASTDDS

TEST(RtpsTopologyBackendTest, DisabledSkip) {
    GTEST_SKIP() << "RtpsTopologyBackend requires -DAUTOLINK_ENABLE_FASTDDS=ON";
}

#else

TEST(RtpsTopologyBackendTest, PublishSubscribeChangeMsg) {
    RtpsTopologyBackend backend;
    ASSERT_TRUE(backend.Start());

    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<int> received{0};
    proto::ChangeMsg got;

    const int64_t sub_id = backend.Subscribe(
            proto::ChangeType::CHANGE_NODE,
            [&](const proto::ChangeMsg& msg) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    got = msg;
                    received.fetch_add(1);
                }
                cv.notify_one();
            });
    ASSERT_GT(sub_id, 0);

    // IsFromSelf filters host_name+process_id of this process; use a synthetic
    // remote role so the loopback sample is delivered to the callback.
    proto::ChangeMsg join;
    join.set_timestamp(1);
    join.set_change_type(proto::ChangeType::CHANGE_NODE);
    join.set_operate_type(proto::OperateType::OPT_JOIN);
    join.set_role_type(proto::RoleType::ROLE_NODE);
    auto* attr = join.mutable_role_attr();
    attr->set_host_name("remote-host-smoke");
    attr->set_host_ip("10.0.0.2");
    attr->set_process_id(424242);
    attr->set_node_name("rtps_topo_smoke_remote");
    attr->set_node_id(9001);

    // SIMPLE discovery + same-process matching can take >500ms; retry until
    // callback or ~2s wall budget (same pattern as rtps_transceiver_test).
    const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
    bool published_ok = false;
    while (std::chrono::steady_clock::now() < deadline) {
        published_ok = backend.Publish(join) || published_ok;
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (cv.wait_for(lock, std::chrono::milliseconds(100),
                            [&] { return received.load() >= 1; })) {
                break;
            }
        }
    }

    EXPECT_TRUE(published_ok);
    ASSERT_GE(received.load(), 1)
            << "Publish succeeded but ChangeMsg callback not observed within "
               "2s";
    EXPECT_EQ(got.operate_type(), proto::OperateType::OPT_JOIN);
    EXPECT_EQ(got.change_type(), proto::ChangeType::CHANGE_NODE);
    EXPECT_EQ(got.role_attr().node_name(), "rtps_topo_smoke_remote");
    EXPECT_EQ(got.role_attr().host_name(), "remote-host-smoke");

    backend.Unsubscribe(sub_id);
    backend.Shutdown();
}

#endif  // AUTOLINK_ENABLE_FASTDDS

}  // namespace service_discovery
}  // namespace autolink

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    autolink::Init(argv[0]);
    const int res = RUN_ALL_TESTS();
#if AUTOLINK_ENABLE_FASTDDS
    // Fast DDS DomainParticipant / factory teardown can race with process
    // static destructors (mutex lock on destroyed mutex). Shut Hub down then
    // exit without running remaining static destructors.
    autolink::transport::RtpsParticipantHub::Instance().Shutdown();
    std::_Exit(res);
#endif
    return res;
}
