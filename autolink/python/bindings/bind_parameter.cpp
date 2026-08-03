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

#include "bind_parameter.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "autolink/node/node.hpp"
#include "autolink/parameter/parameter.hpp"
#include "autolink/python/bridge/parameter_bridge.hpp"

namespace autolink::python {
namespace {

std::shared_ptr<Parameter> MakeParameterCopy(const Parameter& parameter) {
    return std::make_shared<Parameter>(Parameter(parameter));
}

std::vector<std::shared_ptr<Parameter>> MakeParameterCopies(
    const std::vector<Parameter>& src) {
    std::vector<std::shared_ptr<Parameter>> out;
    out.reserve(src.size());
    for (const auto& item : src) {
        out.push_back(MakeParameterCopy(item));
    }
    return out;
}

}  // namespace

void BindParameter(pybind11::module_& module) {
    namespace py = pybind11;

    py::class_<Parameter, std::shared_ptr<Parameter>>(module, "Parameter")
        .def(py::init<>())
        .def(py::init<const std::string&>(), py::arg("name"))
        .def(py::init<const std::string&, bool>(), py::arg("name"),
             py::arg("bool_value"))
        .def(py::init<const std::string&, int>(), py::arg("name"),
             py::arg("int_value"))
        .def(py::init<const std::string&, int64_t>(), py::arg("name"),
             py::arg("int64_value"))
        .def(py::init<const std::string&, double>(), py::arg("name"),
             py::arg("double_value"))
        .def(py::init<const std::string&, const std::string&>(), py::arg("name"),
             py::arg("string_value"))
        .def("name", &Parameter::Name)
        .def("type_name", &Parameter::TypeName)
        .def("as_bool", &Parameter::AsBool)
        .def("as_int64", &Parameter::AsInt64)
        .def("as_double", &Parameter::AsDouble)
        .def("as_string", &Parameter::AsString)
        .def("debug_string", &Parameter::DebugString)
        .def("descriptor", &Parameter::Descriptor);

    py::class_<python_support::ParameterServerHandle,
               std::shared_ptr<python_support::ParameterServerHandle>>(
        module, "ParameterServer")
        .def(py::init([](const py::object& node) {
                 auto native =
                     node.attr("_native")().cast<std::shared_ptr<Node>>();
                 return std::make_shared<python_support::ParameterServerHandle>(
                     native);
             }),
             py::arg("node"))
        .def(
            "set_parameter",
            [](python_support::ParameterServerHandle& self,
               const Parameter& parameter) { self.SetParameter(parameter); },
            py::arg("parameter"))
        .def(
            "get_parameter",
            [](python_support::ParameterServerHandle& self,
               const std::string& name) -> std::shared_ptr<Parameter> {
                Parameter parameter;
                if (!self.GetParameter(name, &parameter)) {
                    throw py::key_error("parameter not found: " + name);
                }
                return MakeParameterCopy(parameter);
            },
            py::arg("name"))
        .def("list_parameters",
             [](python_support::ParameterServerHandle& self) {
                 std::vector<Parameter> parameters;
                 self.ListParameters(&parameters);
                 return MakeParameterCopies(parameters);
             });

    py::class_<python_support::ParameterClientHandle,
               std::shared_ptr<python_support::ParameterClientHandle>>(
        module, "ParameterClient")
        .def(py::init([](const py::object& node,
                         const std::string& server_node_name) {
                 auto native =
                     node.attr("_native")().cast<std::shared_ptr<Node>>();
                 return std::make_shared<python_support::ParameterClientHandle>(
                     native, server_node_name);
             }),
             py::arg("node"), py::arg("server_node_name"))
        .def(
            "set_parameter",
            [](python_support::ParameterClientHandle& self,
               const Parameter& parameter) {
                py::gil_scoped_release release;
                return self.SetParameter(parameter);
            },
            py::arg("parameter"))
        .def(
            "get_parameter",
            [](python_support::ParameterClientHandle& self,
               const std::string& name) -> std::shared_ptr<Parameter> {
                Parameter parameter;
                bool ok = false;
                {
                    py::gil_scoped_release release;
                    ok = self.GetParameter(name, &parameter);
                }
                if (!ok) {
                    throw std::runtime_error("get_parameter failed: " + name);
                }
                return MakeParameterCopy(parameter);
            },
            py::arg("name"))
        .def("list_parameters",
             [](python_support::ParameterClientHandle& self) {
                 std::vector<Parameter> parameters;
                 bool ok = false;
                 {
                     py::gil_scoped_release release;
                     ok = self.ListParameters(&parameters);
                 }
                 if (!ok) {
                     throw std::runtime_error("list_parameters failed");
                 }
                 return MakeParameterCopies(parameters);
             });
}

}  // namespace autolink::python
