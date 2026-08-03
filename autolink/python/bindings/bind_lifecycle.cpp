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

#include "bind_lifecycle.hpp"

#include <stdexcept>
#include <string>

#include "autolink/init.hpp"
#include "autolink/state.hpp"

namespace autolink::python {

void BindLifecycle(pybind11::module_& module) {
    namespace py = pybind11;

    module.def(
        "init",
        [](const std::string& name) {
            // Init starts logger/scheduler threads; must not hold the GIL.
            py::gil_scoped_release release;
            if (!autolink::Init(name.c_str())) {
                throw std::runtime_error("autolink initialization failed");
            }
        },
        py::arg("module_name") = "autolink");
    module.def("ok", &autolink::OK);
    module.def(
        "shutdown",
        []() {
            py::gil_scoped_release release;
            autolink::Clear();
        });
    module.def("is_shutdown", &autolink::IsShutdown);
    module.def("wait_for_shutdown", &autolink::WaitForShutdown,
               py::call_guard<py::gil_scoped_release>());
}

}  // namespace autolink::python
