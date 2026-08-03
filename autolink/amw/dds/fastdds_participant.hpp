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

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include "autolink/amw/dds/fastdds_raw_type.hpp"
#include "autolink/common/macros.hpp"

namespace autolink {
namespace amw {

class FastDdsParticipant
{
public:
    ~FastDdsParticipant();

    bool Init();
    void Shutdown();
    bool IsReady() const {
        return ready_.load();
    }

    uint32_t domain_id() const {
        return domain_id_;
    }

    eprosima::fastdds::dds::DomainParticipant* participant() const {
        return participant_;
    }
    eprosima::fastdds::dds::Publisher* publisher() const {
        return publisher_;
    }
    eprosima::fastdds::dds::Subscriber* subscriber() const {
        return subscriber_;
    }

    eprosima::fastdds::dds::Topic* GetOrCreateTopic(const std::string& channel);

private:
    uint32_t ResolveDomainId() const;

    std::atomic<bool> ready_{false};
    uint32_t domain_id_ = 0;
    eprosima::fastdds::dds::DomainParticipant* participant_ = nullptr;
    eprosima::fastdds::dds::Publisher* publisher_ = nullptr;
    eprosima::fastdds::dds::Subscriber* subscriber_ = nullptr;
    eprosima::fastdds::dds::TypeSupport type_support_;
    std::mutex topic_mutex_;
    std::unordered_map<std::string, eprosima::fastdds::dds::Topic*> topics_;

    DECLARE_SINGLETON(FastDdsParticipant)
};

}  // namespace amw
}  // namespace autolink

#endif  // AUTOLINK_ENABLE_FASTDDS
