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

#include "autolink/amw/dds/fastdds_provider.hpp"

#include <algorithm>
#include <atomic>
#include <vector>

#include "autolink/amw/dds/dds_stub_provider.hpp"
#include "autolink/amw/dds/qos_utils.hpp"
#include "autolink/common/log.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/qos_profile.pb.h"
#include "autolink/transport/common/identity.hpp"

#ifdef AUTOLINK_ENABLE_FASTDDS

#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/rtps/attributes/ResourceManagement.hpp>

#include "autolink/amw/dds/fastdds_participant.hpp"
#include "autolink/amw/dds/fastdds_raw_type.hpp"

namespace autolink {
namespace amw {
namespace {

using eprosima::fastdds::dds::DataReader;
using eprosima::fastdds::dds::DataReaderListener;
using eprosima::fastdds::dds::DataReaderQos;
using eprosima::fastdds::dds::DataWriter;
using eprosima::fastdds::dds::DataWriterQos;
using eprosima::fastdds::dds::RETCODE_OK;
using eprosima::fastdds::dds::SampleInfo;

constexpr const char* kTopologyChannel = "/autolink/topology";

void ApplyWriterQos(DataWriterQos* qos, const proto::QosProfile& in_profile) {
    const proto::QosProfile profile = ResolveQos(in_profile);
    qos->endpoint().history_memory_policy =
        eprosima::fastdds::rtps::DYNAMIC_RESERVE_MEMORY_MODE;
    if (profile.reliability() ==
        proto::QosReliabilityPolicy::RELIABILITY_BEST_EFFORT) {
        qos->reliability().kind =
            eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS;
    } else {
        qos->reliability().kind =
            eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
    }
    if (profile.history() == proto::QosHistoryPolicy::HISTORY_KEEP_ALL) {
        qos->history().kind = eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS;
        const int32_t budget = ResolveKeepAllSampleBudget(profile);
        qos->resource_limits().max_samples = budget;
        qos->resource_limits().max_instances = 1;
        qos->resource_limits().max_samples_per_instance = budget;
    } else {
        qos->history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        qos->history().depth = ResolveKeepLastDepth(profile);
    }
    if (profile.durability() ==
        proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
        qos->durability().kind =
            eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
    } else {
        qos->durability().kind =
            eprosima::fastdds::dds::VOLATILE_DURABILITY_QOS;
    }
}

void ApplyReaderQos(DataReaderQos* qos, const proto::QosProfile& in_profile) {
    const proto::QosProfile profile = ResolveQos(in_profile);
    qos->endpoint().history_memory_policy =
        eprosima::fastdds::rtps::DYNAMIC_RESERVE_MEMORY_MODE;
    if (profile.reliability() ==
        proto::QosReliabilityPolicy::RELIABILITY_BEST_EFFORT) {
        qos->reliability().kind =
            eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS;
    } else {
        qos->reliability().kind =
            eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
    }
    if (profile.history() == proto::QosHistoryPolicy::HISTORY_KEEP_ALL) {
        qos->history().kind = eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS;
        const int32_t budget = ResolveKeepAllSampleBudget(profile);
        qos->resource_limits().max_samples = budget;
        qos->resource_limits().max_instances = 1;
        qos->resource_limits().max_samples_per_instance = budget;
    } else {
        qos->history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        qos->history().depth = ResolveKeepLastDepth(profile);
    }
    if (profile.durability() ==
        proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
        qos->durability().kind =
            eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
    } else {
        qos->durability().kind =
            eprosima::fastdds::dds::VOLATILE_DURABILITY_QOS;
    }
}

class FastDdsPublisher final : public IPublisher
{
public:
    FastDdsPublisher(DataWriter* writer, proto::RoleAttributes attr)
        : writer_(writer), attr_(std::move(attr)), enabled_(true) {}

    ~FastDdsPublisher() override {
        auto* participant = FastDdsParticipant::Instance();
        if (participant && participant->publisher() && writer_) {
            participant->publisher()->delete_datawriter(writer_);
            writer_ = nullptr;
        }
    }

    void Enable() override {
        enabled_.store(true);
    }
    void Disable() override {
        enabled_.store(false);
    }

