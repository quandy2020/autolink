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

#include "autolink/amw/dds/fastdds_raw_type.hpp"

#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>
#include <fastdds/dds/topic/TypeSupport.hpp>

namespace autolink {
namespace amw {

namespace {
constexpr const char* kTypeName = "autolink::amw::AutolinkRawSample";
}  // namespace

AutolinkRawPubSubType::AutolinkRawPubSubType() {
    set_name(kTypeName);
    // FastDDS payload pool requires a finite upper bound (keep modest).
    max_serialized_type_size = kAutolinkRawMaxSerializedSize;
    is_compute_key_provided = false;
}

bool AutolinkRawPubSubType::serialize(
    const void* const data,
    eprosima::fastdds::rtps::SerializedPayload_t& payload,
    eprosima::fastdds::dds::DataRepresentationId_t data_representation) {
    const auto* sample = static_cast<const AutolinkRawSample*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
        reinterpret_cast<char*>(payload.data), payload.max_size);
    eprosima::fastcdr::Cdr ser(
        fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
        data_representation ==
                eprosima::fastdds::dds::DataRepresentationId_t::
                    XCDR_DATA_REPRESENTATION
            ? eprosima::fastcdr::CdrVersion::XCDRv1
            : eprosima::fastcdr::CdrVersion::XCDRv2);
    payload.encapsulation =
        ser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE
                                                                  : CDR_LE;
    try {
        ser.serialize_encapsulation();
        ser << sample->payload;
        ser << sample->seq_num;
        ser << sample->sender_id;
    } catch (const eprosima::fastcdr::exception::Exception&) {
        return false;
    }
    payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
    return true;
}

bool AutolinkRawPubSubType::deserialize(
    eprosima::fastdds::rtps::SerializedPayload_t& payload, void* data) {
    auto* sample = static_cast<AutolinkRawSample*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
        reinterpret_cast<char*>(payload.data), payload.length);
    eprosima::fastcdr::Cdr deser(fastbuffer,
                                 eprosima::fastcdr::Cdr::DEFAULT_ENDIAN);
    try {
        deser.read_encapsulation();
        deser >> sample->payload;
        deser >> sample->seq_num;
        deser >> sample->sender_id;
    } catch (const eprosima::fastcdr::exception::Exception&) {
        return false;
    }
    return true;
}

uint32_t AutolinkRawPubSubType::calculate_serialized_size(
    const void* const data,
    eprosima::fastdds::dds::DataRepresentationId_t /*data_representation*/) {
    const auto* sample = static_cast<const AutolinkRawSample*>(data);
    // encapsulation(4) + string length(4) + bytes + padding + 2*uint64
    return static_cast<uint32_t>(4 + 4 + sample->payload.size() + 8 + 16 + 64);
}

void* AutolinkRawPubSubType::create_data() {
    return new AutolinkRawSample();
}

void AutolinkRawPubSubType::delete_data(void* data) {
    delete static_cast<AutolinkRawSample*>(data);
}

bool AutolinkRawPubSubType::compute_key(
    eprosima::fastdds::rtps::SerializedPayload_t& /*payload*/,
    eprosima::fastdds::rtps::InstanceHandle_t& /*ihandle*/,
    bool /*force_md5*/) {
    return false;
}

bool AutolinkRawPubSubType::compute_key(
    const void* const /*data*/,
    eprosima::fastdds::rtps::InstanceHandle_t& /*ihandle*/, bool /*force_md5*/) {
    return false;
}

}  // namespace amw
}  // namespace autolink

#endif  // AUTOLINK_ENABLE_FASTDDS
