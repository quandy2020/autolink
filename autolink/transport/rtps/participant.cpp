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

#include "autolink/transport/rtps/participant.hpp"

#include <memory>

#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"
#include "fastdds/dds/domain/DomainParticipantFactory.hpp"
#include "fastdds/dds/domain/qos/DomainParticipantQos.hpp"
#include "fastdds/rtps/transport/UDPv4TransportDescriptor.h"
#include "fastrtps/attributes/LibrarySettingsAttributes.h"
#include "fastrtps/types/TypesBase.h"
#include "fastrtps/utils/IPLocator.h"
#include "fastrtps/xmlparser/XMLProfileManager.h"

namespace autolink {
namespace transport {

namespace {
using eprosima::fastdds::dds::DomainParticipantFactory;
using eprosima::fastdds::dds::DomainParticipantQos;
using eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;
using eprosima::fastrtps::rtps::DiscoveryProtocol_t;
using eprosima::fastrtps::rtps::IPLocator;
using eprosima::fastrtps::rtps::Locator_t;
using eprosima::fastrtps::types::ReturnCode_t;
}  // namespace

Participant::Participant(const proto::RtpsParticipantAttr& attr)
    : attr_(attr), type_(new UnderlayMessageType()) {
    auto* gd = common::GlobalData::Instance();
    domain_id_ = gd->DomainId();
    host_ip_ = gd->HostIp();
    name_ = gd->HostName() + std::to_string(gd->ProcessId());
}

Participant::~Participant() {
    Shutdown();
}

bool Participant::Init() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (shutdown_.load()) {
        return false;
    }
    if (participant_ != nullptr) {
        return true;
    }

    {
        // Same-process DataWriter/DataReader on one DomainParticipant need
        // intraprocess delivery (library default FULL). Force it explicitly so
        // behavior does not depend on discovering fastdds_profiles.xml.
        eprosima::fastrtps::LibrarySettingsAttributes ls =
                eprosima::fastrtps::xmlparser::XMLProfileManager::library_settings();
        ls.intraprocess_delivery = eprosima::fastrtps::INTRAPROCESS_FULL;
        eprosima::fastrtps::xmlparser::XMLProfileManager::library_settings(ls);
    }

    DomainParticipantQos qos = PARTICIPANT_QOS_DEFAULT;
    qos.name(name_);
    // Same DomainParticipant writer/reader must match for in-process RTPS.
    qos.properties().properties().emplace_back("fastdds.ignore_local_endpoints",
                                               "false");
    // Prefer UDPv4 only: builtin SharedMem transport can match endpoints on
    // macOS without delivering user samples to DataReaderListener.
    qos.transport().use_builtin_transports = false;
    auto udp_transport =
            std::make_shared<eprosima::fastdds::rtps::UDPv4TransportDescriptor>();
    qos.transport().user_transports.push_back(udp_transport);

    auto& wire = qos.wire_protocol();
    wire.port.domainIDGain = static_cast<uint16_t>(attr_.domain_id_gain());
    wire.port.portBase = static_cast<uint16_t>(attr_.port_base());

    wire.builtin.discovery_config.discoveryProtocol =
            DiscoveryProtocol_t::SIMPLE;
    wire.builtin.discovery_config.use_SIMPLE_EndpointDiscoveryProtocol = true;
    wire.builtin.discovery_config.m_simpleEDP
            .use_PublicationReaderANDSubscriptionWriter = true;
    wire.builtin.discovery_config.m_simpleEDP
            .use_PublicationWriterANDSubscriptionReader = true;
    wire.builtin.discovery_config.leaseDuration.seconds = attr_.lease_duration();
    wire.builtin.discovery_config.leaseDuration_announcementperiod.seconds =
            attr_.announcement_period();
    wire.builtin.discovery_config.initial_announcements.count = 5;
    wire.builtin.discovery_config.initial_announcements.period.seconds = 0;
    wire.builtin.discovery_config.initial_announcements.period.nanosec =
            100000000u;

    Locator_t unicast;
    unicast.port = 0;
    if (!IPLocator::setIPv4(unicast, host_ip_)) {
        AERROR << "Participant Init failed: invalid host ip=" << host_ip_
               << " domain=" << domain_id_;
        return false;
    }
    wire.default_unicast_locator_list.push_back(unicast);
    wire.builtin.metatrafficUnicastLocatorList.push_back(unicast);
    // Also advertise loopback for same-host / same-process UDP delivery.
    if (host_ip_ != "127.0.0.1") {
        Locator_t loopback;
        loopback.port = 0;
        IPLocator::setIPv4(loopback, "127.0.0.1");
        wire.default_unicast_locator_list.push_back(loopback);
        wire.builtin.metatrafficUnicastLocatorList.push_back(loopback);
    }

    Locator_t multicast;
    multicast.port = 0;
    IPLocator::setIPv4(multicast, 239, 255, 0, 1);
    wire.builtin.metatrafficMulticastLocatorList.push_back(multicast);

    participant_ = DomainParticipantFactory::get_instance()->create_participant(
            domain_id_, qos);
    if (participant_ == nullptr) {
        AERROR << "Participant Init failed: create_participant domain="
               << domain_id_ << " ip=" << host_ip_;
        return false;
    }

    if (type_.register_type(participant_) != ReturnCode_t::RETCODE_OK) {
        AERROR << "Participant Init failed: register UnderlayMessage type "
                  "domain="
               << domain_id_ << " ip=" << host_ip_;
        DomainParticipantFactory::get_instance()->delete_participant(
                participant_);
        participant_ = nullptr;
        return false;
    }

    AINFO << "Participant ready name=" << name_ << " domain=" << domain_id_
          << " ip=" << host_ip_;
    return true;
}

void Participant::Shutdown() {
    if (shutdown_.exchange(true)) {
        return;
    }
    std::lock_guard<std::mutex> lk(mutex_);
    if (participant_ != nullptr) {
        DomainParticipantFactory::get_instance()->delete_participant(
                participant_);
        participant_ = nullptr;
    }
}

eprosima::fastdds::dds::DomainParticipant* Participant::get() {
    if (shutdown_.load()) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mutex_);
    return participant_;
}

}  // namespace transport
}  // namespace autolink
