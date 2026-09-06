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
#include "fastcdr/config.h"
#include "fastcdr/exceptions/Exception.h"

namespace autolink {
namespace transport {

UnderlayMessageType::UnderlayMessageType() : m_keyBuffer(nullptr) {
    setName("UnderlayMessage");
    m_typeSize = static_cast<uint32_t>(UnderlayMessage::getMaxCdrSerializedSize()) +
                 4u /*encapsulation*/;
    m_isGetKeyDefined = UnderlayMessage::isKeyDefined();
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
        void* data, eprosima::fastrtps::rtps::SerializedPayload_t* payload) {
    if (data == nullptr || payload == nullptr) {
        return false;
    }
    UnderlayMessage* p_type = static_cast<UnderlayMessage*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
            reinterpret_cast<char*>(payload->data), payload->max_size);
#if FASTCDR_VERSION_MAJOR == 1
    eprosima::fastcdr::Cdr ser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
                               eprosima::fastcdr::Cdr::DDS_CDR);
#else
    eprosima::fastcdr::Cdr ser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
                               eprosima::fastcdr::CdrVersion::XCDRv1);
#endif
    payload->encapsulation =
            ser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE
                                                                      : CDR_LE;
    try {
        ser.serialize_encapsulation();
        p_type->serialize(ser);
    } catch (eprosima::fastcdr::exception::Exception&) {
        return false;
    }
#if FASTCDR_VERSION_MAJOR == 1
    payload->length = static_cast<uint32_t>(ser.getSerializedDataLength());
#else
    payload->length = static_cast<uint32_t>(ser.get_serialized_data_length());
#endif
    return true;
}

bool UnderlayMessageType::deserialize(
        eprosima::fastrtps::rtps::SerializedPayload_t* payload, void* data) {
    if (data == nullptr || payload == nullptr) {
        return false;
    }
    UnderlayMessage* p_type = static_cast<UnderlayMessage*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
            reinterpret_cast<char*>(payload->data), payload->length);
#if FASTCDR_VERSION_MAJOR == 1
    eprosima::fastcdr::Cdr deser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
                                 eprosima::fastcdr::Cdr::DDS_CDR);
#else
    eprosima::fastcdr::Cdr deser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
                                 eprosima::fastcdr::CdrVersion::XCDRv1);
#endif
    try {
        deser.read_encapsulation();
        payload->encapsulation =
                deser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS
                        ? CDR_BE
                        : CDR_LE;
        p_type->deserialize(deser);
    } catch (eprosima::fastcdr::exception::Exception&) {
        return false;
    }
    return true;
}

std::function<uint32_t()> UnderlayMessageType::getSerializedSizeProvider(
        void* data) {
    return [data]() -> uint32_t {
        return static_cast<uint32_t>(
                       type::getCdrSerializedSize(
                               *static_cast<UnderlayMessage*>(data))) +
               4u /*encapsulation*/;
    };
}

void* UnderlayMessageType::createData() {
    return reinterpret_cast<void*>(new UnderlayMessage());
}

void UnderlayMessageType::deleteData(void* data) {
    delete reinterpret_cast<UnderlayMessage*>(data);
}

bool UnderlayMessageType::getKey(
        void* data, eprosima::fastrtps::rtps::InstanceHandle_t* handle,
        bool force_md5) {
    (void)force_md5;
    if (!m_isGetKeyDefined || data == nullptr || handle == nullptr) {
        return false;
    }
    UnderlayMessage* p_type = static_cast<UnderlayMessage*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(
            reinterpret_cast<char*>(m_keyBuffer),
            UnderlayMessage::getKeyMaxCdrSerializedSize());
    eprosima::fastcdr::Cdr ser(fastbuffer,
                               eprosima::fastcdr::Cdr::BIG_ENDIANNESS);
    p_type->serializeKey(ser);
#if FASTCDR_VERSION_MAJOR == 1
    const auto ser_len = ser.getSerializedDataLength();
#else
    const auto ser_len = ser.get_serialized_data_length();
#endif
    if (UnderlayMessage::getKeyMaxCdrSerializedSize() > 16) {
        m_md5.init();
        m_md5.update(m_keyBuffer, static_cast<unsigned int>(ser_len));
        m_md5.finalize();
        for (uint8_t i = 0; i < 16; ++i) {
            handle->value[i] = m_md5.digest[i];
        }
    } else {
        for (uint8_t i = 0; i < 16; ++i) {
            handle->value[i] = m_keyBuffer[i];
        }
    }
    return true;
}

}  // namespace transport
}  // namespace autolink
