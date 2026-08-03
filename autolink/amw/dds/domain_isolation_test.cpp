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
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include "autolink/amw/dds/host_ip_utils.hpp"
#include "gtest/gtest.h"

#ifdef AUTOLINK_ENABLE_CYCLONEDDS
#include "autolink/amw/dds/cyclonedds_provider.hpp"
#include "autolink/amw/types.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/role_attributes.pb.h"
#endif

namespace autolink {
namespace amw {
namespace {

TEST(HostIpUtilsTest, FakeSimIpIsNotLocalInterface) {
    EXPECT_FALSE(HostIpIsLocalInterface("10.255.0.1"));
    EXPECT_FALSE(HostIpIsLocalInterface("10.255.0.2"));
    EXPECT_FALSE(HostIpIsLocalInterface(""));
}

#ifdef AUTOLINK_ENABLE_CYCLONEDDS

class EnvGuard {
public:
    EnvGuard(const char* key, const std::string& value) : key_(key) {
        const char* prev = std::getenv(key);
        if (prev != nullptr) {
            had_prev_ = true;
            prev_ = prev;
        }
        ::setenv(key, value.c_str(), 1);
    }
    ~EnvGuard() {
        if (had_prev_) {
            ::setenv(key_, prev_.c_str(), 1);
        } else {
            ::unsetenv(key_);
        }
    }

private:
    const char* key_;
    bool had_prev_ = false;
    std::string prev_;
};

EndpointDesc MakeEndpoint(const std::string& channel) {
    EndpointDesc endpoint;
    endpoint.attr.set_channel_name(channel);
    endpoint.attr.mutable_qos_profile()->set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    endpoint.attr.mutable_qos_profile()->set_history(
        proto::QosHistoryPolicy::HISTORY_KEEP_LAST);
    endpoint.attr.mutable_qos_profile()->set_depth(10);
    endpoint.attr.mutable_qos_profile()->set_durability(
        proto::QosDurabilityPolicy::DURABILITY_VOLATILE);
    return endpoint;
}

bool WaitReceived(std::atomic<int>* count, int expect,
                  std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (count->load() >= expect) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return count->load() >= expect;
}

TEST(DomainIsolationTest, DifferentDomainDoesNotDeliver) {
    const std::string channel = "/autolink/test/domain_isolation";
    const std::string payload = "domain-isolation-payload";

    std::shared_ptr<ITransportProvider> pub_transport;
    std::shared_ptr<IPublisher> publisher;
    {
        EnvGuard domain("AUTOLINK_DOMAIN_ID", "211");
        pub_transport = CreateCycloneDdsTransportProvider();
        ASSERT_NE(pub_transport, nullptr);
        ASSERT_TRUE(pub_transport->Init(AmwContext{}));
        ASSERT_FALSE(pub_transport->IsStub());
        publisher = pub_transport->CreatePublisher(MakeEndpoint(channel));
        ASSERT_NE(publisher, nullptr);
        publisher->Enable();
    }

    std::atomic<int> received{0};
    std::shared_ptr<ITransportProvider> sub_transport;
    std::shared_ptr<ISubscription> subscription;
    {
        EnvGuard domain("AUTOLINK_DOMAIN_ID", "212");
        sub_transport = CreateCycloneDdsTransportProvider();
        ASSERT_NE(sub_transport, nullptr);
        ASSERT_TRUE(sub_transport->Init(AmwContext{}));
        subscription = sub_transport->CreateSubscription(
            MakeEndpoint(channel),
            [&](const std::shared_ptr<message::RawMessage>& raw,
                const transport::MessageInfo&,
                const proto::RoleAttributes&) {
                if (raw && raw->message == payload) {
                    received.fetch_add(1);
                }
            });
        ASSERT_NE(subscription, nullptr);
        subscription->Enable();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    auto msg = std::make_shared<message::RawMessage>(payload);
    ASSERT_TRUE(publisher->Publish(msg));
    EXPECT_FALSE(WaitReceived(&received, 1, std::chrono::milliseconds(800)))
        << "samples must not cross AUTOLINK_DOMAIN_ID boundaries";

    subscription->Disable();
    publisher->Disable();
    sub_transport->Shutdown();
    pub_transport->Shutdown();
}

TEST(DomainIsolationTest, SameDomainDelivers) {
    const std::string channel = "/autolink/test/domain_same";
    const std::string payload = "domain-same-payload";

    EnvGuard domain("AUTOLINK_DOMAIN_ID", "213");
    auto transport_a = CreateCycloneDdsTransportProvider();
    auto transport_b = CreateCycloneDdsTransportProvider();
    ASSERT_TRUE(transport_a->Init(AmwContext{}));
    ASSERT_TRUE(transport_b->Init(AmwContext{}));

    std::atomic<int> received{0};
    auto subscription = transport_b->CreateSubscription(
        MakeEndpoint(channel),
        [&](const std::shared_ptr<message::RawMessage>& raw,
            const transport::MessageInfo&, const proto::RoleAttributes&) {
            if (raw && raw->message == payload) {
                received.fetch_add(1);
            }
        });
    ASSERT_NE(subscription, nullptr);
    subscription->Enable();

    auto publisher = transport_a->CreatePublisher(MakeEndpoint(channel));
    ASSERT_NE(publisher, nullptr);
    publisher->Enable();

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    auto msg = std::make_shared<message::RawMessage>(payload);
    ASSERT_TRUE(publisher->Publish(msg));
    EXPECT_TRUE(WaitReceived(&received, 1, std::chrono::milliseconds(2000)));

    subscription->Disable();
    publisher->Disable();
    transport_b->Shutdown();
    transport_a->Shutdown();
}

#endif  // AUTOLINK_ENABLE_CYCLONEDDS

}  // namespace
}  // namespace amw
}  // namespace autolink
