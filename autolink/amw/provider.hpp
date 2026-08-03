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

#include <cstdint>
#include <functional>
#include <memory>

#include "autolink/amw/types.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/topology_change.pb.h"
#include "autolink/transport/message/message_info.hpp"

namespace autolink {
namespace amw {

class IPublisher
{
public:
    virtual ~IPublisher() = default;
    virtual void Enable() = 0;
    virtual void Disable() = 0;
    virtual bool Publish(const std::shared_ptr<message::RawMessage>& msg) = 0;
};

class ISubscription
{
public:
    virtual ~ISubscription() = default;
    virtual void Enable() = 0;
    virtual void Disable() = 0;
};

using MessageCallback = std::function<void(
    const std::shared_ptr<message::RawMessage>&,
    const transport::MessageInfo&, const proto::RoleAttributes&)>;

class ITransportProvider
{
public:
    virtual ~ITransportProvider() = default;

    virtual ProviderId id() const = 0;
    virtual bool Init(const AmwContext& context) = 0;
    virtual void Shutdown() = 0;

    virtual std::shared_ptr<IPublisher> CreatePublisher(
        const EndpointDesc& endpoint) = 0;
    virtual std::shared_ptr<ISubscription> CreateSubscription(
        const EndpointDesc& endpoint, const MessageCallback& callback) = 0;

    // True when CreatePublisher/Subscription are not yet implemented.
    virtual bool IsStub() const {
        return false;
    }
};

class IDiscoveryProvider
{
public:
    using ChangeCallback = std::function<void(const proto::ChangeMsg&)>;

    virtual ~IDiscoveryProvider() = default;

    virtual ProviderId id() const = 0;
    virtual bool Start() = 0;
    virtual int64_t Subscribe(proto::ChangeType type,
                              const ChangeCallback& callback) = 0;
    virtual void Unsubscribe(int64_t subscription_id) = 0;
    virtual bool PublishChange(const proto::ChangeMsg& msg) = 0;
    virtual void Shutdown() = 0;

    virtual bool IsStub() const {
        return false;
    }
};

}  // namespace amw
}  // namespace autolink
