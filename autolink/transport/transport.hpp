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
#include <string>

#include "autolink/amw/amw.hpp"
#include "autolink/common/log.hpp"
#include "autolink/common/macros.hpp"
#include "autolink/proto/transport_conf.pb.h"
#include "autolink/transport/dispatcher/intra_dispatcher.hpp"
#include "autolink/transport/dispatcher/shm_dispatcher.hpp"
#include "autolink/transport/qos/qos_profile_conf.hpp"
#include "autolink/transport/receiver/receiver.hpp"
#include "autolink/transport/shm/notifier_factory.hpp"
#include "autolink/transport/transmitter/transmitter.hpp"

namespace autolink {
namespace transport {
using ParticipantPtr = std::shared_ptr<void>;
}
}  // namespace autolink

#include "autolink/transport/receiver/hybrid_receiver.hpp"
#include "autolink/transport/transmitter/hybrid_transmitter.hpp"

namespace autolink {
namespace transport {

using autolink::proto::OptionalMode;

class Transport
{
public:
    virtual ~Transport();

    void Shutdown();

    template <typename M>
    auto CreateTransmitter(const RoleAttributes& attr,
                           const OptionalMode& mode = OptionalMode::HYBRID) ->
        typename std::shared_ptr<Transmitter<M>>;

    template <typename M>
    auto CreateReceiver(
        const RoleAttributes& attr,
        const typename Receiver<M>::MessageListener& msg_listener,
        const OptionalMode& mode = OptionalMode::HYBRID) ->
        typename std::shared_ptr<Receiver<M>>;

    ParticipantPtr participant() const {
        return participant_;
    }

private:
    std::atomic<bool> is_shutdown_ = {false};
    ParticipantPtr participant_ = nullptr;
    NotifierPtr notifier_ = nullptr;
    IntraDispatcherPtr intra_dispatcher_ = nullptr;
    ShmDispatcherPtr shm_dispatcher_ = nullptr;

    DECLARE_SINGLETON(Transport)
};

template <typename M>
auto Transport::CreateTransmitter(const RoleAttributes& attr,
                                  const OptionalMode& mode) ->
    typename std::shared_ptr<Transmitter<M>> {
    if (is_shutdown_.load()) {
        AINFO << "transport has been shut down.";
        return nullptr;
    }

    RoleAttributes modified_attr = attr;
    if (modified_attr.qos_profile().depth() == 0) {
        modified_attr.mutable_qos_profile()->CopyFrom(
            QosProfileConf::QOS_PROFILE_DEFAULT);
    }

    if (mode == OptionalMode::HYBRID) {
        auto transmitter = std::make_shared<HybridTransmitter<M>>(
            modified_attr, participant_);
        return transmitter;
    }

    auto transmitter =
        amw::Amw::Instance()->CreateTransmitter<M>(modified_attr, mode);
    RETURN_VAL_IF_NULL(transmitter, nullptr);
    if (mode != OptionalMode::HYBRID) {
        transmitter->Enable();
    }
    return transmitter;
}

template <typename M>
auto Transport::CreateReceiver(
    const RoleAttributes& attr,
    const typename Receiver<M>::MessageListener& msg_listener,
    const OptionalMode& mode) -> typename std::shared_ptr<Receiver<M>> {
    if (is_shutdown_.load()) {
        AINFO << "transport has been shut down.";
        return nullptr;
    }

    RoleAttributes modified_attr = attr;
    if (modified_attr.qos_profile().depth() == 0) {
        modified_attr.mutable_qos_profile()->CopyFrom(
            QosProfileConf::QOS_PROFILE_DEFAULT);
    }

    if (mode == OptionalMode::HYBRID) {
        auto receiver = std::make_shared<HybridReceiver<M>>(
            modified_attr, msg_listener, participant_);
        return receiver;
    }

    auto receiver = amw::Amw::Instance()->CreateReceiver<M>(
        modified_attr, msg_listener, mode);
    RETURN_VAL_IF_NULL(receiver, nullptr);
    receiver->Enable();
    return receiver;
}

}  // namespace transport
}  // namespace autolink
