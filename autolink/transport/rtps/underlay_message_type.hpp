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

#pragma once

#include <functional>

#include "autolink/transport/rtps/underlay_message.hpp"
#include "fastdds/dds/topic/TopicDataType.hpp"
#include "fastrtps/rtps/common/InstanceHandle.h"
#include "fastrtps/rtps/common/SerializedPayload.h"
#include "fastrtps/utils/md5.h"

namespace autolink {
namespace transport {

/**
 * TopicDataType / TypeSupport for UnderlayMessage (Fast DDS 2.14).
 * SerializedPayload lives under the historical fastrtps namespace.
 */
class UnderlayMessageType : public eprosima::fastdds::dds::TopicDataType
{
public:
    using type = UnderlayMessage;

    UnderlayMessageType();
    ~UnderlayMessageType() override;

    bool serialize(void* data,
                   eprosima::fastrtps::rtps::SerializedPayload_t* payload) override;
    bool deserialize(eprosima::fastrtps::rtps::SerializedPayload_t* payload,
                     void* data) override;
    std::function<uint32_t()> getSerializedSizeProvider(void* data) override;
    bool getKey(void* data, eprosima::fastrtps::rtps::InstanceHandle_t* ihandle,
                bool force_md5 = false) override;
    void* createData() override;
    void deleteData(void* data) override;

    MD5 m_md5;
    unsigned char* m_keyBuffer;
};

}  // namespace transport
}  // namespace autolink
