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

#include "autolink/transport/dispatcher/rtps_dispatcher.hpp"

namespace autolink {
namespace transport {

namespace {
using eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
using eprosima::fastdds::dds::DomainParticipant;
using eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT;
using eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
using eprosima::fastdds::dds::Topic;
}  // namespace

RtpsDispatcher::RtpsDispatcher() : participant_(nullptr) {}

RtpsDispatcher::~RtpsDispatcher() {
    Shutdown();
}

void RtpsDispatcher::Shutdown() {
    if (is_shutdown_.exchange(true)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(subs_mutex_);
        for (auto& item : subs_) {
            auto& sub = item.second;
            if (sub.subscriber != nullptr && sub.reader != nullptr) {
                sub.subscriber->delete_datareader(sub.reader);
            }
            if (participant_ != nullptr) {
                auto* dp = participant_->get();
                if (dp != nullptr) {
                    if (sub.subscriber != nullptr) {
                        dp->delete_subscriber(sub.subscriber);
                    }
                    // Topic may be shared with transmitters; delete best-effort.
                    if (sub.topic != nullptr) {
                        dp->delete_topic(sub.topic);
                    }
                }
            }
            sub.reader = nullptr;
            sub.subscriber = nullptr;
            sub.topic = nullptr;
            sub.listener = nullptr;
        }
        subs_.clear();
    }

    participant_ = nullptr;
}

Topic* RtpsDispatcher::EnsureTopic(DomainParticipant* dp,
                                   const std::string& channel_name) {
    RETURN_VAL_IF_NULL(dp, nullptr);
    auto* desc = dp->lookup_topicdescription(channel_name);
    if (desc != nullptr) {
        return dynamic_cast<Topic*>(desc);
    }
    return dp->create_topic(channel_name, "UnderlayMessage", TOPIC_QOS_DEFAULT);
}

void RtpsDispatcher::AddSubscriber(const RoleAttributes& self_attr) {
    if (participant_ == nullptr) {
        AWARN << "please set participant firstly.";
        return;
    }

    uint64_t channel_id = self_attr.channel_id();
    std::lock_guard<std::mutex> lock(subs_mutex_);
    if (subs_.count(channel_id) > 0) {
        return;
    }

    auto* dp = participant_->get();
    RETURN_IF_NULL(dp);

    RtpsSubscriber new_sub;
    new_sub.topic = EnsureTopic(dp, self_attr.channel_name());
    RETURN_IF_NULL(new_sub.topic);

    new_sub.subscriber = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    RETURN_IF_NULL(new_sub.subscriber);

    eprosima::fastdds::dds::DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    RETURN_IF(!AttributesFiller::FillInSubQos(self_attr.qos_profile(), &rqos));

    auto listener_adapter =
            [this](uint64_t cid, const std::shared_ptr<std::string>& msg_str,
                   const MessageInfo& msg_info) {
                this->OnMessage(cid, msg_str, msg_info);
            };
    new_sub.listener =
            std::make_shared<RtpsReaderListener>(channel_id, listener_adapter);

    new_sub.reader = new_sub.subscriber->create_datareader(
            new_sub.topic, rqos, new_sub.listener.get());
    if (new_sub.reader == nullptr) {
        AERROR << "create_datareader failed for channel "
               << self_attr.channel_name();
        dp->delete_subscriber(new_sub.subscriber);
        return;
    }

    subs_[channel_id] = new_sub;
}

void RtpsDispatcher::OnMessage(uint64_t channel_id,
                               const std::shared_ptr<std::string>& msg_str,
                               const MessageInfo& msg_info) {
    if (is_shutdown_.load()) {
        return;
    }

    ListenerHandlerBasePtr* handler_base = nullptr;
    if (msg_listeners_.Get(channel_id, &handler_base)) {
        auto handler = std::dynamic_pointer_cast<ListenerHandler<std::string>>(
                *handler_base);
        if (handler != nullptr) {
            handler->Run(msg_str, msg_info);
        }
    }
}

}  // namespace transport
}  // namespace autolink
