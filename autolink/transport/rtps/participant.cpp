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

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"
#include "autolink/transport/rtps/security_config.hpp"
// UnderlayMessageType lives in underlay_message_type (Task 3). Keep this TU
// free of that header so Participant can compile against Fast DDS 3.x first.
#include "fastdds/LibrarySettings.hpp"
#include "fastdds/dds/core/ReturnCode.hpp"
#include "fastdds/dds/domain/DomainParticipantFactory.hpp"
#include "fastdds/dds/domain/qos/DomainParticipantQos.hpp"
#include "fastdds/rtps/attributes/RTPSParticipantAttributes.hpp"
#include "fastdds/rtps/common/Locator.hpp"
#include "fastdds/rtps/common/LocatorList.hpp"
#include "fastdds/rtps/transport/UDPv4TransportDescriptor.hpp"
#include "fastdds/utils/IPLocator.hpp"

namespace eprosima {
namespace fastdds {
namespace rtps {

// Exported by libfastdds; public header was removed in 3.x (internal only).
bool load_environment_server_info(const std::string& list,
                                  LocatorList& servers_list);

}  // namespace rtps
}  // namespace fastdds
}  // namespace eprosima

namespace autolink {
namespace transport {

// Implemented in underlay_message_type.cpp (Task 3 migration).
eprosima::fastdds::dds::TypeSupport MakeUnderlayTypeSupport();

namespace {
using eprosima::fastdds::LibrarySettings;
using eprosima::fastdds::INTRAPROCESS_FULL;
using eprosima::fastdds::dds::DomainParticipantFactory;
using eprosima::fastdds::dds::DomainParticipantQos;
using eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;
using eprosima::fastdds::dds::RETCODE_OK;
using eprosima::fastdds::rtps::DiscoveryProtocol;
using eprosima::fastdds::rtps::IPLocator;
using eprosima::fastdds::rtps::LocatorList;
using eprosima::fastdds::rtps::Locator_t;
using eprosima::fastdds::rtps::load_environment_server_info;
}  // namespace

Participant::Participant(const proto::RtpsParticipantAttr& attr,
                         ParticipantRole role,
                         const std::vector<std::string>& discovery_servers)
    : attr_(attr),
      role_(role),
      discovery_servers_(discovery_servers),
      type_(MakeUnderlayTypeSupport()) {
    auto* gd = common::GlobalData::Instance();
    domain_id_ = gd->DomainId();
    host_ip_ = gd->HostIp();
    name_ = gd->HostName() + std::to_string(gd->ProcessId());
    name_ += (role_ == ParticipantRole::kTopology) ? ":topology" : ":transport";
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
        LibrarySettings ls;
        ls.intraprocess_delivery = INTRAPROCESS_FULL;
        DomainParticipantFactory::get_instance()->set_library_settings(ls);
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

    wire.builtin.discovery_config.leaseDuration.seconds = attr_.lease_duration();
    wire.builtin.discovery_config.leaseDuration_announcementperiod.seconds =
            attr_.announcement_period();
    wire.builtin.discovery_config.initial_announcements.count = 5;
    wire.builtin.discovery_config.initial_announcements.period.seconds = 0;
    wire.builtin.discovery_config.initial_announcements.period.nanosec =
            100000000u;

    if (discovery_servers_.empty()) {
        wire.builtin.discovery_config.discoveryProtocol =
                DiscoveryProtocol::SIMPLE;
        wire.builtin.discovery_config.use_SIMPLE_EndpointDiscoveryProtocol =
                true;
        wire.builtin.discovery_config.m_simpleEDP
                .use_PublicationReaderANDSubscriptionWriter = true;
        wire.builtin.discovery_config.m_simpleEDP
                .use_PublicationWriterANDSubscriptionReader = true;
    } else {
        // Comma-separated env entries → Fast DDS semicolon list (ROS DS format).
        std::ostringstream joined;
        for (size_t i = 0; i < discovery_servers_.size(); ++i) {
            if (i > 0) {
                joined << ';';
            }
            joined << discovery_servers_[i];
        }
        LocatorList servers;
        if (!load_environment_server_info(joined.str(), servers) ||
            servers.empty()) {
            AERROR << "Participant Init failed: invalid discovery servers="
                   << joined.str();
            return false;
        }
        wire.builtin.discovery_config.discoveryProtocol =
                DiscoveryProtocol::CLIENT;
        wire.builtin.discovery_config.m_DiscoveryServers = servers;
    }

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

    if (discovery_servers_.empty()) {
        Locator_t multicast;
        multicast.port = 0;
        IPLocator::setIPv4(multicast, 239, 255, 0, 1);
        wire.builtin.metatrafficMulticastLocatorList.push_back(multicast);
    }

    {
        auto sec = SecurityConfig::FromEnv();
        if (sec.enabled) {
            auto file_uri = [](const std::string& path) {
                namespace fs = std::filesystem;
                std::error_code ec;
                const fs::path abs = fs::absolute(path, ec);
                const std::string resolved =
                        ec ? path : abs.lexically_normal().string();
                return std::string("file://") + resolved;
            };
            auto& props = qos.properties().properties();
            props.emplace_back("dds.sec.auth.plugin", "builtin.PKI-DH");
            props.emplace_back(
                    "dds.sec.auth.builtin.PKI-DH.identity_ca",
                    file_uri(sec.Path(SecurityConfig::kIdentityCa)));
            props.emplace_back(
                    "dds.sec.auth.builtin.PKI-DH.identity_certificate",
                    file_uri(sec.Path(SecurityConfig::kCert)));
            props.emplace_back("dds.sec.auth.builtin.PKI-DH.private_key",
                               file_uri(sec.Path(SecurityConfig::kKey)));
            props.emplace_back("dds.sec.access.plugin",
                               "builtin.Access-Permissions");
            props.emplace_back(
                    "dds.sec.access.builtin.Access-Permissions.permissions_ca",
                    file_uri(sec.Path(SecurityConfig::kPermissionsCa)));
            props.emplace_back(
                    "dds.sec.access.builtin.Access-Permissions.governance",
                    file_uri(sec.Path(SecurityConfig::kGovernance)));
            props.emplace_back(
                    "dds.sec.access.builtin.Access-Permissions.permissions",
                    file_uri(sec.Path(SecurityConfig::kPermissions)));
            props.emplace_back("dds.sec.crypto.plugin",
                               "builtin.AES-GCM-GMAC");
        }
    }

    participant_ = DomainParticipantFactory::get_instance()->create_participant(
            domain_id_, qos);
    if (participant_ == nullptr) {
        AERROR << "Participant Init failed: create_participant domain="
               << domain_id_ << " ip=" << host_ip_;
        return false;
    }

    if (type_.register_type(participant_) != RETCODE_OK) {
        AERROR << "Participant Init failed: register UnderlayMessage type "
                  "domain="
               << domain_id_ << " ip=" << host_ip_;
        DomainParticipantFactory::get_instance()->delete_participant(
                participant_);
        participant_ = nullptr;
        return false;
    }

    AINFO << "Participant ready name=" << name_ << " domain=" << domain_id_
          << " ip=" << host_ip_
          << (discovery_servers_.empty() ? " discovery=SIMPLE"
                                         : " discovery=CLIENT");
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
