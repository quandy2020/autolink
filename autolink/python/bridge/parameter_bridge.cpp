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

#include "autolink/python/bridge/parameter_bridge.hpp"

#include "autolink/parameter/parameter_client.hpp"
#include "autolink/parameter/parameter_server.hpp"

namespace autolink {
namespace python_support {

class ParameterServerHandle::Impl {
public:
    explicit Impl(const std::shared_ptr<Node>& node) : server_(node) {}

    ParameterServer server_;
};

ParameterServerHandle::ParameterServerHandle(
    const std::shared_ptr<Node>& node)
    : impl_(std::make_unique<Impl>(node)) {}

ParameterServerHandle::~ParameterServerHandle() = default;

void ParameterServerHandle::SetParameter(const Parameter& parameter) {
    impl_->server_.SetParameter(parameter);
}

bool ParameterServerHandle::GetParameter(const std::string& name,
                                         Parameter* parameter) {
    return impl_->server_.GetParameter(name, parameter);
}

void ParameterServerHandle::ListParameters(
    std::vector<Parameter>* parameters) {
    impl_->server_.ListParameters(parameters);
}

class ParameterClientHandle::Impl {
public:
    Impl(const std::shared_ptr<Node>& node,
         const std::string& server_node_name)
        : client_(node, server_node_name) {}

    ParameterClient client_;
};

ParameterClientHandle::ParameterClientHandle(
    const std::shared_ptr<Node>& node, const std::string& server_node_name)
    : impl_(std::make_unique<Impl>(node, server_node_name)) {}

ParameterClientHandle::~ParameterClientHandle() = default;

bool ParameterClientHandle::SetParameter(const Parameter& parameter) {
    return impl_->client_.SetParameter(parameter);
}

bool ParameterClientHandle::GetParameter(const std::string& name,
                                         Parameter* parameter) {
    return impl_->client_.GetParameter(name, parameter);
}

bool ParameterClientHandle::ListParameters(
    std::vector<Parameter>* parameters) {
    return impl_->client_.ListParameters(parameters);
}

}  // namespace python_support
}  // namespace autolink
