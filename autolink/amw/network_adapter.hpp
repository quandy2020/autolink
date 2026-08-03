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

#include <memory>

#include "autolink/amw/provider.hpp"
#include "autolink/message/message_traits.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/transport/receiver/receiver.hpp"
#include "autolink/transport/transmitter/transmitter.hpp"

namespace autolink {
namespace amw {

// Adapts AMW IPublisher (RawMessage) to typed Transmitter<M>.
template <typename M>
class NetworkTransmitter : public transport::Transmitter<M>
{
public:
    NetworkTransmitter(const proto::RoleAttributes& attr,
                       const std::shared_ptr<IPublisher>& publisher)
        : transport::Transmitter<M>(attr), publisher_(publisher) {
        enabled_ = (publisher_ != nullptr);
    }

    void Enable() override {
        if (!publisher_) {
            enabled_ = false;
            return;
        }
        publisher_->Enable();
        enabled_ = true;
    }

    void Disable() override {
        if (publisher_) {
            publisher_->Disable();
        }
        enabled_ = false;
    }

    void Enable(const proto::RoleAttributes& /*opposite_attr*/) override {
        Enable();
    }

    void Disable(const proto::RoleAttributes& /*opposite_attr*/) override {
        Disable();
    }

    bool AcquireMessage(std::shared_ptr<M>& /*msg*/) override {
        return false;
    }

    bool Transmit(const typename transport::Transmitter<M>::MessagePtr& msg,
                  const transport::MessageInfo& msg_info) override {
        if (!enabled_ || !publisher_ || !msg) {
            AERROR << "NetworkTransmitter not ready enabled=" << enabled_
                   << " publisher=" << (publisher_ != nullptr)
                   << " msg=" << (msg != nullptr);
            return false;
        }
        // Frame: [MessageInfo][payload] so Service/Client spare_id/seq survive RTPS.
        std::string framed;
        if (!msg_info.SerializeTo(&framed)) {
            AERROR << "NetworkTransmitter MessageInfo serialize failed";
            return false;
        }
        std::string body;
        if (!message::SerializeToString(*msg, &body)) {
            AERROR << "NetworkTransmitter SerializeToString failed";
            return false;
        }
        framed.append(body);
        auto raw = std::make_shared<message::RawMessage>(framed);
        raw->timestamp = msg_info.seq_num();
        if (!publisher_->Publish(raw)) {
            AERROR << "NetworkTransmitter Publish failed bytes="
                   << framed.size();
            return false;
        }
        return true;
    }

private:
    std::shared_ptr<IPublisher> publisher_;
    bool enabled_ = false;
};

// Adapts AMW ISubscription to typed Receiver<M>.
template <typename M>
class NetworkReceiver : public transport::Receiver<M>
{
public:
    using Listener = typename transport::Receiver<M>::MessageListener;

    NetworkReceiver(const proto::RoleAttributes& attr, const Listener& listener,
                    const std::shared_ptr<ITransportProvider>& provider)
        : transport::Receiver<M>(attr, listener), provider_(provider) {
        EndpointDesc endpoint;
        endpoint.attr = attr;
        subscription_ = provider_->CreateSubscription(
            endpoint, [this](const std::shared_ptr<message::RawMessage>& raw,
                             const transport::MessageInfo& info,
                             const proto::RoleAttributes& role) {
                OnRaw(raw, info, role);
            });
        enabled_ = (subscription_ != nullptr);
    }

    void Enable() override {
        if (!subscription_) {
            enabled_ = false;
            return;
        }
        subscription_->Enable();
        enabled_ = true;
    }

    void Disable() override {
        if (subscription_) {
            subscription_->Disable();
        }
        enabled_ = false;
    }

    void Enable(const proto::RoleAttributes& /*opposite_attr*/) override {
        Enable();
    }

    void Disable(const proto::RoleAttributes& /*opposite_attr*/) override {
        Disable();
    }

private:
    void OnRaw(const std::shared_ptr<message::RawMessage>& raw,
               const transport::MessageInfo& info,
               const proto::RoleAttributes& /*role*/) {
        if (!enabled_ || !raw) {
            return;
        }
        transport::MessageInfo msg_info = info;
        const char* data = raw->message.data();
        int size = static_cast<int>(raw->message.size());
        // Prefer framed MessageInfo; fall back to legacy payload-only samples.
        if (raw->message.size() >= transport::MessageInfo::kSize &&
            msg_info.DeserializeFrom(data,
                                     transport::MessageInfo::kSize)) {
            data += transport::MessageInfo::kSize;
            size -= static_cast<int>(transport::MessageInfo::kSize);
        }
        auto msg = std::make_shared<M>();
        if (size <= 0 ||
            !message::ParseFromArray(data, size, msg.get())) {
            return;
        }
        this->OnNewMessage(msg, msg_info);
    }

    std::shared_ptr<ITransportProvider> provider_;
    std::shared_ptr<ISubscription> subscription_;
    bool enabled_ = false;
};

}  // namespace amw
}  // namespace autolink
