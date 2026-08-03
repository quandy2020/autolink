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

#include "bind_time.hpp"

#include <cstdint>

#include "autolink/time/duration.hpp"
#include "autolink/time/rate.hpp"
#include "autolink/time/time.hpp"

namespace autolink::python {

void BindTime(pybind11::module_& module) {
    namespace py = pybind11;

    py::class_<autolink::Time>(module, "Time")
        .def(py::init<>())
        .def(py::init<uint64_t>(), py::arg("nanoseconds"))
        .def(py::init<double>(), py::arg("seconds"))
        .def_static("now", &autolink::Time::Now)
        .def_static("mono_time", &autolink::Time::MonoTime)
        .def_static("sleep_until", &autolink::Time::SleepUntil,
                    py::call_guard<py::gil_scoped_release>())
        .def("to_sec", &autolink::Time::ToSecond)
        .def("to_nsec", &autolink::Time::ToNanosecond)
        .def("is_zero", &autolink::Time::IsZero);

    py::class_<autolink::Duration>(module, "Duration")
        .def(py::init<>())
        .def(py::init<int64_t>(), py::arg("nanoseconds"))
        .def(py::init<double>(), py::arg("seconds"))
        .def("to_sec", &autolink::Duration::ToSecond)
        .def("to_nsec", &autolink::Duration::ToNanosecond)
        .def("sleep", &autolink::Duration::Sleep,
             py::call_guard<py::gil_scoped_release>())
        .def("is_zero", &autolink::Duration::IsZero);

    py::class_<autolink::Rate>(module, "Rate")
        .def(py::init<double>(), py::arg("frequency"))
        .def(py::init<uint64_t>(), py::arg("nanoseconds"))
        .def("sleep", &autolink::Rate::Sleep,
             py::call_guard<py::gil_scoped_release>())
        .def("reset", &autolink::Rate::Reset)
        .def("cycle_time", &autolink::Rate::CycleTime)
        .def("expected_cycle_time", &autolink::Rate::ExpectedCycleTime);
}

}  // namespace autolink::python
