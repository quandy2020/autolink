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

/** @file rtps_receiver.hpp
 * @brief RTPS reception using callbacks that survive dispatcher copies safely.
 */

#pragma once

#include "autolink/common/log.hpp"
#include "autolink/transport/dispatcher/rtps_dispatcher.hpp"
#include "autolink/transport/receiver/receiver.hpp"

namespace autolink {
namespace transport {

template <typename M>
class RtpsReceiver : public Receiver<M>
{
public:
    RtpsReceiver(const RoleAttributes& attr,
                 const typename Receiver<M>::MessageListener& msg_listener);
    virtual ~RtpsReceiver();

    void Enable() override;
    void Disable() override;

    void Enable(const RoleAttributes& opposite_attr) override;
    void Disable(const RoleAttributes& opposite_attr) override;

private:
    RtpsDispatcherPtr dispatcher_;
};

template <typename M>
RtpsReceiver<M>::RtpsReceiver(
        const RoleAttributes& attr,
        const typename Receiver<M>::MessageListener& msg_listener)
    : Receiver<M>(attr, msg_listener) {
    dispatcher_ = RtpsDispatcher::Instance();
}

template <typename M>
RtpsReceiver<M>::~RtpsReceiver() {
    this->CloseCallbacks();
    Disable();
}

template <typename M>
void RtpsReceiver<M>::Enable() {
    if (this->enabled_) {
        return;
    }
    dispatcher_->AddListener<M>(
            this->attr_,
            this->SnapshotDispatcherListener());
    this->enabled_ = true;
}

template <typename M>
void RtpsReceiver<M>::Disable() {
    if (!this->enabled_) {
        return;
    }
    dispatcher_->RemoveListener<M>(this->attr_);
    this->enabled_ = false;
}

template <typename M>
void RtpsReceiver<M>::Enable(const RoleAttributes& opposite_attr) {
    dispatcher_->AddListener<M>(
            this->attr_, opposite_attr,
            this->SnapshotDispatcherListener());
}

template <typename M>
void RtpsReceiver<M>::Disable(const RoleAttributes& opposite_attr) {
    dispatcher_->RemoveListener<M>(this->attr_, opposite_attr);
}

}  // namespace transport
}  // namespace autolink
