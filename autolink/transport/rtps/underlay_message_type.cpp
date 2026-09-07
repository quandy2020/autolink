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

#include "autolink/transport/rtps/underlay_message_type.hpp"

#include <cstdlib>

#include "fastcdr/Cdr.h"
#include "fastcdr/FastBuffer.h"
#include "fastcdr/exceptions/Exception.h"

namespace autolink {
namespace transport {

namespace {
using eprosima::fastdds::dds::DataRepresentationId_t;
using eprosima::fastdds::rtps::InstanceHandle_t;
using eprosima::fastdds::rtps::SerializedPayload_t;
}  // namespace

UnderlayMessageType::UnderlayMessageType() : m_keyBuffer(nullptr) {
    set_name("UnderlayMessage");
    max_serialized_type_size =
            static_cast<uint32_t>(UnderlayMessage::getMaxCdrSerializedSize()) +
            4u /*encapsulation*/;
    is_compute_key_provided = UnderlayMessage::isKeyDefined();
    const size_t key_size = UnderlayMessage::getKeyMaxCdrSerializedSize();
    m_keyBuffer = static_cast<unsigned char*>(malloc(key_size > 16 ? key_size : 16));
}

UnderlayMessageType::~UnderlayMessageType() {
    if (m_keyBuffer != nullptr) {
        free(m_keyBuffer);
        m_keyBuffer = nullptr;
    }
}

bool UnderlayMessageType::serialize(
        const void* const data, SerializedPayload_t& payload,
        DataRepresentationId_t data_representation) {
    if (data == nullptr) {
        return false;
    }
    const UnderlayMessage* p_type = static_cast<const UnderlayMessage*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
            reinterpret_cast<char*>(payload.data), payload.max_size);
    eprosima::fastcdr::Cdr ser(
            fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
            data_representation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::CdrVersion::XCDRv1
                    : eprosima::fastcdr::CdrVersion::XCDRv2);
    payload.encapsulation =
            ser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE
                                                                      : CDR_LE;
    ser.set_encoding_flag(
            data_representation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR
                    : eprosima::fastcdr::EncodingAlgorithmFlag::DELIMIT_CDR2);
    try {
        ser.serialize_encapsulation();
        p_type->serialize(ser);
        ser.set_dds_cdr_options({0, 0});
    } catch (eprosima::fastcdr::exception::Exception&) {
        return false;
    }
    payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
    return true;
}

bool UnderlayMessageType::deserialize(SerializedPayload_t& payload,
                                      void* data) {
    if (data == nullptr) {
        return false;
    }
    UnderlayMessage* p_type = static_cast<UnderlayMessage*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
            reinterpret_cast<char*>(payload.data), payload.length);
    eprosima::fastcdr::Cdr deser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
                                 eprosima::fastcdr::CdrVersion::XCDRv1);
    try {
        deser.read_encapsulation();
        payload.encapsulation =
                deser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS
                        ? CDR_BE
                        : CDR_LE;
        p_type->deserialize(deser);
    } catch (eprosima::fastcdr::exception::Exception&) {
        return false;
    }
    return true;
}

uint32_t UnderlayMessageType::calculate_serialized_size(
        const void* const data, DataRepresentationId_t data_representation) {
    (void)data_representation;
    if (data == nullptr) {
        return 0;
    }
    return static_cast<uint32_t>(type::getCdrSerializedSize(
                   *static_cast<const UnderlayMessage*>(data))) +
           4u /*encapsulation*/;
}

void* UnderlayMessageType::create_data() {
    return reinterpret_cast<void*>(new UnderlayMessage());
}

void UnderlayMessageType::delete_data(void* data) {
    delete reinterpret_cast<UnderlayMessage*>(data);
}

bool UnderlayMessageType::compute_key(SerializedPayload_t& payload,
                                      InstanceHandle_t& handle,
                                      bool force_md5) {
    if (!is_compute_key_provided) {
        (void)payload;
        (void)handle;
        (void)force_md5;
        return false;
    }
    UnderlayMessage sample;
    if (!deserialize(payload, &sample)) {
        return false;
    }
    return compute_key(static_cast<const void*>(&sample), handle, force_md5);
}

bool UnderlayMessageType::compute_key(const void* const data,
                                      InstanceHandle_t& handle,
                                      bool force_md5) {
    (void)force_md5;
    if (!is_compute_key_provided || data == nullptr) {
        return false;
    }
    const UnderlayMessage* p_type = static_cast<const UnderlayMessage*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
            reinterpret_cast<char*>(m_keyBuffer),
            UnderlayMessage::getKeyMaxCdrSerializedSize());
    eprosima::fastcdr::Cdr ser(fastbuffer,
                               eprosima::fastcdr::Cdr::BIG_ENDIANNESS);
    p_type->serializeKey(ser);
    const auto ser_len = ser.get_serialized_data_length();
    if (UnderlayMessage::getKeyMaxCdrSerializedSize() > 16) {
        m_md5.init();
        m_md5.update(m_keyBuffer, static_cast<unsigned int>(ser_len));
        m_md5.finalize();
        for (uint8_t i = 0; i < 16; ++i) {
            handle.value[i] = m_md5.digest[i];
        }
    } else {
        for (uint8_t i = 0; i < 16; ++i) {
            handle.value[i] = m_keyBuffer[i];
        }
    }
    return true;
}

eprosima::fastdds::dds::TypeSupport MakeUnderlayTypeSupport() {
    return eprosima::fastdds::dds::TypeSupport(new UnderlayMessageType());
}

}  // namespace transport
}  // namespace autolink
