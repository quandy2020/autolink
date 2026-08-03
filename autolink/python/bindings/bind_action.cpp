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

#include "bind_action.hpp"

#include <memory>
#include <string>
#include <utility>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "autolink/action/types.hpp"
#include "autolink/common/log.hpp"
#include "autolink/node/node.hpp"
#include "autolink/python/bridge/action_bridge.hpp"
#include "gil_utils.hpp"

namespace autolink::python {
namespace {

std::shared_ptr<Node> NativeNode(const pybind11::object& node) {
    return node.attr("_native")().cast<std::shared_ptr<Node>>();
}

}  // namespace

void BindAction(pybind11::module_& module) {
    namespace py = pybind11;

    py::enum_<action::GoalStatus>(module, "GoalStatus")
        .value("UNKNOWN", action::GoalStatus::UNKNOWN)
        .value("ACCEPTED", action::GoalStatus::ACCEPTED)
        .value("EXECUTING", action::GoalStatus::EXECUTING)
        .value("CANCELING", action::GoalStatus::CANCELING)
        .value("SUCCEEDED", action::GoalStatus::SUCCEEDED)
        .value("CANCELED", action::GoalStatus::CANCELED)
        .value("ABORTED", action::GoalStatus::ABORTED);

    py::enum_<action::GoalResponse>(module, "GoalResponse")
        .value("REJECT", action::GoalResponse::REJECT)
        .value("ACCEPT_AND_EXECUTE", action::GoalResponse::ACCEPT_AND_EXECUTE)
        .value("ACCEPT_AND_DEFER", action::GoalResponse::ACCEPT_AND_DEFER);

    py::enum_<action::CancelResponse>(module, "CancelResponse")
        .value("REJECT", action::CancelResponse::REJECT)
        .value("ACCEPT", action::CancelResponse::ACCEPT);

    py::enum_<action::ResultCode>(module, "ResultCode")
        .value("UNKNOWN", action::ResultCode::UNKNOWN)
        .value("SUCCEEDED", action::ResultCode::SUCCEEDED)
        .value("CANCELED", action::ResultCode::CANCELED)
        .value("ABORTED", action::ResultCode::ABORTED);

    py::class_<python_support::ClientGoalHandleView,
               std::shared_ptr<python_support::ClientGoalHandleView>>(
        module, "ClientGoalHandle")
        .def_property_readonly("goal_id",
                               &python_support::ClientGoalHandleView::GoalIdHex)
        .def_property_readonly("status",
                               &python_support::ClientGoalHandleView::Status)
        .def("is_succeeded",
             &python_support::ClientGoalHandleView::IsSucceeded)
        .def("is_canceled", &python_support::ClientGoalHandleView::IsCanceled)
        .def("is_aborted", &python_support::ClientGoalHandleView::IsAborted)
        .def(
            "get_result",
            [](python_support::ClientGoalHandleView& self, double timeout_sec) {
                std::string data;
                {
                    py::gil_scoped_release release;
                    data = self.GetResult(timeout_sec);
                }
                return py::bytes(data);
            },
            py::arg("timeout_sec") = 60.0)
        .def("get_result_code",
             &python_support::ClientGoalHandleView::GetResultCode)
        .def(
            "cancel",
            [](python_support::ClientGoalHandleView& self, double timeout_sec) {
                py::gil_scoped_release release;
                return self.Cancel(timeout_sec);
            },
            py::arg("timeout_sec") = 5.0);

    py::class_<python_support::ActionClientHandle,
               std::shared_ptr<python_support::ActionClientHandle>>(
        module, "_ActionClient")
        .def(py::init([](const py::object& node,
                         const std::string& action_name) {
                 return std::make_shared<python_support::ActionClientHandle>(
                     NativeNode(node), action_name);
             }),
             py::arg("node"), py::arg("action_name"))
        .def("server_is_ready",
             &python_support::ActionClientHandle::ServerIsReady)
        .def(
            "wait_for_server",
            [](python_support::ActionClientHandle& self, double timeout_sec) {
                py::gil_scoped_release release;
                return self.WaitForServer(timeout_sec);
            },
            py::arg("timeout_sec") = -1.0)
        .def(
            "send_goal",
            [](python_support::ActionClientHandle& self, const py::bytes& goal,
               py::object feedback_cb, double timeout_sec) {
                python_support::ActionClientHandle::FeedbackCallback fb;
                if (!feedback_cb.is_none()) {
                    auto holder = std::make_shared<py::function>(
                        feedback_cb.cast<py::function>());
                    fb = [holder](
                             const std::shared_ptr<
                                 python_support::ClientGoalHandleView>& handle,
                             const std::string& feedback_bytes) {
                        CallPyBytesFirst(holder, feedback_bytes, handle);
                    };
                }
                std::shared_ptr<python_support::ClientGoalHandleView> out;
                {
                    py::gil_scoped_release release;
                    out = self.SendGoal(static_cast<std::string>(goal),
                                        std::move(fb), timeout_sec);
                }
                return out;
            },
            py::arg("goal"), py::arg("feedback_cb") = py::none(),
            py::arg("timeout_sec") = 30.0)
        .def(
            "send_goal_async",
            [](python_support::ActionClientHandle& self, const py::bytes& goal,
               py::object goal_response_cb, py::object feedback_cb,
               py::object result_cb) {
                python_support::ActionClientHandle::GoalResponseCallback gr;
                python_support::ActionClientHandle::FeedbackCallback fb;
                python_support::ActionClientHandle::ResultCallback rb;
                if (!goal_response_cb.is_none()) {
                    auto holder = std::make_shared<py::function>(
                        goal_response_cb.cast<py::function>());
                    gr = [holder](const std::shared_ptr<
                                      python_support::ClientGoalHandleView>&
                                      handle) {
                        CallPy(holder, handle);
                    };
                }
                if (!feedback_cb.is_none()) {
                    auto holder = std::make_shared<py::function>(
                        feedback_cb.cast<py::function>());
                    fb = [holder](
                             const std::shared_ptr<
                                 python_support::ClientGoalHandleView>& handle,
                             const std::string& feedback_bytes) {
                        CallPyBytesFirst(holder, feedback_bytes, handle);
                    };
                }
                if (!result_cb.is_none()) {
                    auto holder = std::make_shared<py::function>(
                        result_cb.cast<py::function>());
                    rb = [holder](const std::string& result_bytes,
                                  int8_t code) {
                        CallPyBytesFirst(holder, result_bytes, code);
                    };
                }
                std::shared_ptr<python_support::ClientGoalHandleView> out;
                {
                    py::gil_scoped_release release;
                    out = self.SendGoalAsync(static_cast<std::string>(goal),
                                             std::move(gr), std::move(fb),
                                             std::move(rb));
                }
                return out;
            },
            py::arg("goal"), py::arg("goal_response_cb") = py::none(),
            py::arg("feedback_cb") = py::none(),
            py::arg("result_cb") = py::none());

    py::class_<python_support::SimpleActionServerHandle,
               std::shared_ptr<python_support::SimpleActionServerHandle>>(
        module, "_SimpleActionServer")
        .def(py::init([](const py::object& node, const std::string& action_name,
                         py::function execute_cb, py::object completion_cb) {
                 auto exec_holder =
                     std::make_shared<py::function>(std::move(execute_cb));
                 python_support::SimpleActionServerHandle::CompletionCallback
                     completion;
                 if (!completion_cb.is_none()) {
                     auto cholder = std::make_shared<py::function>(
                         completion_cb.cast<py::function>());
                     completion = [cholder]() { CallPyVoid(cholder); };
                 }
                 return std::make_shared<
                     python_support::SimpleActionServerHandle>(
                     NativeNode(node), action_name,
                     [exec_holder]() { CallPyVoid(exec_holder); },
                     std::move(completion));
             }),
             py::arg("node"), py::arg("action_name"), py::arg("execute_cb"),
             py::arg("completion_cb") = py::none())
        .def("get_current_goal",
             [](python_support::SimpleActionServerHandle& self) {
                 return py::bytes(self.GetCurrentGoal());
             })
        .def("is_cancel_requested",
             &python_support::SimpleActionServerHandle::IsCancelRequested)
        .def("is_preempt_requested",
             &python_support::SimpleActionServerHandle::IsPreemptRequested)
        .def("accept_pending_goal",
             &python_support::SimpleActionServerHandle::AcceptPendingGoal)
        .def(
            "publish_feedback",
            [](python_support::SimpleActionServerHandle& self,
               const py::bytes& feedback) {
                self.PublishFeedback(static_cast<std::string>(feedback));
            },
            py::arg("feedback"))
        .def(
            "succeeded_current",
            [](python_support::SimpleActionServerHandle& self,
               const py::bytes& result) {
                self.SucceededCurrent(static_cast<std::string>(result));
            },
            py::arg("result") = py::bytes(""))
        .def(
            "terminate_current",
            [](python_support::SimpleActionServerHandle& self,
               const py::bytes& result) {
                self.TerminateCurrent(static_cast<std::string>(result));
            },
            py::arg("result") = py::bytes(""))
        .def("has_feedback_subscriber",
             &python_support::SimpleActionServerHandle::HasFeedbackSubscriber)
        .def("activate", &python_support::SimpleActionServerHandle::Activate)
        .def("deactivate",
             &python_support::SimpleActionServerHandle::Deactivate);

    py::class_<python_support::ServerGoalHandleView,
               std::shared_ptr<python_support::ServerGoalHandleView>>(
        module, "ServerGoalHandle")
        .def("get_goal",
             [](python_support::ServerGoalHandleView& self) {
                 return py::bytes(self.GetGoal());
             })
        .def_property_readonly(
            "goal_id", &python_support::ServerGoalHandleView::GoalIdHex)
        .def(
            "publish_feedback",
            [](python_support::ServerGoalHandleView& self,
               const py::bytes& feedback) {
                self.PublishFeedback(static_cast<std::string>(feedback));
            },
            py::arg("feedback"))
        .def(
            "succeed",
            [](python_support::ServerGoalHandleView& self,
               const py::bytes& result) {
                self.Succeed(static_cast<std::string>(result));
            },
            py::arg("result") = py::bytes(""))
        .def(
            "abort",
            [](python_support::ServerGoalHandleView& self,
               const py::bytes& result) {
                self.Abort(static_cast<std::string>(result));
            },
            py::arg("result") = py::bytes(""))
        .def(
            "canceled",
            [](python_support::ServerGoalHandleView& self,
               const py::bytes& result) {
                self.Canceled(static_cast<std::string>(result));
            },
            py::arg("result") = py::bytes(""))
        .def("execute", &python_support::ServerGoalHandleView::Execute)
        .def("is_active", &python_support::ServerGoalHandleView::IsActive)
        .def("is_executing",
             &python_support::ServerGoalHandleView::IsExecuting)
        .def("is_canceling",
             &python_support::ServerGoalHandleView::IsCanceling)
        .def("get_status", &python_support::ServerGoalHandleView::GetStatus);

    py::class_<python_support::ActionServerHandle,
               std::shared_ptr<python_support::ActionServerHandle>>(
        module, "_ActionServer")
        .def(py::init([](const py::object& node, const std::string& action_name,
                         py::function handle_goal, py::function handle_cancel,
                         py::function handle_accepted) {
                 auto goal_holder =
                     std::make_shared<py::function>(std::move(handle_goal));
                 auto cancel_holder =
                     std::make_shared<py::function>(std::move(handle_cancel));
                 auto accepted_holder =
                     std::make_shared<py::function>(std::move(handle_accepted));
                 return std::make_shared<python_support::ActionServerHandle>(
                     NativeNode(node), action_name,
                     [goal_holder](const std::string& goal_id_hex,
                                   const std::string& goal_bytes) {
                         py::gil_scoped_acquire gil;
                         try {
                             py::object out =
                                 (*goal_holder)(goal_id_hex, py::bytes(goal_bytes));
                             return out.cast<action::GoalResponse>();
                         } catch (py::error_already_set& error) {
                             AERROR << "Action handle_goal failed: "
                                    << error.what();
                             error.discard_as_unraisable(__func__);
                             return action::GoalResponse::REJECT;
                         }
                     },
                     [cancel_holder](
                         const std::shared_ptr<
                             python_support::ServerGoalHandleView>& handle) {
                         py::gil_scoped_acquire gil;
                         try {
                             py::object out = (*cancel_holder)(handle);
                             return out.cast<action::CancelResponse>();
                         } catch (py::error_already_set& error) {
                             AERROR << "Action handle_cancel failed: "
                                    << error.what();
                             error.discard_as_unraisable(__func__);
                             return action::CancelResponse::REJECT;
                         }
                     },
                     [accepted_holder](
                         const std::shared_ptr<
                             python_support::ServerGoalHandleView>& handle) {
                         CallPy(accepted_holder, handle);
                     });
             }),
             py::arg("node"), py::arg("action_name"), py::arg("handle_goal"),
             py::arg("handle_cancel"), py::arg("handle_accepted"))
        .def("has_feedback_subscriber",
             &python_support::ActionServerHandle::HasFeedbackSubscriber);
}

}  // namespace autolink::python