    bool Publish(const std::shared_ptr<message::RawMessage>& msg) override {
        if (!enabled_.load() || !writer_ || !msg) {
            return false;
        }
        if (msg->message.size() + 32 > kAutolinkRawMaxSerializedSize) {
            AERROR << "FastDDS message exceeds max serialized size channel="
                   << attr_.channel_name()
                   << " bytes=" << msg->message.size();
            return false;
        }
        AutolinkRawSample sample;
        sample.payload = msg->message;
        sample.seq_num = msg->timestamp;
        sample.sender_id = attr_.id();
        const auto ret = writer_->write(&sample);
        if (ret != RETCODE_OK) {
            AWARN << "FastDDS DataWriter::write failed ret="
                  << static_cast<int>(ret)
                  << " channel=" << attr_.channel_name()
                  << " bytes=" << sample.payload.size();
            return false;
        }
        return true;
    }

private:
    DataWriter* writer_ = nullptr;
    proto::RoleAttributes attr_;
    std::atomic<bool> enabled_;
};

class FastDdsReaderListener : public DataReaderListener
{
public:
    FastDdsReaderListener(MessageCallback callback, proto::RoleAttributes attr)
        : callback_(std::move(callback)),
          attr_(std::move(attr)),
          enabled_(true) {}

    void SetEnabled(bool enabled) {
        enabled_.store(enabled);
    }

    void on_data_available(DataReader* reader) override {
        if (!enabled_.load() || !callback_) {
            // Drain samples while disabled so history does not stall.
            AutolinkRawSample sample;
            SampleInfo info;
            while (reader->take_next_sample(&sample, &info) == RETCODE_OK) {
            }
            return;
        }
        AutolinkRawSample sample;
        SampleInfo info;
        while (reader->take_next_sample(&sample, &info) == RETCODE_OK) {
            if (!info.valid_data || !enabled_.load() || !callback_) {
                continue;
            }
            auto raw = std::make_shared<message::RawMessage>(sample.payload,
                                                             sample.seq_num);
            transport::MessageInfo msg_info;
            // Framed MessageInfo in payload is authoritative; fill seq only.
            msg_info.set_seq_num(sample.seq_num);
            msg_info.set_msg_seq_num(static_cast<int32_t>(sample.seq_num));
            callback_(raw, msg_info, attr_);
        }
    }

private:
    MessageCallback callback_;
    proto::RoleAttributes attr_;
    std::atomic<bool> enabled_;
};

class FastDdsSubscription final : public ISubscription
{
public:
    FastDdsSubscription(DataReader* reader,
                        std::unique_ptr<FastDdsReaderListener> listener)
        : reader_(reader), listener_(std::move(listener)) {}

    ~FastDdsSubscription() override {
        auto* participant = FastDdsParticipant::Instance();
        if (participant && participant->subscriber() && reader_) {
            participant->subscriber()->delete_datareader(reader_);
            reader_ = nullptr;
        }
    }

