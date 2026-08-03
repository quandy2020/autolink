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

#ifdef AUTOLINK_ENABLE_FASTDDS

#include <cstdint>
#include <string>
#include <vector>

#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/rtps/common/SerializedPayload.hpp>
#include <fastdds/rtps/common/InstanceHandle.hpp>

namespace autolink {
namespace amw {

// Upper bound for serialized AutolinkRawSample (must match PubSubType).
constexpr uint32_t kAutolinkRawMaxSerializedSize = 256 * 1024;

struct AutolinkRawSample {
    std::string payload;
    uint64_t seq_num = 0;
    uint64_t sender_id = 0;
};

class AutolinkRawPubSubType
    : public eprosima::fastdds::dds::TopicDataType
{
public:
    AutolinkRawPubSubType();

    bool serialize(
        const void* const data,
        eprosima::fastdds::rtps::SerializedPayload_t& payload,
        eprosima::fastdds::dds::DataRepresentationId_t data_representation)
        override;

    bool deserialize(eprosima::fastdds::rtps::SerializedPayload_t& payload,
                     void* data) override;

    uint32_t calculate_serialized_size(
        const void* const data,
        eprosima::fastdds::dds::DataRepresentationId_t data_representation)
        override;

    void* create_data() override;
    void delete_data(void* data) override;

    bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t& payload,
                     eprosima::fastdds::rtps::InstanceHandle_t& ihandle,
                     bool force_md5 = false) override;
    bool compute_key(const void* const data,
                     eprosima::fastdds::rtps::InstanceHandle_t& ihandle,
                     bool force_md5 = false) override;
};

}  // namespace amw
}  // namespace autolink

#endif  // AUTOLINK_ENABLE_FASTDDS
