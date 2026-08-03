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

#include "autolink/amw/local/local_provider.hpp"

#include "autolink/common/log.hpp"
#include "autolink/message/raw_message.hpp"

namespace autolink {
namespace amw {
namespace {

class LocalPublisher final : public IPublisher
{
public:
    explicit LocalPublisher(
        const std::shared_ptr<transport::Transmitter<message::RawMessage>>& tx)
        : tx_(tx) {}

    void Enable() override {
        if (tx_) {
            tx_->Enable();
        }
    }

    void Disable() override {
        if (tx_) {
            tx_->Disable();
        }
    }

    bool Publish(const std::shared_ptr<message::RawMessage>& msg) override {
        if (!tx_ || !msg) {
            return false;
        }
        return tx_->Transmit(msg);
    }

private:
    std::shared_ptr<transport::Transmitter<message::RawMessage>> tx_;
};

class LocalSubscription final : public ISubscription
{
public:
    explicit LocalSubscription(
        const std::shared_ptr<transport::Receiver<message::RawMessage>>& rx)
        : rx_(rx) {}

    void Enable() override {
        if (rx_) {
            rx_->Enable();
        }
    }

    void Disable() override {
        if (rx_) {
            rx_->Disable();
        }
    }

private:
    std::shared_ptr<transport::Receiver<message::RawMessage>> rx_;
};

}  // namespace

bool LocalProvider::Init(const AmwContext& /*context*/) {
    inited_.store(true);
    return true;
}

void LocalProvider::Shutdown() {
    inited_.store(false);
}

std::shared_ptr<IPublisher> LocalProvider::CreatePublisher(
    const EndpointDesc& endpoint) {
    if (!inited_.load()) {
        AWARN << "LocalProvider not initialized.";
        return nullptr;
    }
    auto mode = endpoint.local_mode;
    if (mode != proto::OptionalMode::INTRA &&
        mode != proto::OptionalMode::SHM) {
        mode = proto::OptionalMode::SHM;
    }
    auto tx = CreateTransmitter<message::RawMessage>(endpoint.attr, mode);
    if (!tx) {
        return nullptr;
    }
    return std::make_shared<LocalPublisher>(tx);
}

std::shared_ptr<ISubscription> LocalProvider::CreateSubscription(
    const EndpointDesc& endpoint, const MessageCallback& callback) {
    if (!inited_.load()) {
        AWARN << "LocalProvider not initialized.";
        return nullptr;
    }
    auto mode = endpoint.local_mode;
    if (mode != proto::OptionalMode::INTRA &&
        mode != proto::OptionalMode::SHM) {
        mode = proto::OptionalMode::SHM;
    }
    auto listener =
        [callback](const std::shared_ptr<message::RawMessage>& msg,
                   const transport::MessageInfo& info,
                   const proto::RoleAttributes& attr) {
            if (callback) {
                callback(msg, info, attr);
            }
        };
    auto rx =
        CreateReceiver<message::RawMessage>(endpoint.attr, listener, mode);
    if (!rx) {
        return nullptr;
    }
    return std::make_shared<LocalSubscription>(rx);
}

}  // namespace amw
}  // namespace autolink