    void Enable() override {
        if (listener_) {
            listener_->SetEnabled(true);
        }
    }
    void Disable() override {
        if (listener_) {
            listener_->SetEnabled(false);
        }
    }

private:
    DataReader* reader_ = nullptr;
    std::unique_ptr<FastDdsReaderListener> listener_;
};

}  // namespace

bool FastDdsTransportProvider::Init(const AmwContext& /*context*/) {
    if (!FastDdsParticipant::Instance()->Init()) {
        return false;
    }
    inited_.store(true);
    AINFO << "FastDdsTransportProvider initialized (real Fast DDS).";
    return true;
}

void FastDdsTransportProvider::Shutdown() {
    inited_.store(false);
    FastDdsParticipant::Instance()->Shutdown();
}

std::shared_ptr<IPublisher> FastDdsTransportProvider::CreatePublisher(
    const EndpointDesc& endpoint) {
    if (!inited_.load() && !Init(AmwContext{})) {
        return nullptr;
    }
    auto* participant = FastDdsParticipant::Instance();
    auto* topic = participant->GetOrCreateTopic(endpoint.attr.channel_name());
    if (topic == nullptr || participant->publisher() == nullptr) {
        return nullptr;
    }
    DataWriterQos wqos;
    participant->publisher()->get_default_datawriter_qos(wqos);
    ApplyWriterQos(&wqos, endpoint.attr.qos_profile());
    auto* writer =
        participant->publisher()->create_datawriter(topic, wqos, nullptr);
    if (writer == nullptr) {
        AERROR << "FastDDS create_datawriter failed: "
               << endpoint.attr.channel_name();
        return nullptr;
    }
    return std::make_shared<FastDdsPublisher>(writer, endpoint.attr);
}

std::shared_ptr<ISubscription> FastDdsTransportProvider::CreateSubscription(
    const EndpointDesc& endpoint, const MessageCallback& callback) {
    if (!inited_.load() && !Init(AmwContext{})) {
        return nullptr;
    }
    auto* participant = FastDdsParticipant::Instance();
    auto* topic = participant->GetOrCreateTopic(endpoint.attr.channel_name());
    if (topic == nullptr || participant->subscriber() == nullptr) {
        return nullptr;
    }
    DataReaderQos rqos;
    participant->subscriber()->get_default_datareader_qos(rqos);
    ApplyReaderQos(&rqos, endpoint.attr.qos_profile());
    auto listener =
        std::make_unique<FastDdsReaderListener>(callback, endpoint.attr);
    auto* reader = participant->subscriber()->create_datareader(
        topic, rqos, listener.get());
    if (reader == nullptr) {
        AERROR << "FastDDS create_datareader failed: "
               << endpoint.attr.channel_name();
        return nullptr;
    }
    return std::make_shared<FastDdsSubscription>(reader, std::move(listener));
}

bool FastDdsDiscoveryProvider::Start() {
    if (started_.load()) {
        return true;
    }
    auto transport = CreateFastDdsTransportProvider();
    AmwContext ctx;
    if (!transport->Init(ctx)) {
        AERROR << "FastDdsDiscoveryProvider: transport Init failed.";
        return false;
    }
    EndpointDesc endpoint;
    endpoint.attr.set_channel_name(kTopologyChannel);
    endpoint.attr.mutable_qos_profile()->set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    // KEEP_ALL so late joiners observe Node/Channel/Service joins, not only
    // the latest ChangeMsg sample.
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
        AERROR << "FastDdsDiscoveryProvider failed to create topology endpoints.";
        return false;
    }
    started_.store(true);
    AINFO << "FastDdsDiscoveryProvider started on " << kTopologyChannel;
    return true;
}

int64_t FastDdsDiscoveryProvider::Subscribe(proto::ChangeType type,
                                            const ChangeCallback& callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t id = next_id_++;
    subscribers_[id] = {type, callback};
    return id;
}

void FastDdsDiscoveryProvider::Unsubscribe(int64_t subscription_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.erase(subscription_id);
}

bool FastDdsDiscoveryProvider::PublishChange(const proto::ChangeMsg& msg) {
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
    return publisher_->Publish(raw);
}

void FastDdsDiscoveryProvider::Shutdown() {
    started_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.clear();
    }
    subscription_.reset();
    publisher_.reset();
}

void FastDdsDiscoveryProvider::OnTopologyRaw(
    const std::shared_ptr<message::RawMessage>& raw,
    const transport::MessageInfo& /*info*/,
    const proto::RoleAttributes& /*attr*/) {
    if (!raw) {
        return;
    }
    proto::ChangeMsg msg;
    if (!msg.ParseFromString(raw->message)) {
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
}

std::shared_ptr<ITransportProvider> CreateFastDdsTransportProvider() {
    return std::make_shared<FastDdsTransportProvider>();
}

std::shared_ptr<IDiscoveryProvider> CreateFastDdsDiscoveryProvider() {
    return std::make_shared<FastDdsDiscoveryProvider>();
}

}  // namespace amw
}  // namespace autolink

#else  // !AUTOLINK_ENABLE_FASTDDS

namespace autolink {
namespace amw {

std::shared_ptr<ITransportProvider> CreateFastDdsTransportProvider() {
    return std::make_shared<DdsStubTransportProvider>(ProviderId::kFastDds);
}

std::shared_ptr<IDiscoveryProvider> CreateFastDdsDiscoveryProvider() {
    return std::make_shared<DdsStubDiscoveryProvider>(ProviderId::kFastDds);
}

}  // namespace amw
}  // namespace autolink

#endif
