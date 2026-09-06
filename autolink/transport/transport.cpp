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

#include "autolink/transport/transport.hpp"

#include "autolink/common/global_data.hpp"
#if AUTOLINK_ENABLE_FASTDDS
#include "autolink/transport/rtps/participant_hub.hpp"
#endif

namespace autolink {
namespace transport {

Transport::Transport() {
    notifier_ = NotifierFactory::CreateNotifier();
    intra_dispatcher_ = IntraDispatcher::Instance();
    shm_dispatcher_ = ShmDispatcher::Instance();
#if AUTOLINK_ENABLE_FASTDDS
    auto& hub = RtpsParticipantHub::Instance();
    if (!hub.Init(common::GlobalData::Instance()
                          ->Config()
                          .transport_conf()
                          .participant_attr())) {
        AERROR << "RtpsParticipantHub Init failed";
    } else {
        participant_ = hub.TransportParticipant();
        RtpsDispatcher::Instance()->set_participant(participant_);
    }
#endif
}

Transport::~Transport() {
    Shutdown();
}

void Transport::Shutdown() {
    if (is_shutdown_.exchange(true)) {
        return;
    }

    intra_dispatcher_->Shutdown();
    shm_dispatcher_->Shutdown();
    notifier_->Shutdown();
#if AUTOLINK_ENABLE_FASTDDS
    RtpsDispatcher::Instance()->Shutdown();
    // Hub owns DomainParticipant lifetime; only drop our shared_ptr.
    participant_.reset();
    RtpsParticipantHub::Instance().Shutdown();
#endif
}

}  // namespace transport
}  // namespace autolink
