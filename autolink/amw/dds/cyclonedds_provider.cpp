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

#include "autolink/amw/dds/cyclonedds_provider.hpp"

#include "autolink/amw/dds/dds_stub_provider.hpp"
#include "autolink/amw/dds/qos_utils.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"
#include "autolink/message/raw_message.hpp"

#ifdef AUTOLINK_ENABLE_CYCLONEDDS

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <dds/dds.h>

#include "AutolinkRaw.h"
#include "autolink/amw/dds/host_ip_utils.hpp"

namespace autolink {
namespace amw {
namespace {

constexpr const char* kTopologyChannel = "/autolink/topology";
constexpr size_t kMaxPayload = 256 * 1024;

uint32_t ResolveDomainId() {
    const char* env = std::getenv("AUTOLINK_DOMAIN_ID");
    if (env == nullptr || env[0] == '\0') {
        env = std::getenv("ROS_DOMAIN_ID");
    }
    if (env != nullptr && env[0] != '\0') {
        return static_cast<uint32_t>(std::strtoul(env, nullptr, 10));
    }
    return 0;
}

void ApplyEndpointQos(dds_qos_t* qos, const proto::QosProfile& in_profile) {
    const proto::QosProfile profile = ResolveQos(in_profile);
    if (profile.reliability() ==
        proto::QosReliabilityPolicy::RELIABILITY_BEST_EFFORT) {
        dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT,
                             DDS_SECS(1));
    } else {
        dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    }
    if (profile.history() == proto::QosHistoryPolicy::HISTORY_KEEP_ALL) {
        dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
        // KEEP_ALL needs explicit sample budget or late-joiner history collapses.
        const int32_t budget = ResolveKeepAllSampleBudget(profile);
        dds_qset_resource_limits(qos, budget, 1, budget);
    } else {
        const int32_t depth = ResolveKeepLastDepth(profile);
        dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, depth);
        dds_qset_resource_limits(qos, depth, 1, depth);
    }
    if (profile.durability() ==
        proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
        dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
    } else {
        dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
    }
}

// Pin advertised interface when AUTOLINK_IP/HostIp matches a real NIC and the
// user has not already provided CYCLONEDDS_URI. Skip fake/sim addresses so
// participant creation does not fail (e.g. amw_sim_dual_host.sh).
void MaybeApplyHostIpToCycloneEnv() {
    const auto& host_ip = common::GlobalData::Instance()->HostIp();
    if (host_ip.empty() || host_ip == "127.0.0.1") {
        return;
    }
    const char* existing = std::getenv("CYCLONEDDS_URI");
    if (existing != nullptr && existing[0] != '\0') {
        return;
    }
    if (!HostIpIsLocalInterface(host_ip)) {
        AINFO << "CycloneDDS skip CYCLONEDDS_URI pin; host_ip=" << host_ip
              << " is not a local interface (ok for same-host DIFF_HOST sim).";
        return;
    }
    const std::string uri =
        "<CycloneDDS><Domain id=\"any\"><General><Interfaces>"
        "<NetworkInterface address=\"" +
        host_ip +
        "\" presence_required=\"false\"/></Interfaces></General></Domain>"
        "</CycloneDDS>";
    ::setenv("CYCLONEDDS_URI", uri.c_str(), 0);
    AINFO << "CycloneDDS CYCLONEDDS_URI pinned to host_ip=" << host_ip;
}

class CycloneDdsPublisher final : public IPublisher
{
public:
    CycloneDdsPublisher(dds_entity_t writer, proto::RoleAttributes attr)
        : writer_(writer), attr_(std::move(attr)), enabled_(true) {}

    ~CycloneDdsPublisher() override {
        if (writer_ > 0) {
            dds_delete(writer_);
            writer_ = 0;
        }
    }

    void Enable() override {
        enabled_.store(true);
    }
    void Disable() override {
        enabled_.store(false);
    }

