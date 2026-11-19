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

/** @file receiver.hpp
 * @brief Owned message forwarding and callback retirement shared by receiver transports.
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include "autolink/transport/common/endpoint.hpp"
#include "autolink/transport/message/history.hpp"
#include "autolink/transport/message/message_info.hpp"

namespace autolink {
namespace transport {

template <typename M>
class Receiver : public Endpoint
{
public:
    using MessagePtr = std::shared_ptr<M>;
    using MessageListener = std::function<void(
        const MessagePtr&, const MessageInfo&, const RoleAttributes&)>;

    Receiver(const RoleAttributes& attr, const MessageListener& msg_listener);
    virtual ~Receiver();

    virtual void Enable() = 0;
    virtual void Disable() = 0;
    virtual void Enable(const RoleAttributes& opposite_attr) = 0;
    virtual void Disable(const RoleAttributes& opposite_attr) = 0;

protected:
    /** @brief Owned forwarding generation retained by copied dispatcher listeners. */
    struct CallbackState {
        /** @brief Preserve endpoint attributes and callback independently of its receiver. */
        CallbackState(const RoleAttributes& attributes, const MessageListener& callback)
            : attr(attributes), listener(callback) {}
        std::recursive_mutex mutex;  ///< Serializes admission and receiver retirement, including reentry.
        std::atomic<bool> retired{false};  ///< Closes admission and wakes polling history tasks.
        const RoleAttributes attr;  ///< Original endpoint identity, never the temporary history channel.
        const MessageListener listener;  ///< Callback whose owner is protected until CloseCallbacks returns.
    };
    using CallbackStatePtr = std::shared_ptr<CallbackState>;  ///< Shared lifetime of one receiving generation.
    using DispatcherListener = std::function<void(const MessagePtr&, const MessageInfo&)>;  ///< Native dispatcher callback signature.

    /** @brief Copy an owned forwarding callback; dispatcher registrations must not capture this. */
    MessageListener SnapshotListener() const;
    /** @brief Adapt owned forwarding to the dispatcher's message/info signature. */
    DispatcherListener SnapshotDispatcherListener() const;
    /** @brief Close admission and wait for callbacks already executing on another thread. */
    void CloseCallbacks();
    /** @brief Forward a message while this receiver is owned by the synchronous caller. */
    void OnNewMessage(const MessagePtr& msg, const MessageInfo& msg_info);

    MessageListener msg_listener_;
    CallbackStatePtr callback_state_;  ///< Survives receiver destruction in copied listeners and history tasks.
};

template <typename M>
Receiver<M>::Receiver(const RoleAttributes& attr,
                      const MessageListener& msg_listener)
    : Endpoint(attr), msg_listener_(msg_listener),
      callback_state_(std::make_shared<CallbackState>(this->attr_, msg_listener)) {}

template <typename M>
Receiver<M>::~Receiver() { CloseCallbacks(); }

template <typename M>
typename Receiver<M>::MessageListener Receiver<M>::SnapshotListener() const {
    return [listener = SnapshotDispatcherListener()](const MessagePtr& msg,
                                                     const MessageInfo& info,
                                                     const RoleAttributes&) {
        listener(msg, info);
    };
}

template <typename M>
typename Receiver<M>::DispatcherListener Receiver<M>::SnapshotDispatcherListener() const {
    return [state = callback_state_](const MessagePtr& msg, const MessageInfo& info) {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        if (state->retired.load() || !state->listener) return;
        // A callback may retire its receiver reentrantly. Keep the callable
        // itself owned until this invocation has returned.
        auto listener = state->listener;
        listener(msg, info, state->attr);
    };
}

template <typename M>
void Receiver<M>::CloseCallbacks() {
    std::lock_guard<std::recursive_mutex> lock(callback_state_->mutex);
    callback_state_->retired.store(true);
}

template <typename M>
void Receiver<M>::OnNewMessage(const MessagePtr& msg,
                               const MessageInfo& msg_info) {
    SnapshotListener()(msg, msg_info, this->attr_);
}

}  // namespace transport
}  // namespace autolink
