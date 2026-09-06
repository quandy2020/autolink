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

#include "autolink/service_discovery/rtps_topology_backend.hpp"

#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"
#include "autolink/transport/qos/qos_profile_conf.hpp"
#include "autolink/transport/rtps/attributes_filler.hpp"
#include "autolink/transport/rtps/participant_hub.hpp"
#include "autolink/transport/rtps/payload_limit.hpp"
#include "autolink/transport/rtps/underlay_message.hpp"
#include "fastdds/dds/subscriber/SampleInfo.hpp"
#include "fastrtps/types/TypesBase.h"

namespace autolink {
namespace service_discovery {
namespace {

constexpr const char* kChangeMsgDatatype = "ChangeMsg";

constexpr const char* kNodeTopic = "node_change_broadcast";
constexpr const char* kChannelTopic = "channel_change_broadcast";
constexpr const char* kServiceTopic = "service_change_broadcast";

using eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
using eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
using eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT;
using eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT;
using eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
using transport::AttributesFiller;
using transport::CheckPayloadSize;
using transport::PayloadCheck;
using transport::PayloadLimit;
using transport::QosProfileConf;
using transport::RtpsParticipantHub;
using transport::UnderlayMessage;

}  // namespace

class RtpsTopologyBackend::TopologyReaderListener
    : public eprosima::fastdds::dds::DataReaderListener
{
public:
    explicit TopologyReaderListener(RtpsTopologyBackend* backend)
        : backend_(backend) {}

    void on_data_available(
            eprosima::fastdds::dds::DataReader* reader) override {
        RETURN_IF_NULL(reader);
        RETURN_IF_NULL(backend_);

        std::lock_guard<std::mutex> lock(mutex_);
        UnderlayMessage sample;
        eprosima::fastdds::dds::SampleInfo info;
        while (reader->take_next_sample(&sample, &info) ==
               eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK) {
            if (!info.valid_data) {
                continue;
            }
            backend_->OnUnderlaySample(sample.datatype(), sample.data());
        }
    }

private:
    RtpsTopologyBackend* backend_ = nullptr;
    std::mutex mutex_;
};

RtpsTopologyBackend::RtpsTopologyBackend() = default;

RtpsTopologyBackend::~RtpsTopologyBackend() {
    Shutdown();
}

bool RtpsTopologyBackend::Start() {
    if (running_.exchange(true)) {
        return true;
    }

    self_host_name_ = common::GlobalData::Instance()->HostName();
    self_process_id_ = common::GlobalData::Instance()->ProcessId();

    auto& hub = RtpsParticipantHub::Instance();
    const auto& attr = common::GlobalData::Instance()
                               ->Config()
                               .transport_conf()
                               .participant_attr();
    if (!hub.Init(attr)) {
        AERROR << "RtpsTopologyBackend: RtpsParticipantHub Init failed";
        running_.store(false);
        return false;
    }

    participant_ = hub.TopologyParticipant();
    if (participant_ == nullptr || participant_->get() == nullptr) {
        AERROR << "RtpsTopologyBackend: topology participant unavailable";
        running_.store(false);
        return false;
    }

    if (!CreateEndpoints()) {
        DestroyEndpoints();
        participant_.reset();
        running_.store(false);
        return false;
    }

    AINFO << "RtpsTopologyBackend started on topology participant";
    return true;
}

int64_t RtpsTopologyBackend::Subscribe(proto::ChangeType type,
                                       const ChangeCallback& callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    const int64_t subscription_id = next_subscription_id_++;
    subscribers_[subscription_id] = Subscriber{type, callback};
    return subscription_id;
}

void RtpsTopologyBackend::Unsubscribe(int64_t subscription_id) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.erase(subscription_id);
}

bool RtpsTopologyBackend::Publish(const proto::ChangeMsg& msg) {
    if (!running_.load()) {
        return false;
    }

    TopicEndpoint* endpoint = EndpointFor(msg.change_type());
    if (endpoint == nullptr || endpoint->writer == nullptr) {
        AERROR << "RtpsTopologyBackend: no writer for change_type="
               << static_cast<int>(msg.change_type());
        return false;
    }

    std::string bytes;
    if (!msg.SerializeToString(&bytes)) {
        AERROR << "RtpsTopologyBackend: failed to serialize ChangeMsg";
        return false;
    }

    UnderlayMessage underlay;
    underlay.datatype(kChangeMsgDatatype);
    underlay.data(std::move(bytes));
    underlay.timestamp(static_cast<int32_t>(0x0fffffff & msg.timestamp()));
    underlay.seq(0);

    const auto payload_limit = PayloadLimit::FromEnv();
    const auto check =
            CheckPayloadSize(underlay.data().size(), payload_limit);
    if (check == PayloadCheck::kWarn) {
        AWARN << "RtpsTopologyBackend payload exceeds soft limit: size="
              << underlay.data().size()
              << " max=" << payload_limit.max_bytes;
    } else if (check == PayloadCheck::kReject) {
        AERROR << "RtpsTopologyBackend payload rejected (oversize): size="
               << underlay.data().size()
               << " max=" << payload_limit.max_bytes;
        return false;
    }

    std::lock_guard<std::mutex> lock(publish_mutex_);
    if (!endpoint->writer->write(&underlay)) {
        AERROR << "RtpsTopologyBackend: DataWriter::write failed topic="
               << endpoint->topic_name;
        return false;
    }
    return true;
}

void RtpsTopologyBackend::Shutdown() {
    if (!running_.exchange(false)) {
        DestroyEndpoints();
        participant_.reset();
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_.clear();
        return;
    }

    DestroyEndpoints();
    participant_.reset();
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_.clear();
    }
}

