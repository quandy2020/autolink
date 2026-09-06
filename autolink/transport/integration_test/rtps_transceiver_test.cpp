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
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "autolink/common/global_data.hpp"
#include "autolink/common/util.hpp"
#include "autolink/init.hpp"
#include "autolink/proto/unit_test.pb.h"
#include "autolink/transport/qos/qos_profile_conf.hpp"
#include "autolink/transport/transport.hpp"
#include "gtest/gtest.h"

namespace autolink {
namespace transport {

#if !AUTOLINK_ENABLE_FASTDDS

TEST(RtpsTransceiverTest, DisabledSkip) {
    GTEST_SKIP() << "RTPS requires -DAUTOLINK_ENABLE_FASTDDS=ON";
}

#else

TEST(RtpsTransceiverTest, PubSub) {
    ASSERT_NE(Transport::Instance()->participant(), nullptr);

    const std::string channel_name = "rtps_transceiver_pubsub";
    RoleAttributes attr;
    attr.set_host_name(common::GlobalData::Instance()->HostName());
    attr.set_host_ip(common::GlobalData::Instance()->HostIp());
    attr.set_process_id(common::GlobalData::Instance()->ProcessId());
    attr.set_channel_name(channel_name);
    attr.set_channel_id(common::Hash(channel_name));
    attr.mutable_qos_profile()->CopyFrom(QosProfileConf::QOS_PROFILE_DEFAULT);

    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<int> received{0};
    proto::UnitTest got;

    auto receiver = Transport::Instance()->CreateReceiver<proto::UnitTest>(
            attr,
            [&](const std::shared_ptr<proto::UnitTest>& msg,
                const MessageInfo& msg_info, const RoleAttributes& role) {
                (void)msg_info;
                (void)role;
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    got = *msg;
                    received.fetch_add(1);
                }
                cv.notify_one();
            },
            OptionalMode::RTPS);
    ASSERT_NE(receiver, nullptr);

    auto transmitter = Transport::Instance()->CreateTransmitter<proto::UnitTest>(
            attr, OptionalMode::RTPS);
    ASSERT_NE(transmitter, nullptr);

    auto msg = std::make_shared<proto::UnitTest>();
    msg->set_class_name("RtpsTransceiverTest");
    msg->set_case_name("PubSub");

    // SIMPLE discovery + same-process matching can take >500ms; retry until
    // callback or ~2s wall budget (plan expectation).
    const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
    bool transmitted_ok = false;
    while (std::chrono::steady_clock::now() < deadline) {
        transmitted_ok = transmitter->Transmit(msg) || transmitted_ok;
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (cv.wait_for(lock, std::chrono::milliseconds(100),
                            [&] { return received.load() >= 1; })) {
                break;
            }
        }
    }
    EXPECT_TRUE(transmitted_ok);
    // Dual-host checklist in §14 remains the primary production validation
    // path; this same-process PubSub still must observe the receive callback.
    ASSERT_GE(received.load(), 1)
            << "RTPS Transmit succeeded but same-process callback not observed "
               "within 2s";
    EXPECT_EQ(got.class_name(), "RtpsTransceiverTest");
    EXPECT_EQ(got.case_name(), "PubSub");

    transmitter->Disable();
    receiver->Disable();
}

#endif  // AUTOLINK_ENABLE_FASTDDS

}  // namespace transport
}  // namespace autolink

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    autolink::Init(argv[0]);
    autolink::transport::Transport::Instance();
    auto res = RUN_ALL_TESTS();
    autolink::transport::Transport::Instance()->Shutdown();
    return res;
}
