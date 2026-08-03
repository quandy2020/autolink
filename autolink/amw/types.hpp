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
#include <cctype>
#include <cstdint>
#include <string>

#include "autolink/proto/role_attributes.pb.h"
#include "autolink/proto/transport_conf.pb.h"

namespace autolink {
namespace amw {

// Network DDS vendors selectable like ROS 2 RMW implementations.
enum class ProviderId : int {
    kLocal = 0,
    kFastDds = 1,
    kCycloneDds = 2,
    kOpenDds = 3,
    kConnextDds = 4,  // RTI Connext DDS ("ContextDDS" typo maps here)
};

enum class Relation : int {
    kSameProc = 0,
    kDiffProc = 1,
    kDiffHost = 2,
    kNoRelation = 3,
};

struct AmwQos {
    int32_t reliability = 0;
    int32_t history = 0;
    uint32_t depth = 0;
};

struct EndpointDesc {
    proto::RoleAttributes attr;
    proto::OptionalMode local_mode = proto::OptionalMode::SHM;
};

struct AmwContext {
    // Canonical short name: fastdds | cyclonedds | opendds | connext
    std::string default_network_provider = "fastdds";
    // ROS 2 style implementation id, e.g. amw_fastdds / rmw_fastrtps_cpp
    std::string implementation = "amw_fastdds";
};

inline std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

inline bool IsNetworkProvider(ProviderId id) {
    return id == ProviderId::kFastDds || id == ProviderId::kCycloneDds ||
           id == ProviderId::kOpenDds || id == ProviderId::kConnextDds;
}

inline const char* ProviderIdName(ProviderId id) {
    switch (id) {
        case ProviderId::kLocal:
            return "local";
        case ProviderId::kFastDds:
            return "fastdds";
        case ProviderId::kCycloneDds:
            return "cyclonedds";
        case ProviderId::kOpenDds:
            return "opendds";
        case ProviderId::kConnextDds:
            return "connext";
        default:
            return "unknown";
    }
}

// Canonical AMW implementation name (ROS 2 RMW_IMPLEMENTATION analogue).
inline const char* ImplementationName(ProviderId id) {
    switch (id) {
        case ProviderId::kFastDds:
            return "amw_fastdds";
        case ProviderId::kCycloneDds:
            return "amw_cyclonedds";
        case ProviderId::kOpenDds:
            return "amw_opendds";
        case ProviderId::kConnextDds:
            return "amw_connextdds";
        case ProviderId::kLocal:
            return "amw_local";
        default:
            return "amw_unknown";
    }
}

// Resolve short names, amw_*, and common ROS 2 RMW_IMPLEMENTATION values.
// Unknown names map to kLocal (caller should treat non-network as invalid for
// network selection via ResolveNetworkImplementation).
inline ProviderId ProviderIdFromName(const std::string& name_in) {
    const std::string name = ToLowerAscii(name_in);
    if (name.empty() || name == "local" || name == "amw_local") {
        return ProviderId::kLocal;
    }

    if (name == "fastdds" || name == "fast-dds" || name == "fastrtps" ||
        name == "rtps" || name == "amw_fastdds" ||
        name == "rmw_fastrtps_cpp" || name == "rmw_fastrtps_dynamic_cpp") {
        return ProviderId::kFastDds;
    }

    if (name == "cyclonedds" || name == "cyclone" || name == "amw_cyclonedds" ||
        name == "rmw_cyclonedds_cpp") {
        return ProviderId::kCycloneDds;
    }

    if (name == "opendds" || name == "amw_opendds" ||
        name == "rmw_opendds_cpp") {
        return ProviderId::kOpenDds;
    }

    // RTI Connext DDS (also accept common typo "contextdds")
    if (name == "connext" || name == "connextdds" || name == "contextdds" ||
        name == "rti" || name == "rti_connext" || name == "amw_connextdds" ||
        name == "rmw_connextdds" || name == "rmw_connextddsmicro") {
        return ProviderId::kConnextDds;
    }

    return ProviderId::kLocal;
}

// ROS 2 style network implementation resolution. Empty → FastDDS default.
inline bool ResolveNetworkImplementation(const std::string& name_in,
                                         ProviderId* out) {
    if (out == nullptr) {
        return false;
    }
    if (name_in.empty()) {
        *out = ProviderId::kFastDds;
        return true;
    }
    const ProviderId id = ProviderIdFromName(name_in);
    if (!IsNetworkProvider(id)) {
        return false;
    }
    *out = id;
    return true;
}

}  // namespace amw
}  // namespace autolink
