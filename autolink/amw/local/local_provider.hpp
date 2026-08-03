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

#include <atomic>
#include <memory>

#include "autolink/amw/provider.hpp"
#include "autolink/proto/transport_conf.pb.h"
#include "autolink/transport/receiver/intra_receiver.hpp"
#include "autolink/transport/receiver/receiver.hpp"
#include "autolink/transport/receiver/shm_receiver.hpp"
#include "autolink/transport/transmitter/intra_transmitter.hpp"
#include "autolink/transport/transmitter/shm_transmitter.hpp"
#include "autolink/transport/transmitter/transmitter.hpp"

namespace autolink {
namespace amw {

class LocalProvider final : public ITransportProvider
{
public:
    ProviderId id() const override {
        return ProviderId::kLocal;
    }

    bool Init(const AmwContext& context) override;
    void Shutdown() override;

    std::shared_ptr<IPublisher> CreatePublisher(
        const EndpointDesc& endpoint) override;
    std::shared_ptr<ISubscription> CreateSubscription(
        const EndpointDesc& endpoint, const MessageCallback& callback) override;

    template <typename M>
    std::shared_ptr<transport::Transmitter<M>> CreateTransmitter(
        const proto::RoleAttributes& attr, proto::OptionalMode mode);

    template <typename M>
    std::shared_ptr<transport::Receiver<M>> CreateReceiver(
        const proto::RoleAttributes& attr,
        const typename transport::Receiver<M>::MessageListener& listener,
        proto::OptionalMode mode);

private:
    std::atomic<bool> inited_{false};
};

template <typename M>
std::shared_ptr<transport::Transmitter<M>> LocalProvider::CreateTransmitter(
    const proto::RoleAttributes& attr, proto::OptionalMode mode) {
    if (mode == proto::OptionalMode::INTRA) {
        auto transmitter =
            std::make_shared<transport::IntraTransmitter<M>>(attr);
        transmitter->Enable();
        return transmitter;
    }
    auto transmitter = std::make_shared<transport::ShmTransmitter<M>>(attr);
    transmitter->Enable();
    return transmitter;
}

template <typename M>
std::shared_ptr<transport::Receiver<M>> LocalProvider::CreateReceiver(
    const proto::RoleAttributes& attr,
    const typename transport::Receiver<M>::MessageListener& listener,
    proto::OptionalMode mode) {
    if (mode == proto::OptionalMode::INTRA) {
        auto receiver =
            std::make_shared<transport::IntraReceiver<M>>(attr, listener);
        receiver->Enable();
        return receiver;
    }
    auto receiver =
        std::make_shared<transport::ShmReceiver<M>>(attr, listener);
    receiver->Enable();
    return receiver;
}

}  // namespace amw
}  // namespace autolink
