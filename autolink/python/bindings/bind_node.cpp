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

#include "bind_node.hpp"

#include <memory>
#include <string>
#include <utility>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "autolink/python/bridge/channel_bridge.hpp"
#include "autolink/common/log.hpp"
#include "gil_utils.hpp"

namespace autolink::python {
namespace {

constexpr char kRawDataType[] = "RawData";

class PyChannelWriter {
public:
    explicit PyChannelWriter(
        std::shared_ptr<python_support::ChannelWriter> writer)
        : writer_(std::move(writer)) {}

    bool Write(const pybind11::bytes& data) {
        return writer_->Write(static_cast<std::string>(data));
    }

private:
    std::shared_ptr<python_support::ChannelWriter> writer_;
};

class PyChannelReader {
public:
    PyChannelReader(const std::shared_ptr<Node>& node,
                    const std::string& channel, pybind11::function callback,
                    const std::string& data_type)
        : callback_(std::make_shared<pybind11::function>(std::move(callback))) {
        auto cb_holder = callback_;
        reader_ = std::make_shared<python_support::ChannelReader>(
            node, channel,
            [cb_holder](const std::string& bytes) {
                CallPyBytes(cb_holder, bytes);
            },
            data_type);
    }

    ~PyChannelReader() {
        reader_.reset();
        pybind11::gil_scoped_acquire gil;
        callback_.reset();
    }

    PyChannelReader(const PyChannelReader&) = delete;
    PyChannelReader& operator=(const PyChannelReader&) = delete;

private:
    std::shared_ptr<pybind11::function> callback_;
    std::shared_ptr<python_support::ChannelReader> reader_;
};

class PyServiceHandle {
public:
    explicit PyServiceHandle(
        std::shared_ptr<python_support::ServiceServer> server)
        : server_(std::move(server)) {}

private:
    std::shared_ptr<python_support::ServiceServer> server_;
};

class PyClient {
public:
    explicit PyClient(std::shared_ptr<python_support::ServiceClient> client)
        : client_(std::move(client)) {}

    pybind11::bytes SendRequest(const pybind11::bytes& data, int timeout_sec) {
        const std::string req = data;
        std::string resp;
        {
            pybind11::gil_scoped_release release;
            resp = client_->SendRequest(req, timeout_sec);
        }
        return pybind11::bytes(resp);
    }

private:
    std::shared_ptr<python_support::ServiceClient> client_;
};

class PyNode {
public:
    explicit PyNode(const std::string& name)
        : node_(python_support::CreatePyNode(name)) {}

    void Shutdown() { node_.reset(); }

    std::shared_ptr<PyChannelWriter> CreateWriter(const std::string& channel,
                                                  const std::string& data_type,
                                                  uint32_t qos_depth) const {
        if (!node_) {
            throw std::runtime_error("node has been shut down");
        }
        auto writer = std::make_shared<python_support::ChannelWriter>(
            node_, channel, data_type, qos_depth);
        return std::make_shared<PyChannelWriter>(std::move(writer));
    }

    std::shared_ptr<PyChannelReader> CreateReader(
        const std::string& channel, pybind11::function callback,
        const std::string& data_type) const {
        if (!node_) {
            throw std::runtime_error("node has been shut down");
        }
        return std::make_shared<PyChannelReader>(node_, channel,
                                                 std::move(callback), data_type);
    }

    std::shared_ptr<PyServiceHandle> CreateService(
        const std::string& service_name, pybind11::function callback,
        const std::string& data_type) const {
        if (!node_) {
            throw std::runtime_error("node has been shut down");
        }
        auto cb_holder =
            std::make_shared<pybind11::function>(std::move(callback));
        auto server = std::make_shared<python_support::ServiceServer>(
            node_, service_name,
            [cb_holder](const std::string& req) -> std::string {
                pybind11::gil_scoped_acquire gil;
                try {
                    pybind11::object out = (*cb_holder)(pybind11::bytes(req));
                    if (out.is_none()) {
                        return {};
                    }
                    if (pybind11::isinstance<pybind11::bytes>(out) ||
                        pybind11::isinstance<pybind11::str>(out)) {
                        return out.cast<std::string>();
                    }
                    return pybind11::bytes(out).cast<std::string>();
                } catch (pybind11::error_already_set& error) {
                    AERROR << "Python service callback failed: "
                           << error.what();
                    error.discard_as_unraisable(__func__);
                    return {};
                }
            },
            data_type);
        return std::make_shared<PyServiceHandle>(std::move(server));
    }

    std::shared_ptr<PyClient> CreateClient(const std::string& service_name,
                                           const std::string& data_type) const {
        if (!node_) {
            throw std::runtime_error("node has been shut down");
        }
        auto client = std::make_shared<python_support::ServiceClient>(
            node_, service_name, data_type);
        return std::make_shared<PyClient>(std::move(client));
    }

    bool RegisterMessage(const pybind11::object& descriptor) const {
        std::string serialized;
        if (pybind11::isinstance<pybind11::bytes>(descriptor)) {
            serialized = descriptor.cast<std::string>();
        } else if (pybind11::isinstance<pybind11::str>(descriptor)) {
            serialized = descriptor.cast<std::string>();
        } else {
            throw pybind11::type_error(
                "descriptor must be serialized bytes or str");
        }
        return python_support::RegisterPythonMessageDescriptor(serialized);
    }

    std::shared_ptr<Node> native() const { return node_; }

private:
    std::shared_ptr<Node> node_;
};

}  // namespace

void BindNode(pybind11::module_& module) {
    namespace py = pybind11;

    py::class_<Node, std::shared_ptr<Node>>(module, "_NativeNode");

    py::class_<PyChannelWriter, std::shared_ptr<PyChannelWriter>>(
        module, "ChannelWriter")
        .def("write", &PyChannelWriter::Write, py::arg("data"));

    py::class_<PyChannelReader, std::shared_ptr<PyChannelReader>>(
        module, "ChannelReader");

    py::class_<PyServiceHandle, std::shared_ptr<PyServiceHandle>>(
        module, "Service");

    py::class_<PyClient, std::shared_ptr<PyClient>>(module, "Client")
        .def("send_request", &PyClient::SendRequest, py::arg("data"),
             py::arg("timeout_sec") = 5);

    py::class_<PyNode>(module, "Node", py::dynamic_attr())
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("shutdown", &PyNode::Shutdown)
        .def("_create_writer", &PyNode::CreateWriter, py::arg("channel"),
             py::arg("msg_type"), py::arg("qos_depth") = 1)
        .def("_create_reader", &PyNode::CreateReader, py::arg("channel"),
             py::arg("callback"), py::arg("msg_type") = kRawDataType)
        .def("_create_service", &PyNode::CreateService, py::arg("service_name"),
             py::arg("callback"), py::arg("msg_type") = kRawDataType)
        .def("_create_client", &PyNode::CreateClient, py::arg("service_name"),
             py::arg("msg_type") = kRawDataType)
        .def("register_message", &PyNode::RegisterMessage,
             py::arg("descriptor"))
        .def("_native", &PyNode::native);
}

}  // namespace autolink::python
