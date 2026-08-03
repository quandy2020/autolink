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

#include "autolink/amw/amw.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include "autolink/amw/dds/cyclonedds_provider.hpp"
#include "autolink/amw/dds/dds_stub_provider.hpp"
#include "autolink/amw/dds/fastdds_provider.hpp"
#include "autolink/amw/discovery/discovery_bridge.hpp"
#include "autolink/amw/network_adapter.hpp"
#include "autolink/amw/plugin_abi.h"
#include "autolink/amw/plugin_loader.hpp"
#include "autolink/amw/provider_registry.hpp"
#include "autolink/amw/router.hpp"
#include "autolink/amw/types.hpp"
#include "autolink/init.hpp"
#include "autolink/proto/unit_test.pb.h"
#include "autolink/transport/common/identity.hpp"
#include "autolink/transport/transport.hpp"
#include "gtest/gtest.h"

namespace autolink {
namespace amw {
namespace {

TEST(ProviderRegistryTest, RegisterAndGet) {
    ProviderRegistry registry;
    auto local = std::make_shared<LocalProvider>();
    registry.RegisterTransport(local);
    registry.RegisterTransport(CreateFastDdsTransportProvider());
    RegisterBuiltinDdsProviders(&registry);

    EXPECT_EQ(registry.GetTransport(ProviderId::kLocal)->id(),
              ProviderId::kLocal);
    EXPECT_EQ(registry.GetTransport(ProviderId::kFastDds)->id(),
              ProviderId::kFastDds);
    EXPECT_EQ(registry.GetTransport(ProviderId::kCycloneDds)->id(),
              ProviderId::kCycloneDds);
    EXPECT_EQ(registry.GetTransport(ProviderId::kOpenDds)->id(),
              ProviderId::kOpenDds);
    EXPECT_EQ(registry.GetTransport(ProviderId::kConnextDds)->id(),
              ProviderId::kConnextDds);
    EXPECT_EQ(registry.GetTransportByName("fastdds")->id(),
              ProviderId::kFastDds);
    EXPECT_EQ(registry.GetTransportByName("rmw_cyclonedds_cpp")->id(),
              ProviderId::kCycloneDds);
}

TEST(PluginLoaderTest, BuiltinListAndLibraryNames) {
    auto builtins = ListBuiltinImplementations();
    EXPECT_NE(std::find(builtins.begin(), builtins.end(), "amw_fastdds"),
              builtins.end());
    EXPECT_NE(std::find(builtins.begin(), builtins.end(), "amw_cyclonedds"),
              builtins.end());
    auto libs = PluginLibraryCandidates("rmw_fastrtps_cpp");
    ASSERT_FALSE(libs.empty());
#if defined(__APPLE__)
    EXPECT_NE(libs[0].find("amw_fastdds"), std::string::npos);
#else
    EXPECT_NE(libs[0].find("amw_fastdds"), std::string::npos);
#endif
}

TEST(PluginLoaderTest, SplitSearchPath) {
    auto parts = SplitSearchPath("/a/b:/c/d;/e");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "/a/b");
    EXPECT_EQ(parts[1], "/c/d");
    EXPECT_EQ(parts[2], "/e");
    EXPECT_TRUE(SplitSearchPath("").empty());
}

TEST(PluginLoaderTest, AbiVersionConstant) {
    EXPECT_EQ(AUTOLINK_AMW_PLUGIN_ABI_VERSION, 1);
}

TEST(ImplementationNameTest, Ros2CompatibleAliases) {
    ProviderId id = ProviderId::kLocal;
    EXPECT_TRUE(ResolveNetworkImplementation("rmw_fastrtps_cpp", &id));
    EXPECT_EQ(id, ProviderId::kFastDds);
    EXPECT_EQ(std::string(ImplementationName(id)), "amw_fastdds");

    EXPECT_TRUE(ResolveNetworkImplementation("rmw_cyclonedds_cpp", &id));
    EXPECT_EQ(id, ProviderId::kCycloneDds);

    EXPECT_TRUE(ResolveNetworkImplementation("amw_opendds", &id));
    EXPECT_EQ(id, ProviderId::kOpenDds);

    EXPECT_TRUE(ResolveNetworkImplementation("contextdds", &id));
    EXPECT_EQ(id, ProviderId::kConnextDds);
    EXPECT_TRUE(ResolveNetworkImplementation("rmw_connextdds", &id));
    EXPECT_EQ(id, ProviderId::kConnextDds);

    EXPECT_FALSE(ResolveNetworkImplementation("not_a_dds", &id));
}

TEST(RouterTest, DefaultRelationRouting) {
    Router router;
    proto::RoleAttributes self;
    self.set_host_ip("192.168.1.1");
    self.set_process_id(100);
    self.set_channel_name("/ch");

    proto::RoleAttributes same = self;
    auto d0 = router.Resolve(self, same);
    EXPECT_EQ(d0.provider, ProviderId::kLocal);
    EXPECT_EQ(d0.mode, proto::OptionalMode::INTRA);

    proto::RoleAttributes diff_proc = self;
    diff_proc.set_process_id(200);
    auto d1 = router.Resolve(self, diff_proc);
    EXPECT_EQ(d1.provider, ProviderId::kLocal);
    EXPECT_EQ(d1.mode, proto::OptionalMode::SHM);

    proto::RoleAttributes diff_host = self;
    diff_host.set_host_ip("192.168.1.2");
    auto d2 = router.Resolve(self, diff_host);
    EXPECT_EQ(d2.provider, ProviderId::kFastDds);
    EXPECT_EQ(d2.mode, proto::OptionalMode::RTPS);
}

TEST(RouterTest, ConfiguredCommunicationMode) {
    Router router;
    proto::CommunicationMode mode;
    mode.set_same_proc(proto::OptionalMode::SHM);
    mode.set_diff_proc(proto::OptionalMode::SHM);
    mode.set_diff_host(proto::OptionalMode::SHM);
    router.SetCommunicationMode(mode);

    auto d = router.Resolve(Relation::kDiffHost);
    EXPECT_EQ(d.provider, ProviderId::kLocal);
    EXPECT_EQ(d.mode, proto::OptionalMode::SHM);
}

TEST(RouterTest, NetworkProviderSelection) {
    Router router;
    router.SetDefaultNetworkProvider(ProviderId::kCycloneDds);
    EXPECT_EQ(router.ProviderForMode(proto::OptionalMode::RTPS),
              ProviderId::kCycloneDds);

    router.SetDefaultNetworkProvider(ProviderId::kConnextDds);
    EXPECT_EQ(router.ProviderForMode(proto::OptionalMode::RTPS),
              ProviderId::kConnextDds);
}

TEST(AmwDiscoveryBridgeTest, WrapsProvider) {
    auto local_disc = std::make_shared<LocalDiscoveryProvider>();
    AmwDiscoveryBackend backend(local_disc);
    EXPECT_TRUE(backend.Start());
    backend.Shutdown();
}

TEST(AmwTransportTest, LocalModesStillWork) {
    using transport::OptionalMode;
    using transport::Transport;

    proto::RoleAttributes attr;
    attr.set_channel_name("amw_local_tx");
    transport::Identity id;
    attr.set_id(id.HashValue());

    auto intra = Transport::Instance()->CreateTransmitter<proto::UnitTest>(
        attr, OptionalMode::INTRA);
    ASSERT_NE(intra, nullptr);

    auto shm = Transport::Instance()->CreateTransmitter<proto::UnitTest>(
        attr, OptionalMode::SHM);
    ASSERT_NE(shm, nullptr);
}

TEST(AmwTransportTest, RtpsNetworkPath) {
    using transport::OptionalMode;
    using transport::Transport;

    // Ensure default network provider matches a built-in real DDS when available.
#if defined(AUTOLINK_ENABLE_CYCLONEDDS)
    setenv("AUTOLINK_AMW_IMPLEMENTATION", "amw_cyclonedds", 1);
#elif defined(AUTOLINK_ENABLE_FASTDDS)
    setenv("AUTOLINK_AMW_IMPLEMENTATION", "amw_fastdds", 1);
#endif
    Amw::Instance()->Shutdown();
    ASSERT_TRUE(Amw::Instance()->Init());

    proto::RoleAttributes attr;
    attr.set_channel_name("amw_rtps_path");
    transport::Identity id;
    attr.set_id(id.HashValue());

    auto tx = Transport::Instance()->CreateTransmitter<proto::UnitTest>(
        attr, OptionalMode::RTPS);
    auto listener = [](const std::shared_ptr<proto::UnitTest>&,
                       const transport::MessageInfo&,
                       const proto::RoleAttributes&) {};
    auto rx = Transport::Instance()->CreateReceiver<proto::UnitTest>(
        attr, listener, OptionalMode::RTPS);
#if defined(AUTOLINK_ENABLE_FASTDDS) || defined(AUTOLINK_ENABLE_CYCLONEDDS)
    EXPECT_TRUE(Amw::Instance()->IsNetworkMiddlewareReady());
    EXPECT_NE(tx, nullptr);
    EXPECT_NE(rx, nullptr);
#else
    EXPECT_FALSE(Amw::Instance()->IsNetworkMiddlewareReady());
    EXPECT_EQ(tx, nullptr);
#endif
}

#ifdef AUTOLINK_ENABLE_CYCLONEDDS
TEST(AmwTransportTest, CycloneDdsLoopback) {
    setenv("AUTOLINK_AMW_IMPLEMENTATION", "amw_cyclonedds", 1);
    // Re-init path is singleton-heavy; exercise provider API directly.
    auto transport = CreateCycloneDdsTransportProvider();
    ASSERT_TRUE(transport->Init(AmwContext{}));
    EXPECT_FALSE(transport->IsStub());

    EndpointDesc endpoint;
    endpoint.attr.set_channel_name("/amw/cyclone_loopback");
    endpoint.attr.mutable_qos_profile()->set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    endpoint.attr.mutable_qos_profile()->set_depth(10);

    std::atomic<int> got{0};
    auto sub = transport->CreateSubscription(
        endpoint, [&](const std::shared_ptr<message::RawMessage>& raw,
                      const transport::MessageInfo&,
                      const proto::RoleAttributes&) {
            if (raw && raw->message == "ping") {
                got.fetch_add(1);
            }
        });
    auto pub = transport->CreatePublisher(endpoint);
    ASSERT_NE(sub, nullptr);
    ASSERT_NE(pub, nullptr);
    auto msg = std::make_shared<message::RawMessage>("ping");
    ASSERT_TRUE(pub->Publish(msg));
    for (int i = 0; i < 50 && got.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_GE(got.load(), 1);
    transport->Shutdown();
}
#endif

#ifdef AUTOLINK_ENABLE_FASTDDS
TEST(AmwTransportTest, FastDdsLoopback) {
    using transport::OptionalMode;
    using transport::Transport;

    proto::RoleAttributes attr;
    attr.set_channel_name("/amw/fastdds_loopback");
    transport::Identity id;
    attr.set_id(id.HashValue());
    attr.mutable_qos_profile()->set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    attr.mutable_qos_profile()->set_depth(10);

    std::atomic<int> got{0};
    auto listener = [&](const std::shared_ptr<proto::UnitTest>& msg,
                        const transport::MessageInfo&,
                        const proto::RoleAttributes&) {
        if (msg && msg->case_name() == "ping") {
            got.fetch_add(1);
        }
    };

    auto rx = Transport::Instance()->CreateReceiver<proto::UnitTest>(
        attr, listener, OptionalMode::RTPS);
    auto tx = Transport::Instance()->CreateTransmitter<proto::UnitTest>(
        attr, OptionalMode::RTPS);
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);

    auto msg = std::make_shared<proto::UnitTest>();
    msg->set_class_name("AmwTransportTest");
    msg->set_case_name("ping");
    ASSERT_TRUE(tx->Transmit(msg));

    for (int i = 0; i < 50 && got.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_GE(got.load(), 1);
}
#endif

TEST(DdsStubProviderTest, AllVendorsCreateReturnNull) {
    static const ProviderId kVendors[] = {
        ProviderId::kFastDds,
        ProviderId::kCycloneDds,
        ProviderId::kOpenDds,
        ProviderId::kConnextDds,
    };
    AmwContext ctx;
    EndpointDesc endpoint;
    endpoint.attr.set_channel_name("/stub");
    for (ProviderId id : kVendors) {
        DdsStubTransportProvider provider(id);
        ASSERT_TRUE(provider.Init(ctx));
        EXPECT_TRUE(provider.IsStub());
        EXPECT_EQ(provider.CreatePublisher(endpoint), nullptr);
        EXPECT_EQ(provider.CreateSubscription(
                      endpoint, [](const std::shared_ptr<message::RawMessage>&,
                                   const transport::MessageInfo&,
                                   const proto::RoleAttributes&) {}),
                  nullptr);
        provider.Shutdown();

        DdsStubDiscoveryProvider discovery(id);
        EXPECT_TRUE(discovery.IsStub());
        EXPECT_FALSE(discovery.Start());
        discovery.Shutdown();
    }
}

#if defined(AUTOLINK_ENABLE_CYCLONEDDS) && !defined(AUTOLINK_ENABLE_FASTDDS)
TEST(AmwInitTest, PreferReadySelectsCycloneWhenFastDdsStub) {
    unsetenv("AUTOLINK_AMW_IMPLEMENTATION");
    unsetenv("RMW_IMPLEMENTATION");
    unsetenv("AUTOLINK_AMW_NETWORK_PROVIDER");
    Amw::Instance()->Shutdown();
    ASSERT_TRUE(Amw::Instance()->Init());
    EXPECT_TRUE(Amw::Instance()->IsNetworkMiddlewareReady());
    EXPECT_EQ(Amw::Instance()->selected_network_provider(),
              ProviderId::kCycloneDds);
}

TEST(AmwInitTest, EnvPinKeepsStubFastDds) {
    setenv("AUTOLINK_AMW_IMPLEMENTATION", "amw_fastdds", 1);
    Amw::Instance()->Shutdown();
    ASSERT_TRUE(Amw::Instance()->Init());
    EXPECT_FALSE(Amw::Instance()->IsNetworkMiddlewareReady());
    EXPECT_EQ(Amw::Instance()->selected_network_provider(),
              ProviderId::kFastDds);
    unsetenv("AUTOLINK_AMW_IMPLEMENTATION");
    Amw::Instance()->Shutdown();
}
#endif

#if defined(AUTOLINK_ENABLE_CYCLONEDDS)

TEST(AmwTransportTest, FramedMessageInfoRoundTrip) {
    auto transport = CreateCycloneDdsTransportProvider();
    ASSERT_TRUE(transport->Init(AmwContext{}));
    proto::RoleAttributes attr;
    attr.set_channel_name("/amw/framed_msginfo");
    transport::Identity id;
    attr.set_id(id.HashValue());
    attr.mutable_qos_profile()->set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    attr.mutable_qos_profile()->set_depth(10);

    std::atomic<int> got{0};
    transport::Identity spare;
    std::atomic<uint64_t> got_spare{0};
    auto rx = std::make_shared<NetworkReceiver<proto::UnitTest>>(
        attr,
        [&](const std::shared_ptr<proto::UnitTest>& msg,
            const transport::MessageInfo& info,
            const proto::RoleAttributes&) {
            if (msg && msg->case_name() == "framed") {
                got_spare.store(info.spare_id().HashValue());
                got.fetch_add(1);
            }
        },
        transport);
    EndpointDesc endpoint;
    endpoint.attr = attr;
    auto pub = transport->CreatePublisher(endpoint);
    ASSERT_NE(pub, nullptr);
    NetworkTransmitter<proto::UnitTest> tx(attr, pub);
    auto msg = std::make_shared<proto::UnitTest>();
    msg->set_class_name("AmwTransportTest");
    msg->set_case_name("framed");
    transport::MessageInfo info;
    info.set_seq_num(7);
    info.set_spare_id(spare);
    ASSERT_TRUE(tx.Transmit(msg, info));
    for (int i = 0; i < 50 && got.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_GE(got.load(), 1);
    EXPECT_EQ(got_spare.load(), spare.HashValue());
    transport->Shutdown();
}

TEST(AmwTransportTest, PublisherDisableBlocksPublish) {
    auto transport = CreateCycloneDdsTransportProvider();
    ASSERT_TRUE(transport->Init(AmwContext{}));
    EndpointDesc endpoint;
    endpoint.attr.set_channel_name("/amw/disable_pub");
    auto pub = transport->CreatePublisher(endpoint);
    ASSERT_NE(pub, nullptr);
    pub->Disable();
    auto msg = std::make_shared<message::RawMessage>("x");
    EXPECT_FALSE(pub->Publish(msg));
    pub->Enable();
    EXPECT_TRUE(pub->Publish(msg));
    transport->Shutdown();
}
#endif

TEST(PluginLoaderTest, LoadOpenDdsStubPluginIfPresent) {
    const char* build = std::getenv("AUTOLINK_BUILD_DIR");
    std::string plugin_dir = build ? build : "build";
    plugin_dir += "/lib";
    setenv("AUTOLINK_AMW_PLUGIN_PATH", plugin_dir.c_str(), 1);
    LoadedPlugin plugin;
    if (!TryLoadExternalPlugin("amw_opendds", &plugin)) {
        GTEST_SKIP() << "libamw_opendds not built under " << plugin_dir;
    }
    ASSERT_NE(plugin.transport, nullptr);
    EXPECT_TRUE(plugin.transport->IsStub());
    EXPECT_EQ(plugin.identifier, "amw_opendds");
    unsetenv("AUTOLINK_AMW_PLUGIN_PATH");
}

}  // namespace
}  // namespace amw
}  // namespace autolink

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    autolink::Init(argv[0]);
    return RUN_ALL_TESTS();
}
