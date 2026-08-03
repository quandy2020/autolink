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

#include "autolink/amw/router.hpp"

namespace autolink {
namespace amw {

void Router::SetCommunicationMode(const proto::CommunicationMode& mode) {
    communication_mode_ = mode;
    has_communication_mode_ = true;
}

void Router::SetDefaultNetworkProvider(ProviderId provider) {
    default_network_provider_ = provider;
}

Relation Router::GetRelation(const proto::RoleAttributes& self,
                             const proto::RoleAttributes& opposite) const {
    if (opposite.channel_name() != self.channel_name() &&
        !opposite.channel_name().empty() && !self.channel_name().empty()) {
        // Relation is still valid for host/proc comparison even if channel
        // differs; callers that care about channel should check separately.
    }
    if (!opposite.host_ip().empty() && !self.host_ip().empty() &&
        opposite.host_ip() != self.host_ip()) {
        return Relation::kDiffHost;
    }
    if (opposite.process_id() != 0 && self.process_id() != 0 &&
        opposite.process_id() != self.process_id()) {
        return Relation::kDiffProc;
    }
    return Relation::kSameProc;
}

RouteDecision Router::Resolve(Relation relation) const {
    RouteDecision decision;
    proto::OptionalMode configured = proto::OptionalMode::SHM;

    if (has_communication_mode_) {
        switch (relation) {
            case Relation::kSameProc:
                configured = communication_mode_.same_proc();
                break;
            case Relation::kDiffProc:
                configured = communication_mode_.diff_proc();
                break;
            case Relation::kDiffHost:
                configured = communication_mode_.diff_host();
                break;
            default:
                configured = proto::OptionalMode::SHM;
                break;
        }
    } else {
        switch (relation) {
            case Relation::kSameProc:
                configured = proto::OptionalMode::INTRA;
                break;
            case Relation::kDiffProc:
                configured = proto::OptionalMode::SHM;
                break;
            case Relation::kDiffHost:
                configured = proto::OptionalMode::RTPS;
                break;
            default:
                configured = proto::OptionalMode::SHM;
                break;
        }
    }

    decision.mode = configured;
    decision.provider = ProviderForMode(configured);
    return decision;
}

RouteDecision Router::Resolve(const proto::RoleAttributes& self,
                              const proto::RoleAttributes& opposite) const {
    return Resolve(GetRelation(self, opposite));
}

ProviderId Router::ProviderForMode(proto::OptionalMode mode) const {
    switch (mode) {
        case proto::OptionalMode::INTRA:
        case proto::OptionalMode::SHM:
            return ProviderId::kLocal;
        case proto::OptionalMode::RTPS:
            return default_network_provider_;
        case proto::OptionalMode::HYBRID:
            return ProviderId::kLocal;
        default:
            return ProviderId::kLocal;
    }
}

proto::OptionalMode Router::ModeForProvider(ProviderId id,
                                            Relation relation) const {
    if (id == ProviderId::kLocal) {
        return relation == Relation::kSameProc ? proto::OptionalMode::INTRA
                                               : proto::OptionalMode::SHM;
    }
    return proto::OptionalMode::RTPS;
}

}  // namespace amw
}  // namespace autolink
