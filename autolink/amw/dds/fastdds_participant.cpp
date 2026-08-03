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

#ifdef AUTOLINK_ENABLE_FASTDDS

#include "autolink/amw/dds/fastdds_participant.hpp"

#include <cstdlib>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>
#include <fastdds/rtps/common/Locator.hpp>
#include <fastdds/utils/IPLocator.hpp>

#include "autolink/amw/dds/host_ip_utils.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"

namespace autolink {
namespace amw {

using eprosima::fastdds::dds::DomainParticipantFactory;
using eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;
using eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT;
using eprosima::fastdds::dds::RETCODE_OK;
using eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT;
using eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;

FastDdsParticipant::FastDdsParticipant() = default;

FastDdsParticipant::~FastDdsParticipant() {
    Shutdown();
}

uint32_t FastDdsParticipant::ResolveDomainId() const {
    const char* env = std::getenv("AUTOLINK_DOMAIN_ID");
    if (env == nullptr || env[0] == '\0') {
        env = std::getenv("ROS_DOMAIN_ID");
    }
    if (env != nullptr && env[0] != '\0') {
        return static_cast<uint32_t>(std::strtoul(env, nullptr, 10));
    }
    return 0;
}

bool FastDdsParticipant::Init() {
    if (ready_.load()) {
        return true;
    }

    domain_id_ = ResolveDomainId();
    auto* factory = DomainParticipantFactory::get_instance();
    eprosima::fastdds::dds::DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    const auto& host_ip = common::GlobalData::Instance()->HostIp();
    if (!host_ip.empty()) {
        pqos.name("autolink@" + host_ip);
    } else {
        pqos.name("autolink");
    }

    const auto& transport_conf =
        common::GlobalData::Instance()->Config().transport_conf();
    const auto& pattr = transport_conf.participant_attr();
    if (pattr.lease_duration() > 0) {
        pqos.wire_protocol().builtin.discovery_config.leaseDuration = {
            pattr.lease_duration(), 0};
    }
    if (pattr.announcement_period() > 0) {
        pqos.wire_protocol()
            .builtin.discovery_config.leaseDuration_announcementperiod = {
            pattr.announcement_period(), 0};
    }
    if (pattr.port_base() > 0) {
        pqos.wire_protocol().port.portBase =
            static_cast<uint16_t>(pattr.port_base());
    }
    if (pattr.domain_id_gain() > 0) {
        pqos.wire_protocol().port.domainIDGain =
            static_cast<uint16_t>(pattr.domain_id_gain());
    }

    // Align with Cyclone: only pin a real local NIC. Fake/sim AUTOLINK_IP
    // (e.g. 10.255.0.x in amw_sim_*.sh) must not become a default locator.
    if (!host_ip.empty() && host_ip != "127.0.0.1") {
        if (!HostIpIsLocalInterface(host_ip)) {
            AINFO << "FastDDS skip default unicast locator; host_ip="
                  << host_ip
                  << " is not a local interface (ok for same-host DIFF_HOST "
                     "sim).";
        } else {
            eprosima::fastdds::rtps::Locator_t locator;
            if (eprosima::fastdds::rtps::IPLocator::setIPv4(locator,
                                                           host_ip)) {
                locator.kind = LOCATOR_KIND_UDPv4;
                locator.port = 0;
                pqos.wire_protocol().default_unicast_locator_list.push_back(
                    locator);
                AINFO << "FastDDS default unicast locator set to " << host_ip;
            } else {
                AWARN
                    << "FastDDS failed to parse AUTOLINK_IP/host_ip as IPv4: "
                    << host_ip;
            }
        }
    }

    participant_ = factory->create_participant(domain_id_, pqos);
    if (participant_ == nullptr) {
        AERROR << "FastDDS create_participant failed (domain=" << domain_id_
               << ").";
        return false;
    }

    type_support_ =
        eprosima::fastdds::dds::TypeSupport(new AutolinkRawPubSubType());
    if (type_support_.register_type(participant_) != RETCODE_OK) {
        AERROR << "FastDDS register_type failed.";
        Shutdown();
        return false;
    }

    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);
    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    if (publisher_ == nullptr || subscriber_ == nullptr) {
        AERROR << "FastDDS create publisher/subscriber failed.";
        Shutdown();
        return false;
    }

    ready_.store(true);
    AINFO << "FastDDS participant ready domain=" << domain_id_
          << " host_ip=" << host_ip;
    return true;
}

void FastDdsParticipant::Shutdown() {
    if (!ready_.exchange(false) && participant_ == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(topic_mutex_);
    if (participant_ != nullptr) {
        for (auto& item : topics_) {
            if (item.second != nullptr) {
                participant_->delete_topic(item.second);
            }
        }
        topics_.clear();
        if (publisher_ != nullptr) {
            participant_->delete_publisher(publisher_);
            publisher_ = nullptr;
        }
        if (subscriber_ != nullptr) {
            participant_->delete_subscriber(subscriber_);
            subscriber_ = nullptr;
        }
        DomainParticipantFactory::get_instance()->delete_participant(
            participant_);
        participant_ = nullptr;
    }
}

eprosima::fastdds::dds::Topic* FastDdsParticipant::GetOrCreateTopic(
    const std::string& channel_name) {
    std::lock_guard<std::mutex> lock(topic_mutex_);
    auto it = topics_.find(channel_name);
    if (it != topics_.end()) {
        return it->second;
    }
    if (participant_ == nullptr) {
        return nullptr;
    }
    auto* topic = participant_->create_topic(
        channel_name, type_support_.get_type_name(), TOPIC_QOS_DEFAULT);
    if (topic == nullptr) {
        AERROR << "FastDDS create_topic failed: " << channel_name;
        return nullptr;
    }
    topics_[channel_name] = topic;
    return topic;
}

}  // namespace amw
}  // namespace autolink

#endif  // AUTOLINK_ENABLE_FASTDDS
