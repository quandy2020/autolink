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

#include "bind_timer.hpp"

#include <cstdint>
#include <memory>
#include <utility>

#include "autolink/timer/timer.hpp"
#include "gil_utils.hpp"

namespace autolink::python {

class PyTimer {
public:
    PyTimer(uint32_t period_ms, pybind11::function callback, bool oneshot)
        : callback_(std::make_shared<pybind11::function>(std::move(callback))),
          timer_(std::make_unique<autolink::Timer>(autolink::TimerOption(
              period_ms, [callback = callback_]() { CallPyVoid(callback); },
              oneshot))) {}

    ~PyTimer() {
        Stop();
        pybind11::gil_scoped_acquire gil;
        callback_.reset();
    }

    void Start() {
        timer_->Start();
    }

    void Stop() {
        timer_->Stop();
    }

private:
    std::shared_ptr<pybind11::function> callback_;
    std::unique_ptr<autolink::Timer> timer_;
};

void BindTimer(pybind11::module_& module) {
    namespace py = pybind11;

    py::class_<PyTimer>(module, "Timer")
        .def(py::init<uint32_t, py::function, bool>(), py::arg("period_ms"),
             py::arg("callback"),
             py::arg("oneshot") = false)
        .def("start", &PyTimer::Start)
        .def("stop", &PyTimer::Stop);
}

}  // namespace autolink::python