bool RtpsTopologyBackend::CreateEndpoints() {
    auto* dp = participant_->get();
    RETURN_VAL_IF_NULL(dp, false);

    publisher_ = dp->create_publisher(PUBLISHER_QOS_DEFAULT);
    RETURN_VAL_IF_NULL(publisher_, false);

    subscriber_ = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    RETURN_VAL_IF_NULL(subscriber_, false);

    eprosima::fastdds::dds::DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    eprosima::fastdds::dds::DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    const auto& topo_qos = QosProfileConf::QOS_PROFILE_TOPO_CHANGE;
    RETURN_VAL_IF(!AttributesFiller::FillInPubQos(topo_qos, &wqos), false);
    RETURN_VAL_IF(!AttributesFiller::FillInSubQos(topo_qos, &rqos), false);

    const struct {
        const char* name;
        proto::ChangeType type;
    } specs[] = {
            {kNodeTopic, proto::ChangeType::CHANGE_NODE},
            {kChannelTopic, proto::ChangeType::CHANGE_CHANNEL},
            {kServiceTopic, proto::ChangeType::CHANGE_SERVICE},
    };

    endpoints_.clear();
    endpoints_.reserve(3);
    for (const auto& spec : specs) {
        TopicEndpoint ep;
        ep.topic_name = spec.name;
        ep.change_type = spec.type;

        auto* desc = dp->lookup_topicdescription(spec.name);
        if (desc != nullptr) {
            ep.topic = dynamic_cast<eprosima::fastdds::dds::Topic*>(desc);
        } else {
            ep.topic = dp->create_topic(spec.name, "UnderlayMessage",
                                        TOPIC_QOS_DEFAULT);
        }
        if (ep.topic == nullptr) {
            AERROR << "RtpsTopologyBackend: create_topic failed: " << spec.name;
            return false;
        }

        ep.writer = publisher_->create_datawriter(ep.topic, wqos);
        if (ep.writer == nullptr) {
            AERROR << "RtpsTopologyBackend: create_datawriter failed: "
                   << spec.name;
            return false;
        }

        ep.listener = std::make_shared<TopologyReaderListener>(this);
        ep.reader = subscriber_->create_datareader(
                ep.topic, rqos, ep.listener.get(),
                eprosima::fastdds::dds::StatusMask::all());
        if (ep.reader == nullptr) {
            AERROR << "RtpsTopologyBackend: create_datareader failed: "
                   << spec.name;
            return false;
        }

        endpoints_.push_back(std::move(ep));
    }
    return true;
}

void RtpsTopologyBackend::DestroyEndpoints() {
    auto* dp = participant_ != nullptr ? participant_->get() : nullptr;

    for (auto& ep : endpoints_) {
        if (subscriber_ != nullptr && ep.reader != nullptr) {
            subscriber_->delete_datareader(ep.reader);
        }
        if (publisher_ != nullptr && ep.writer != nullptr) {
            publisher_->delete_datawriter(ep.writer);
        }
        ep.reader = nullptr;
        ep.writer = nullptr;
        ep.listener.reset();
        // Topics may be shared; leave for participant teardown.
        ep.topic = nullptr;
    }
    endpoints_.clear();

    if (dp != nullptr) {
        if (subscriber_ != nullptr) {
            dp->delete_subscriber(subscriber_);
        }
        if (publisher_ != nullptr) {
            dp->delete_publisher(publisher_);
        }
    }
    subscriber_ = nullptr;
    publisher_ = nullptr;
}

RtpsTopologyBackend::TopicEndpoint* RtpsTopologyBackend::EndpointFor(
        proto::ChangeType type) {
    for (auto& ep : endpoints_) {
        if (ep.change_type == type) {
            return &ep;
        }
    }
    return nullptr;
}

void RtpsTopologyBackend::OnUnderlaySample(const std::string& datatype,
                                           const std::string& payload) {
    if (!running_.load()) {
        return;
    }
    if (!datatype.empty() && datatype != kChangeMsgDatatype) {
        return;
    }

    proto::ChangeMsg msg;
    if (!msg.ParseFromString(payload)) {
        AERROR << "RtpsTopologyBackend: failed to parse ChangeMsg";
        return;
    }
    if (IsFromSelf(msg)) {
        return;
    }
    DispatchMessage(msg);
}

void RtpsTopologyBackend::DispatchMessage(const proto::ChangeMsg& msg) {
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        for (const auto& entry : subscribers_) {
            if (entry.second.type == msg.change_type()) {
                callbacks.emplace_back(entry.second.callback);
            }
        }
    }
    for (const auto& callback : callbacks) {
        if (callback) {
            callback(msg);
        }
    }
}

bool RtpsTopologyBackend::IsFromSelf(const proto::ChangeMsg& msg) const {
    const auto& attr = msg.role_attr();
    return attr.host_name() == self_host_name_ &&
           attr.process_id() == self_process_id_;
}

}  // namespace service_discovery
}  // namespace autolink
