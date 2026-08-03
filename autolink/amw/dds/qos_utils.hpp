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

#pragma once

#include <algorithm>
#include <cstdint>

#include "autolink/common/global_data.hpp"
#include "autolink/proto/qos_profile.pb.h"
#include "autolink/transport/qos/qos_profile_conf.hpp"

namespace autolink {
namespace amw {

// Resolve *_SYSTEM_DEFAULT enums via QosProfileConf defaults.
inline proto::QosProfile ResolveQos(const proto::QosProfile& profile) {
    using transport::QosProfileConf;
    proto::QosProfile resolved = profile;
    if (resolved.reliability() ==
        proto::QosReliabilityPolicy::RELIABILITY_SYSTEM_DEFAULT) {
        resolved.set_reliability(
            QosProfileConf::QOS_PROFILE_DEFAULT.reliability());
    }
    if (resolved.history() == proto::QosHistoryPolicy::HISTORY_SYSTEM_DEFAULT) {
        resolved.set_history(QosProfileConf::QOS_PROFILE_DEFAULT.history());
        if (resolved.depth() == 0) {
            resolved.set_depth(QosProfileConf::QOS_PROFILE_DEFAULT.depth());
        }
    }
    if (resolved.durability() ==
        proto::QosDurabilityPolicy::DURABILITY_SYSTEM_DEFAULT) {
        resolved.set_durability(
            QosProfileConf::QOS_PROFILE_DEFAULT.durability());
    }
    return resolved;
}

// KEEP_LAST depth after mps / transport resource_limit caps.
inline int32_t ResolveKeepLastDepth(const proto::QosProfile& profile) {
    int32_t depth =
        profile.depth() == 0 ? 10 : static_cast<int32_t>(profile.depth());
    if (profile.mps() > 0) {
        depth = std::min(depth,
                         std::max<int32_t>(1, static_cast<int32_t>(profile.mps())));
    }
    const auto& conf = common::GlobalData::Instance()->Config();
    if (conf.has_transport_conf() &&
        conf.transport_conf().has_resource_limit()) {
        const uint32_t max_depth =
            conf.transport_conf().resource_limit().max_history_depth();
        if (max_depth > 0) {
            depth = std::min(depth, static_cast<int32_t>(max_depth));
        }
    }
    return std::max<int32_t>(1, depth);
}

// Sample budget for KEEP_ALL (late-joiner history).
inline int32_t ResolveKeepAllSampleBudget(const proto::QosProfile& profile) {
    int32_t budget = 1024;
    if (profile.depth() > 0) {
        budget = static_cast<int32_t>(profile.depth());
    }
    const auto& conf = common::GlobalData::Instance()->Config();
    if (conf.has_transport_conf() &&
        conf.transport_conf().has_resource_limit()) {
        const uint32_t max_depth =
            conf.transport_conf().resource_limit().max_history_depth();
        if (max_depth > 0) {
            budget = std::min(budget, static_cast<int32_t>(max_depth));
        }
    }
    return std::max<int32_t>(1, budget);
}

}  // namespace amw
}  // namespace autolink
