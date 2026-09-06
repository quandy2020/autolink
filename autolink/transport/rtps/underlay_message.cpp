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

#include "autolink/transport/rtps/underlay_message.hpp"

namespace autolink {
namespace transport {

UnderlayMessage::UnderlayMessage() : m_timestamp(0), m_seq(0) {}

UnderlayMessage::~UnderlayMessage() {}

UnderlayMessage::UnderlayMessage(const UnderlayMessage& x)
    : m_timestamp(x.m_timestamp),
      m_seq(x.m_seq),
      m_data(x.m_data),
      m_datatype(x.m_datatype) {}

UnderlayMessage::UnderlayMessage(UnderlayMessage&& x)
    : m_timestamp(x.m_timestamp),
      m_seq(x.m_seq),
      m_data(std::move(x.m_data)),
      m_datatype(std::move(x.m_datatype)) {}

UnderlayMessage& UnderlayMessage::operator=(const UnderlayMessage& x) {
    m_timestamp = x.m_timestamp;
    m_seq = x.m_seq;
    m_data = x.m_data;
    m_datatype = x.m_datatype;
    return *this;
}

UnderlayMessage& UnderlayMessage::operator=(UnderlayMessage&& x) {
    m_timestamp = x.m_timestamp;
    m_seq = x.m_seq;
    m_data = std::move(x.m_data);
    m_datatype = std::move(x.m_datatype);
    return *this;
}

size_t UnderlayMessage::getMaxCdrSerializedSize(size_t current_alignment) {
    size_t initial_alignment = current_alignment;

    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);
    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);
    // length-prefixed string with reserved max 255 (+ null) for type size.
    // Actual payloads use getSerializedSizeProvider at write time.
    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) + 255 +
            1;
    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) + 255 +
            1;

    return current_alignment - initial_alignment;
}

size_t UnderlayMessage::getCdrSerializedSize(const UnderlayMessage& data,
                                             size_t current_alignment) {
    size_t initial_alignment = current_alignment;

    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);
    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);
    current_alignment += 4 +
                         eprosima::fastcdr::Cdr::alignment(current_alignment, 4) +
                         data.data().size() + 1;
    current_alignment +=
            4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) +
            data.datatype().size() + 1;

    return current_alignment - initial_alignment;
}

void UnderlayMessage::serialize(eprosima::fastcdr::Cdr& scdr) const {
    scdr << m_timestamp;
    scdr << m_seq;
    // FastCDR 2 rejects operator<<(std::string) when the payload embeds NULs
    // (24B MessageInfo prefix is binary). Encode as a CDR string by size:
    // uint32 length (including trailing NUL) + bytes + NUL.
    const uint32_t cdr_str_len =
            static_cast<uint32_t>(m_data.size()) + 1u;
    scdr << cdr_str_len;
    if (!m_data.empty()) {
        scdr.serialize_array(m_data.data(), m_data.size());
    }
    const char nul = '\0';
    scdr.serialize_array(&nul, 1u);
    scdr << m_datatype;
}

void UnderlayMessage::deserialize(eprosima::fastcdr::Cdr& dcdr) {
    dcdr >> m_timestamp;
    dcdr >> m_seq;
    dcdr >> m_data;
    dcdr >> m_datatype;
}

size_t UnderlayMessage::getKeyMaxCdrSerializedSize(size_t current_alignment) {
    return current_alignment;
}

bool UnderlayMessage::isKeyDefined() {
    return false;
}

void UnderlayMessage::serializeKey(eprosima::fastcdr::Cdr& scdr) const {
    (void)scdr;
}

}  // namespace transport
}  // namespace autolink