    bool Publish(const std::shared_ptr<message::RawMessage>& msg) override {
        if (!enabled_.load() || writer_ <= 0 || !msg) {
            return false;
        }
        if (msg->message.size() > kMaxPayload) {
            AERROR << "CycloneDDS message too large channel="
                   << attr_.channel_name()
                   << " bytes=" << msg->message.size();
            return false;
        }
        autolink_amw_AutolinkRawSample sample{};
        sample.seq_num = msg->timestamp;
        sample.sender_id = attr_.id();
        sample.payload._maximum = static_cast<uint32_t>(msg->message.size());
        sample.payload._length = sample.payload._maximum;
        sample.payload._buffer =
            reinterpret_cast<uint8_t*>(const_cast<char*>(msg->message.data()));
        sample.payload._release = false;
        const dds_return_t rc = dds_write(writer_, &sample);
        if (rc != DDS_RETCODE_OK) {
            AWARN << "CycloneDDS dds_write failed rc=" << rc
                  << " channel=" << attr_.channel_name();
            return false;
        }
        return true;
    }

private:
    dds_entity_t writer_ = 0;
    proto::RoleAttributes attr_;
    std::atomic<bool> enabled_;
};

class CycloneDdsSubscription final : public ISubscription
{
public:
    CycloneDdsSubscription(dds_entity_t reader, MessageCallback callback,
                           proto::RoleAttributes attr)
        : reader_(reader),
          callback_(std::move(callback)),
          attr_(std::move(attr)),
          enabled_(true),
          running_(true) {
        thread_ = std::thread([this]() { PollLoop(); });
    }

    ~CycloneDdsSubscription() override {
        running_.store(false);
        if (thread_.joinable()) {
            thread_.join();
        }
        if (reader_ > 0) {
            dds_delete(reader_);
            reader_ = 0;
        }
    }

    void Enable() override {
        enabled_.store(true);
    }
    void Disable() override {
        enabled_.store(false);
    }

private:
    void PollLoop() {
        dds_entity_t participant = dds_get_participant(reader_);
        dds_entity_t waitset = 0;
        if (participant > 0) {
            waitset = dds_create_waitset(participant);
            if (waitset > 0) {
                dds_waitset_attach(waitset, reader_, 0);
            }
        }
        while (running_.load()) {
            if (waitset > 0) {
                dds_attach_t xs[1];
                (void)dds_waitset_wait(waitset, xs, 1, DDS_MSECS(100));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!running_.load() || !enabled_.load() || !callback_) {
                continue;
            }
            void* samples[1] = {nullptr};
            dds_sample_info_t infos[1];
            const dds_return_t n = dds_take(reader_, samples, infos, 1, 1);
            if (n <= 0 || !infos[0].valid_data || samples[0] == nullptr) {
                continue;
            }
            auto* sample =
                static_cast<autolink_amw_AutolinkRawSample*>(samples[0]);
            std::string bytes;
            if (sample->payload._buffer != nullptr &&
                sample->payload._length > 0) {
                bytes.assign(reinterpret_cast<char*>(sample->payload._buffer),
                             sample->payload._length);
            }
            auto raw =
                std::make_shared<message::RawMessage>(bytes, sample->seq_num);
            transport::MessageInfo info;
            info.set_seq_num(sample->seq_num);
            info.set_msg_seq_num(static_cast<int32_t>(sample->seq_num));
            if (enabled_.load() && callback_) {
                callback_(raw, info, attr_);
            }
            dds_return_loan(reader_, samples, n);
        }
        if (waitset > 0) {
            dds_delete(waitset);
        }
    }

    dds_entity_t reader_ = 0;
    MessageCallback callback_;
    proto::RoleAttributes attr_;
    std::atomic<bool> enabled_;
    std::atomic<bool> running_;
    std::thread thread_;
};

dds_entity_t CreateTopic(dds_entity_t participant, const std::string& name) {
    return dds_create_topic(participant, &autolink_amw_AutolinkRawSample_desc,
                            name.c_str(), nullptr, nullptr);
}

}  // namespace

