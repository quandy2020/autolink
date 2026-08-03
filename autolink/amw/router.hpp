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

#include <string>

#include "autolink/amw/types.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/proto/transport_conf.pb.h"

namespace autolink {
namespace amw {

struct RouteDecision {
    ProviderId provider = ProviderId::kLocal;
    proto::OptionalMode mode = proto::OptionalMode::SHM;
};

class Router
{
public:
    Router() = default;

    void SetCommunicationMode(const proto::CommunicationMode& mode);
    void SetDefaultNetworkProvider(ProviderId provider);

    Relation GetRelation(const proto::RoleAttributes& self,
                         const proto::RoleAttributes& opposite) const;

    RouteDecision Resolve(Relation relation) const;
    RouteDecision Resolve(const proto::RoleAttributes& self,
                          const proto::RoleAttributes& opposite) const;

    ProviderId ProviderForMode(proto::OptionalMode mode) const;
    proto::OptionalMode ModeForProvider(ProviderId id,
                                        Relation relation) const;

private:
    proto::CommunicationMode communication_mode_;
    ProviderId default_network_provider_ = ProviderId::kFastDds;
    bool has_communication_mode_ = false;
};

}  // namespace amw
}  // namespace autolink
