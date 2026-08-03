#include <pybind11/pybind11.h>

#include "bind_lifecycle.hpp"
#include "bind_node.hpp"
#include "bind_action.hpp"
#include "bind_parameter.hpp"
#include "bind_record.hpp"
#include "bind_time.hpp"
#include "bind_timer.hpp"
#include "bind_utils.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_core, m) {
  m.doc() = "autolink Python bindings (pybind11)";
  m.attr("__version__") = "0.1.0-pybind11";
  autolink::python::BindLifecycle(m);
  autolink::python::BindNode(m);
  autolink::python::BindAction(m);
  autolink::python::BindParameter(m);
  autolink::python::BindRecord(m);
  autolink::python::BindTime(m);
  autolink::python::BindTimer(m);
  autolink::python::BindUtils(m);
}