bool CycloneDdsTransportProvider::Init(const AmwContext& /*context*/) {
    if (inited_.load()) {
        return true;
    }
    MaybeApplyHostIpToCycloneEnv();
    const uint32_t domain = ResolveDomainId();
    participant_ = dds_create_participant(static_cast<dds_domainid_t>(domain),
                                          nullptr, nullptr);
    if (participant_ <= 0) {
        AERROR << "CycloneDDS create_participant failed domain=" << domain
               << " rc=" << participant_;
        participant_ = 0;
        return false;
    }
    inited_.store(true);
    AINFO << "CycloneDdsTransportProvider initialized domain=" << domain
          << " host_ip=" << common::GlobalData::Instance()->HostIp();
    return true;
}

void CycloneDdsTransportProvider::Shutdown() {
    inited_.store(false);
    if (participant_ > 0) {
        dds_delete(participant_);
        participant_ = 0;
    }
}

std::shared_ptr<IPublisher> CycloneDdsTransportProvider::CreatePublisher(
    const EndpointDesc& endpoint) {
    if (!inited_.load() && !Init(AmwContext{})) {
        return nullptr;
    }
    dds_entity_t topic =
        CreateTopic(participant_, endpoint.attr.channel_name());
    if (topic <= 0) {
        AERROR << "CycloneDDS create_topic failed: "
               << endpoint.attr.channel_name();
        return nullptr;
    }
    dds_qos_t* qos = dds_create_qos();
    ApplyEndpointQos(qos, endpoint.attr.qos_profile());
    dds_entity_t writer =
        dds_create_writer(participant_, topic, qos, nullptr);
    dds_delete_qos(qos);
    if (writer <= 0) {
        AERROR << "CycloneDDS create_writer failed: "
               << endpoint.attr.channel_name();
        return nullptr;
    }
    return std::make_shared<CycloneDdsPublisher>(writer, endpoint.attr);
}

std::shared_ptr<ISubscription> CycloneDdsTransportProvider::CreateSubscription(
    const EndpointDesc& endpoint, const MessageCallback& callback) {
    if (!inited_.load() && !Init(AmwContext{})) {
        return nullptr;
    }
    dds_entity_t topic =
        CreateTopic(participant_, endpoint.attr.channel_name());
    if (topic <= 0) {
        AERROR << "CycloneDDS create_topic failed: "
               << endpoint.attr.channel_name();
        return nullptr;
    }
    dds_qos_t* qos = dds_create_qos();
    ApplyEndpointQos(qos, endpoint.attr.qos_profile());
    dds_entity_t reader =
        dds_create_reader(participant_, topic, qos, nullptr);
    dds_delete_qos(qos);
    if (reader <= 0) {
        AERROR << "CycloneDDS create_reader failed: "
               << endpoint.attr.channel_name();
        return nullptr;
    }
    return std::make_shared<CycloneDdsSubscription>(reader, callback,
                                                    endpoint.attr);
}

bool CycloneDdsDiscoveryProvider::Start() {
    if (started_.load()) {
        return true;
    }
    auto transport = CreateCycloneDdsTransportProvider();
    AmwContext ctx;
    if (!transport->Init(ctx)) {
        AERROR << "CycloneDdsDiscoveryProvider: transport Init failed.";
        return false;
    }
    EndpointDesc endpoint;
    endpoint.attr.set_channel_name(kTopologyChannel);
    endpoint.attr.mutable_qos_profile()->set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    // KEEP_ALL: late joiners must see Node/Channel/Service joins, not only the
    // latest sample (KEEP_LAST depth was observed to behave like depth=1).
    endpoint.attr.mutable_qos_profile()->set_history(
        proto::QosHistoryPolicy::HISTORY_KEEP_ALL);
    endpoint.attr.mutable_qos_profile()->set_depth(1000);
    endpoint.attr.mutable_qos_profile()->set_durability(
        proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL);

    publisher_ = transport->CreatePublisher(endpoint);
    subscription_ = transport->CreateSubscription(
        endpoint, [this](const std::shared_ptr<message::RawMessage>& raw,
                         const transport::MessageInfo& info,
                         const proto::RoleAttributes& attr) {
            OnTopologyRaw(raw, info, attr);
        });
    if (!publisher_ || !subscription_) {
        AERROR << "CycloneDdsDiscoveryProvider failed to create topology "
                  "endpoints.";
        return false;
    }
    started_.store(true);
    AINFO << "CycloneDdsDiscoveryProvider started on " << kTopologyChannel;
    return true;
}

