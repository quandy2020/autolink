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

#include "bind_utils.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "autolink/python/bridge/utils_bridge.hpp"

namespace autolink::python {
namespace {

struct ChannelUtils {};
struct NodeUtils {};
struct ServiceUtils {};

}  // namespace

void BindUtils(pybind11::module_& module) {
    namespace py = pybind11;

    py::class_<ChannelUtils>(module, "ChannelUtils")
        .def_static(
            "get_msgtype",
            [](const std::string& channel_name, uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetChannelMsgType(channel_name, sleep_s);
            },
            py::arg("channel_name"), py::arg("sleep_s") = 0)
        .def_static(
            "get_channels",
            [](uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetChannels(sleep_s);
            },
            py::arg("sleep_s") = 2)
        .def_static(
            "get_channels_info",
            [](uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetChannelsInfo(sleep_s);
            },
            py::arg("sleep_s") = 2);

    py::class_<NodeUtils>(module, "NodeUtils")
        .def_static(
            "get_nodes",
            [](uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetNodes(sleep_s);
            },
            py::arg("sleep_s") = 2)
        .def_static(
            "get_node_attr",
            [](const std::string& node_name, uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetNodeAttr(node_name, sleep_s);
            },
            py::arg("node_name"), py::arg("sleep_s") = 2)
        .def_static(
            "get_readers_of_node",
            [](const std::string& node_name, uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetReadersOfNode(node_name, sleep_s);
            },
            py::arg("node_name"), py::arg("sleep_s") = 2)
        .def_static(
            "get_writers_of_node",
            [](const std::string& node_name, uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetWritersOfNode(node_name, sleep_s);
            },
            py::arg("node_name"), py::arg("sleep_s") = 2);

    py::class_<ServiceUtils>(module, "ServiceUtils")
        .def_static(
            "get_services",
            [](uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetServices(sleep_s);
            },
            py::arg("sleep_s") = 2)
        .def_static(
            "get_service_attr",
            [](const std::string& service_name, uint8_t sleep_s) {
                py::gil_scoped_release release;
                return python_support::GetServiceAttr(service_name, sleep_s);
            },
            py::arg("service_name"), py::arg("sleep_s") = 2);
}

}  // namespace autolink::python
