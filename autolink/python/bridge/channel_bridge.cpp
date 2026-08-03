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

#include "autolink/python/bridge/channel_bridge.hpp"

#include <stdexcept>
#include <utility>

#include "autolink/autolink.hpp"
#include "autolink/init.hpp"
#include "autolink/message/protobuf_factory.hpp"
#include "autolink/message/py_message.hpp"
#include "autolink/node/reader.hpp"
#include "autolink/node/writer.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service/client.hpp"
#include "autolink/service/service.hpp"

#include <chrono>

namespace autolink {
namespace python_support {
namespace {

constexpr char kRawDataType[] = "RawData";

proto::RoleAttributes MakeRoleAttributes(const std::string& channel,
                                         const std::string& data_type,
                                         uint32_t qos_depth) {
    proto::RoleAttributes role_attr;
    role_attr.set_channel_name(channel);
    role_attr.mutable_qos_profile()->set_depth(qos_depth);

    if (data_type.empty() || data_type == kRawDataType) {
        role_attr.set_message_type(message::PyMessageWrap::TypeName());
        return role_attr;
    }

    role_attr.set_message_type(data_type);
    std::string proto_desc;
    message::ProtobufFactory::Instance()->GetDescriptorString(data_type,
                                                              &proto_desc);
    if (!proto_desc.empty()) {
        role_attr.set_proto_desc(proto_desc);
    }
    return role_attr;
}

}  // namespace

class ChannelWriter::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& channel,
         const std::string& data_type, uint32_t qos_depth)
        : data_type_(data_type.empty() ? kRawDataType : data_type) {
        writer_ = node->CreateWriter<message::PyMessageWrap>(
            MakeRoleAttributes(channel, data_type_, qos_depth));
        if (!writer_) {
            throw std::runtime_error("failed to create channel writer");
        }
    }

    bool Write(const std::string& bytes) {
        auto message =
            std::make_shared<message::PyMessageWrap>(bytes, data_type_);
        message->set_type_name(data_type_);
        return writer_->Write(message);
    }

private:
    std::string data_type_;
    std::shared_ptr<Writer<message::PyMessageWrap>> writer_;
};

ChannelWriter::ChannelWriter(const std::shared_ptr<Node>& node,
                             const std::string& channel,
                             const std::string& data_type,
                             uint32_t qos_depth)
    : impl_(std::make_unique<Impl>(node, channel, data_type, qos_depth)) {}

ChannelWriter::~ChannelWriter() = default;

bool ChannelWriter::Write(const std::string& bytes) {
    return impl_->Write(bytes);
}

class ChannelReader::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& channel,
         Callback callback, const std::string& data_type)
        : callback_(std::move(callback)) {
        const std::string type =
            data_type.empty() ? kRawDataType : data_type;
        auto on_message =
            [this](const std::shared_ptr<message::PyMessageWrap>& message) {
                if (!message || !callback_) {
                    return;
                }
                callback_(message->data());
            };
        reader_ = node->CreateReader<message::PyMessageWrap>(
            MakeRoleAttributes(channel, type, 1), on_message);
        if (!reader_) {
            throw std::runtime_error("failed to create channel reader");
        }
    }

    ~Impl() { reader_.reset(); }

private:
    Callback callback_;
    std::shared_ptr<Reader<message::PyMessageWrap>> reader_;
};

ChannelReader::ChannelReader(const std::shared_ptr<Node>& node,
                             const std::string& channel, Callback callback,
                             const std::string& data_type)
    : impl_(std::make_unique<Impl>(node, channel, std::move(callback),
                                   data_type)) {}

ChannelReader::~ChannelReader() = default;

std::shared_ptr<Node> CreatePyNode(const std::string& name) {
    if (!OK()) {
        throw std::runtime_error(
            "autolink.init() must be called before creating a Node");
    }
    auto node = CreateNode(name);
    if (!node) {
        throw std::runtime_error("failed to create node: " + name);
    }
    return node;
}

bool RegisterPythonMessageDescriptor(const std::string& serialized_desc) {
    return message::ProtobufFactory::Instance()->RegisterPythonMessage(
        serialized_desc);
}

class ServiceServer::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& service_name,
         Callback callback, const std::string& data_type)
        : data_type_(data_type.empty() ? kRawDataType : data_type),
          callback_(std::move(callback)) {
        auto f =
            [this](const std::shared_ptr<message::PyMessageWrap>& request,
                   std::shared_ptr<message::PyMessageWrap>& response) {
                std::string out;
                if (callback_ && request) {
                    out = callback_(request->data());
                }
                response = std::make_shared<message::PyMessageWrap>(
                    out, data_type_);
            };
        service_ =
            node->CreateService<message::PyMessageWrap, message::PyMessageWrap>(
                service_name, f);
        if (!service_) {
            throw std::runtime_error("failed to create service: " +
                                     service_name);
        }
    }

private:
    std::string data_type_;
    Callback callback_;
    std::shared_ptr<Service<message::PyMessageWrap, message::PyMessageWrap>>
        service_;
};

ServiceServer::ServiceServer(const std::shared_ptr<Node>& node,
                             const std::string& service_name, Callback callback,
                             const std::string& data_type)
    : impl_(std::make_unique<Impl>(node, service_name, std::move(callback),
                                   data_type)) {}

ServiceServer::~ServiceServer() = default;

class ServiceClient::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& service_name,
         const std::string& data_type)
        : data_type_(data_type.empty() ? kRawDataType : data_type) {
        client_ =
            node->CreateClient<message::PyMessageWrap, message::PyMessageWrap>(
                service_name);
        if (!client_) {
            throw std::runtime_error("failed to create client: " +
                                     service_name);
        }
    }

    std::string SendRequest(const std::string& request_bytes, int timeout_sec) {
        auto req = std::make_shared<message::PyMessageWrap>(request_bytes,
                                                            data_type_);
        auto resp =
            client_->SendRequest(req, std::chrono::seconds(timeout_sec));
        if (!resp) {
            throw std::runtime_error("service request timed out or failed");
        }
        return resp->data();
    }

private:
    std::string data_type_;
    std::shared_ptr<Client<message::PyMessageWrap, message::PyMessageWrap>>
        client_;
};

ServiceClient::ServiceClient(const std::shared_ptr<Node>& node,
                             const std::string& service_name,
                             const std::string& data_type)
    : impl_(std::make_unique<Impl>(node, service_name, data_type)) {}

ServiceClient::~ServiceClient() = default;

std::string ServiceClient::SendRequest(const std::string& request_bytes,
                                       int timeout_sec) {
    return impl_->SendRequest(request_bytes, timeout_sec);
}

}  // namespace python_support
}  // namespace autolink