int64_t CycloneDdsDiscoveryProvider::Subscribe(proto::ChangeType type,
                                               const ChangeCallback& callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t id = next_id_++;
    subscribers_[id] = {type, callback};
    return id;
}

void CycloneDdsDiscoveryProvider::Unsubscribe(int64_t subscription_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.erase(subscription_id);
}

bool CycloneDdsDiscoveryProvider::PublishChange(const proto::ChangeMsg& msg) {
    if (!publisher_) {
        return false;
    }
    std::string bytes;
    if (!msg.SerializeToString(&bytes)) {
        return false;
    }
    auto raw = std::make_shared<message::RawMessage>(bytes);
    // Unique seq so writer history cannot collapse distinct topology samples.
    static std::atomic<uint64_t> topo_seq{1};
    raw->timestamp = topo_seq.fetch_add(1);
    const bool ok = publisher_->Publish(raw);
    ADEBUG << "CycloneDDS topology publish ok=" << ok
           << " change_type=" << static_cast<int>(msg.change_type())
           << " role=" << static_cast<int>(msg.role_type())
           << " channel=" << msg.role_attr().channel_name()
           << " service=" << msg.role_attr().service_name()
           << " bytes=" << bytes.size();
    return ok;
}

void CycloneDdsDiscoveryProvider::Shutdown() {
    started_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.clear();
    }
    subscription_.reset();
    publisher_.reset();
}

void CycloneDdsDiscoveryProvider::OnTopologyRaw(
    const std::shared_ptr<message::RawMessage>& raw,
    const transport::MessageInfo& /*info*/,
    const proto::RoleAttributes& /*attr*/) {
    if (!raw) {
        return;
    }
    proto::ChangeMsg msg;
    if (!msg.ParseFromString(raw->message)) {
        AWARN << "CycloneDDS topology ChangeMsg parse failed.";
        return;
    }
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks.reserve(subscribers_.size());
        for (auto& item : subscribers_) {
            if (item.second.first == msg.change_type() && item.second.second) {
                callbacks.push_back(item.second.second);
            }
        }
    }
    for (auto& cb : callbacks) {
        cb(msg);
    }
    ADEBUG << "CycloneDDS topology recv change_type="
           << static_cast<int>(msg.change_type())
           << " role=" << static_cast<int>(msg.role_type())
           << " channel=" << msg.role_attr().channel_name()
           << " service=" << msg.role_attr().service_name()
           << " delivered=" << callbacks.size();
}

std::shared_ptr<ITransportProvider> CreateCycloneDdsTransportProvider() {
    return std::make_shared<CycloneDdsTransportProvider>();
}

std::shared_ptr<IDiscoveryProvider> CreateCycloneDdsDiscoveryProvider() {
    return std::make_shared<CycloneDdsDiscoveryProvider>();
}

}  // namespace amw
}  // namespace autolink

#else  // !AUTOLINK_ENABLE_CYCLONEDDS

namespace autolink {
namespace amw {

std::shared_ptr<ITransportProvider> CreateCycloneDdsTransportProvider() {
    return std::make_shared<DdsStubTransportProvider>(ProviderId::kCycloneDds);
}

std::shared_ptr<IDiscoveryProvider> CreateCycloneDdsDiscoveryProvider() {
    return std::make_shared<DdsStubDiscoveryProvider>(ProviderId::kCycloneDds);
}

}  // namespace amw
}  // namespace autolink

#endif  // AUTOLINK_ENABLE_CYCLONEDDS
