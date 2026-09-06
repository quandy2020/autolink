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

#include "autolink/service_discovery/topology_backend_factory.hpp"

#include "autolink/common/log.hpp"
#if AUTOLINK_ENABLE_FASTDDS
#include "autolink/service_discovery/rtps_topology_backend.hpp"
#endif

namespace autolink {
namespace service_discovery {

std::shared_ptr<ITopologyBackend> TopologyBackendFactory::Create(
    const std::string& name) {
    std::string key = name.empty() ? "local" : name;
    if (key == "local") {
        return std::make_shared<LocalTopologyBackend>();
    }
    if (key == "rtps") {
#if AUTOLINK_ENABLE_FASTDDS
        return std::make_shared<RtpsTopologyBackend>();
#else
        AERROR << "AUTOLINK_TOPOLOGY_BACKEND=rtps requires "
                  "AUTOLINK_ENABLE_FASTDDS";
        return std::make_shared<LocalTopologyBackend>();
#endif
    }
    AWARN << "Topology backend '" << key
          << "' unavailable; falling back to local file backend.";
    return std::make_shared<LocalTopologyBackend>();
}

}  // namespace service_discovery
}  // namespace autolink
