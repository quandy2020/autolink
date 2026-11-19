/**
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file client.hpp
 * @brief Native action transport with facade-independent asynchronous ownership.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <exception>
#include <stdexcept>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "autolink/action/client_goal_handle.hpp"
#include "autolink/action/exceptions.hpp"
#include "autolink/action/internal_types.hpp"
#include "autolink/action/result_response_wait.hpp"
#include "autolink/action/types.hpp"
#include "autolink/common/log.hpp"
#include "autolink/common/macros.hpp"
#include "autolink/node/node.hpp"
#include "autolink/node/reader.hpp"
#include "autolink/service/client.hpp"
#include "autolink/service_discovery/topology_manager.hpp"

namespace autolink {
namespace action {

// Forward declaration
template <typename ActionT>
class Client;

/**
 * @class Client
 * @brief Action client implementation.
 *
 * This class creates an action client that can send goals, cancel goals,
 * and receive feedback and results. It uses autolink's Service and Channel
 * mechanisms.
 *
 * @tparam ActionT The action type, which should have Goal, Feedback, and
 * Result types defined.
 */
template <typename ActionT>
class Client
{
public:
    using Goal = typename ActionT::Goal;
    using Feedback = typename ActionT::Feedback;
    using Result = typename ActionT::Result;
    using GoalHandle = ClientGoalHandle<ActionT>;
    using WrappedResult = typename GoalHandle::WrappedResult;

    /**
     * @brief Defines shared_ptr and weak_ptr for Client.
     */
    AUTOLINK_SHARED_PTR_DEFINITIONS(Client)

    /// Options for sending a goal.
    struct SendGoalOptions {
        /// Function called when the goal is accepted or rejected.
        std::function<void(std::shared_ptr<GoalHandle>)> goal_response_callback;

        /// Function called whenever feedback is received for the goal.
        typename GoalHandle::FeedbackCallback feedback_callback;

        /// Function called when the result for the goal is received.
        typename GoalHandle::ResultCallback result_callback;

        /// Register the generated UUID before the request can reach transport.
        std::function<void(const GoalUUID&)> goal_request_callback;
        /// A real negative send_goal response; unlike timeout, proves rejection.
        std::function<void(const GoalUUID&)> goal_rejection_callback;
        /// Indeterminate transmission/acceptance failure, never a terminal ACK.
        std::function<void(const GoalUUID&, std::exception_ptr)> goal_error_callback;
    };

    /**
     * @brief Construct a new Client object.
     *
     * @param node The node to create the client on.
     * @param action_name The name of the action.
     */
    explicit Client(std::shared_ptr<Node> node, const std::string& action_name)
        : node_(node), action_name_(action_name) {
        Init();
    }

    /**
     * @brief Release subscriptions without invalidating outstanding goal evidence.
     * Committed workers and cancellation retain Runtime until they finish.
     * Destruction is not a remote stop; the invocation owner must cancel/drain.
     */
    virtual ~Client() {
        feedback_reader_.reset();
        status_reader_.reset();
    }

    /**
     * @brief Send an action goal and asynchronously get the result.
     *
     * @param goal The goal request.
     * @param options Options for sending the goal request.
     * @return std::shared_future<std::shared_ptr<GoalHandle>> A future that
     * completes when the goal has been accepted or rejected.
     */
    std::shared_future<std::shared_ptr<GoalHandle>> AsyncSendGoal(
        const Goal& goal, const SendGoalOptions& options = SendGoalOptions());

    /**
     * @brief Asynchronously get the result for an active goal.
     *
     * @param goal_handle The goal handle for which to get the result.
     * @param result_callback Optional callback that is called when the result
     * is received.
     * @return std::shared_future<WrappedResult> A future that is set to the
     * goal result when the goal is finished.
     * @throws UnknownGoalHandleError If the goal is unknown.
     */
    std::shared_future<WrappedResult> AsyncGetResult(
        std::shared_ptr<GoalHandle> goal_handle,
        typename GoalHandle::ResultCallback result_callback = nullptr);

    /**
     * @brief Asynchronously request a goal be canceled.
     *
     * @param goal_handle The goal handle requesting to be canceled.
     * @return std::shared_future<bool> A future that is set to true if the
     * cancel request was accepted.
     * @throws UnknownGoalHandleError If the goal is unknown.
     */
    std::shared_future<bool> AsyncCancelGoal(
        std::shared_ptr<GoalHandle> goal_handle);

    /**
     * @brief Cancel one generated UUID even when acceptance is still uncertain.
     * @param goal_id Identity recorded before AsyncSendGoal transmits.
     * @return Cancel request acceptance, not proof of a terminal goal or stop.
     * No local handle lookup is needed for an acceptance-timeout request.
     */
    std::shared_future<bool> AsyncCancelGoalById(const GoalUUID& goal_id);

    /// Goal-specific cancellation transport independent of this Client facade.
    using GoalCancel = std::function<std::shared_future<bool>(const GoalUUID&)>;
    /**
     * @brief Retain cancellation services for an already registered goal.
     * @return Callback usable after facade release; no task or BT node is kept.
     * Cancellation acceptance remains separate from authoritative termination.
     */
    GoalCancel MakeGoalCancelCallback() const {
        const auto runtime = runtime_;
        return [runtime](const GoalUUID& id) { return CancelById(runtime, id); };
    }

    /**
     * @brief Asynchronously request cancellation of all goals on the server.
     *
     * Sends a zero goal UUID (ROS 2 cancel-all semantics).
     *
     * @return std::shared_future<bool> Completes when the cancel service responds.
     */
    std::shared_future<bool> AsyncCancelAllGoals();

    /**
     * @brief Check if the action server is ready.
     *
     * @return true if the server is ready, false otherwise.
     */
    bool ActionServerIsReady() const;

    /**
     * @brief Wait until the action server is ready (event-driven).
     *
     * Blocks until SendGoal/CancelGoal/GetResult services are discoverable,
     * or until timeout. Negative timeout waits indefinitely (same as Service).
     *
     * @tparam RatioT timeout unit, default is std::milli
     * @param timeout wait time in unit of `RatioT`
     * @return true if the action server became ready
     * @return false if timeout
     */
    template <typename RatioT = std::milli>
    bool WaitForActionServer(
        std::chrono::duration<int64_t, RatioT> timeout =
            std::chrono::duration<int64_t, RatioT>(-1)) {
        return WaitForActionServerNanoseconds(
            std::chrono::duration_cast<std::chrono::nanoseconds>(timeout));
    }

private:
    /** @brief Shared transport state which outlives the public Client facade. */
    struct Runtime {
        std::shared_ptr<Node> node; ///< Retains the native service infrastructure.
        std::shared_ptr<autolink::Client<internal::SendGoalRequest<ActionT>,
                                       internal::SendGoalResponse>> send_goal;
        ///< Owns the acceptance transport while a worker is pending.
        std::shared_ptr<autolink::Client<internal::CancelGoalRequest,
                                       internal::CancelGoalResponse>> cancel_goal;
        ///< Supports UUID cancellation after late acceptance or facade release.
        std::shared_ptr<autolink::Client<internal::GetResultRequest,
                                       internal::GetResultResponse<ActionT>>> get_result;
        ///< Retains the service which supplies actual terminal results.
        std::mutex mutex; ///< Registry lock; never held across callbacks/handle methods.
        std::map<GoalUUID, std::weak_ptr<GoalHandle>> goal_handles;
        ///< Weak handles avoid result-callback/Runtime ownership cycles.
    };

    bool WaitForActionServerNanoseconds(std::chrono::nanoseconds time_out);

    void Init();

    /** @brief Monitor results using shared services without a facade pointer. */
    static std::shared_future<WrappedResult> StartResultPolling(
        const std::shared_ptr<Runtime>& runtime, std::shared_ptr<GoalHandle> goal_handle,
        typename GoalHandle::ResultCallback result_callback);
    /** @brief Initiate UUID cancellation using retained services outside locks. */
    static std::shared_future<bool> CancelById(
        const std::shared_ptr<Runtime>& runtime, const GoalUUID& goal_id);
    /** @brief Dispatch feedback outside the handle registry mutex. */
    static void HandleFeedback(const std::shared_ptr<Runtime>& runtime,
        const std::shared_ptr<internal::FeedbackMessage<ActionT>>&
            feedback_msg);
    /** @brief Apply status without holding the registry lock across a handle. */
    static void HandleStatus(const std::shared_ptr<Runtime>& runtime,
        const std::shared_ptr<internal::StatusMessage>& status_msg);

    std::shared_ptr<Node> node_;
    std::string action_name_;

    // Services
    std::shared_ptr<autolink::Client<internal::SendGoalRequest<ActionT>,
                                     internal::SendGoalResponse>>
        send_goal_client_;
    std::shared_ptr<autolink::Client<internal::CancelGoalRequest,
                                     internal::CancelGoalResponse>>
        cancel_goal_client_;
    std::shared_ptr<autolink::Client<internal::GetResultRequest,
                                     internal::GetResultResponse<ActionT>>>
        get_result_client_;

    // Subscribers
    std::shared_ptr<Reader<internal::FeedbackMessage<ActionT>>>
        feedback_reader_;
    std::shared_ptr<Reader<internal::StatusMessage>> status_reader_;

    std::shared_ptr<Runtime> runtime_{std::make_shared<Runtime>()};
    ///< Workers and cancellation own this state; it retains no Client facade.
};

template <typename ActionT>
void Client<ActionT>::Init() {
    // Create SendGoal client
    send_goal_client_ = node_->CreateClient<internal::SendGoalRequest<ActionT>,
                                            internal::SendGoalResponse>(
        action_name_ + "/send_goal");

    // Create CancelGoal client
    cancel_goal_client_ = node_->CreateClient<internal::CancelGoalRequest,
                                              internal::CancelGoalResponse>(
        action_name_ + "/cancel_goal");

    // Create GetResult client
    get_result_client_ =
        node_->CreateClient<internal::GetResultRequest,
                            internal::GetResultResponse<ActionT>>(
            action_name_ + "/get_result");

    // Create feedback reader
    runtime_->node = node_;
    runtime_->send_goal = send_goal_client_;
    runtime_->cancel_goal = cancel_goal_client_;
    runtime_->get_result = get_result_client_;
    const std::weak_ptr<Runtime> weak_runtime = runtime_;
    auto feedback_callback = [weak_runtime](
        const std::shared_ptr<internal::FeedbackMessage<ActionT>>& message) {
        if (auto runtime = weak_runtime.lock()) HandleFeedback(runtime, message);
    };
    std::string feedback_channel = action_name_ + "/feedback";
    feedback_reader_ = node_->CreateReader<internal::FeedbackMessage<ActionT>>(
        feedback_channel, feedback_callback);
    if (!feedback_reader_) {
        AERROR << "Client::Init: Failed to create feedback reader";
    }

    // Create status reader
    auto status_callback = [weak_runtime](
        const std::shared_ptr<internal::StatusMessage>& message) {
        if (auto runtime = weak_runtime.lock()) HandleStatus(runtime, message);
    };
    status_reader_ = node_->CreateReader<internal::StatusMessage>(
        action_name_ + "/status", status_callback);
}

template <typename ActionT>
std::shared_future<std::shared_ptr<typename Client<ActionT>::GoalHandle>>
Client<ActionT>::AsyncSendGoal(const Goal& goal, const SendGoalOptions& options) {
    const auto runtime = runtime_;
    auto promise = std::make_shared<std::promise<std::shared_ptr<GoalHandle>>>();
    auto future = promise->get_future().share();
    const GoalUUID goal_id = GenerateGoalUUID();
    // These notifications are task-owned and must not escape a transport worker.
    const auto notify_response = [options](std::shared_ptr<GoalHandle> handle) {
        try { if (options.goal_response_callback) options.goal_response_callback(std::move(handle)); }
        catch (...) { AERROR << "goal response callback threw"; }
    };
    const auto notify_error = [options, goal_id, promise, notify_response](std::exception_ptr error) {
        try { if (options.goal_error_callback) options.goal_error_callback(goal_id, error); }
        catch (...) { AERROR << "goal error callback threw"; }
        try { promise->set_exception(error); } catch (...) {}
        notify_response(nullptr);  // Legacy callback; error is not rejection.
    };
    try {
        if (options.goal_request_callback) options.goal_request_callback(goal_id);
        auto request = std::make_shared<internal::SendGoalRequest<ActionT>>();
        request->goal_id = goal_id;
        request->goal = goal;
        if (!runtime->send_goal) throw std::runtime_error("send_goal transport unavailable");
        auto response_future = runtime->send_goal->AsyncSendRequest(request);
        std::thread([runtime, promise, goal_id, options, response_future,
                     notify_response, notify_error]() {
            try {
                if (!response_future.valid())
                    throw std::runtime_error("send_goal transport returned an invalid future");
                if (response_future.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
                    throw std::runtime_error("send_goal acceptance timeout; remote state unknown");
                const auto response = response_future.get();
                if (!response) throw std::runtime_error("send_goal null transport response");
                if (!response->accepted) {
                    try {
                        if (options.goal_rejection_callback) options.goal_rejection_callback(goal_id);
                    } catch (...) { AERROR << "goal rejection callback threw"; }
                    promise->set_value(nullptr);
                    notify_response(nullptr);
                    return;
                }
                GoalInfo info;
                info.goal_id = goal_id;
                info.stamp = response->stamp;
                auto handle = std::make_shared<GoalHandle>(
                    info, options.feedback_callback, options.result_callback);
                {
                    std::lock_guard<std::mutex> lock(runtime->mutex);
                    runtime->goal_handles[goal_id] = handle;
                }
                // Install result awareness before either callback or waiter can
                // query AsyncGetResult. Ownership callbacks may arrive very late.
                StartResultPolling(runtime, handle, options.result_callback);
                promise->set_value(handle);
                notify_response(handle);
            } catch (...) { notify_error(std::current_exception()); }
        }).detach();
    } catch (...) { notify_error(std::current_exception()); }
    return future;
}

template <typename ActionT>
std::shared_future<typename Client<ActionT>::WrappedResult>
Client<ActionT>::AsyncGetResult(
    std::shared_ptr<GoalHandle> goal_handle,
    typename GoalHandle::ResultCallback result_callback) {
    return StartResultPolling(runtime_, std::move(goal_handle), std::move(result_callback));
}

template <typename ActionT>
std::shared_future<typename Client<ActionT>::WrappedResult>
Client<ActionT>::StartResultPolling(
    const std::shared_ptr<Runtime>& runtime, std::shared_ptr<GoalHandle> goal_handle,
    typename GoalHandle::ResultCallback result_callback) {
    if (!goal_handle || !runtime->get_result) throw UnknownGoalHandleError();
    const auto goal_id = goal_handle->GetGoalId();
    {
        std::lock_guard<std::mutex> lock(runtime->mutex);
        const auto found = runtime->goal_handles.find(goal_id);
        if (found == runtime->goal_handles.end() || found->second.lock() != goal_handle)
            throw UnknownGoalHandleError();
    }

    if (result_callback) {
        goal_handle->SetResultCallback(result_callback);
    }

    // SetResultAwareness returns the *previous* value. If already true,
    // another GetResult poller is running — only return the shared future.
    if (goal_handle->SetResultAwareness(true)) {
        return goal_handle->AsyncGetResult();
    }

    std::thread([runtime, goal_handle, goal_id]() {
        try {
            while (goal_handle->IsResultAware()) {
                auto request = std::make_shared<internal::GetResultRequest>();
                request->goal_id = goal_handle->GetGoalId();

                auto response_future =
                    runtime->get_result->AsyncSendRequest(request);

                // A locally invalidated/completed handle must not remain stuck
                // inside get() on a response which may never arrive. Retained
                // long-running goals continue to wait without a fake timeout.
                if (!internal::WaitForResultResponse(response_future, [goal_handle] {
                        return goal_handle->IsResultAware();
                    })) break;
                auto response = response_future.get();
                if (!response) {
                    AWARN << "AsyncGetResult: received null response for goal "
                             "ID: "
                          << ToString(goal_handle->GetGoalId());
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                GoalStatus response_status =
                    static_cast<GoalStatus>(response->status);
                ADEBUG << "AsyncGetResult: received response for goal ID: "
                       << ToString(goal_handle->GetGoalId())
                       << " with status: " << static_cast<int>(response_status);

                if (response_status == GoalStatus::SUCCEEDED ||
                    response_status == GoalStatus::CANCELED ||
                    response_status == GoalStatus::ABORTED) {
                    WrappedResult wrapped_result;
                    wrapped_result.goal_id = goal_handle->GetGoalId();
                    wrapped_result.code =
                        static_cast<ResultCode>(response_status);
                    wrapped_result.result =
                        std::make_shared<Result>(response->result);

                    ADEBUG << "AsyncGetResult: setting result for goal ID: "
                           << ToString(goal_handle->GetGoalId())
                           << " with code: "
                           << static_cast<int>(wrapped_result.code);

                    try { goal_handle->SetResult(wrapped_result); }
                    catch (...) { AERROR << "terminal delivery callback threw"; }

                    std::lock_guard<std::mutex> lock(runtime->mutex);
                    runtime->goal_handles.erase(goal_id);
                    break;
                }

                ADEBUG << "AsyncGetResult: goal not in terminal state yet, "
                          "polling again";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } catch (const std::exception& e) {
            AERROR << "Error in AsyncGetResult response: " << e.what();
            goal_handle->Invalidate(UnawareGoalHandleError());
        } catch (...) {
            AERROR << "Non-standard exception in AsyncGetResult response";
            goal_handle->Invalidate(UnawareGoalHandleError());
        }
        // Retiring local result monitoring is not remote termination. Remove
        // only this weak registration so repeated local failures cannot retain
        // an unbounded registry of expired handles in a long-lived Runtime.
        {
            std::lock_guard<std::mutex> lock(runtime->mutex);
            const auto found = runtime->goal_handles.find(goal_id);
            if (found != runtime->goal_handles.end() && found->second.lock() == goal_handle)
                runtime->goal_handles.erase(found);
        }
    }).detach();

    return goal_handle->AsyncGetResult();
}

template <typename ActionT>
std::shared_future<bool> Client<ActionT>::AsyncCancelGoal(
    std::shared_ptr<GoalHandle> goal_handle) {
    if (!goal_handle) throw UnknownGoalHandleError();
    return AsyncCancelGoalById(goal_handle->GetGoalId());
}

template <typename ActionT>
std::shared_future<bool> Client<ActionT>::AsyncCancelGoalById(const GoalUUID& goal_id) {
    return CancelById(runtime_, goal_id);
}

template <typename ActionT>
std::shared_future<bool> Client<ActionT>::CancelById(
    const std::shared_ptr<Runtime>& runtime, const GoalUUID& goal_id) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future().share();
    auto request = std::make_shared<internal::CancelGoalRequest>();
    request->goal_id = goal_id;
    if (!runtime->cancel_goal) throw std::runtime_error("cancel_goal transport unavailable");
    auto response_future = runtime->cancel_goal->AsyncSendRequest(request);
    std::thread([runtime, promise, response_future]() {
        try {
            if (!response_future.valid()) throw std::runtime_error("invalid cancel future");
            if (response_future.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
                throw std::runtime_error("cancel acknowledgment timeout; remote state unknown");
            const auto response = response_future.get();
            promise->set_value(response && response->goals_canceling > 0);
        } catch (...) {
            try { promise->set_exception(std::current_exception()); } catch (...) {}
        }
    }).detach();
    return future;
}

template <typename ActionT>
std::shared_future<bool> Client<ActionT>::AsyncCancelAllGoals() {
    const auto runtime = runtime_;
    auto promise = std::make_shared<std::promise<bool>>();
    std::shared_future<bool> future(promise->get_future());

    if (!runtime->cancel_goal) {
        promise->set_value(false);
        return future;
    }

    auto request = std::make_shared<internal::CancelGoalRequest>();
    request->goal_id = GoalUUID{};

    auto response_future = runtime->cancel_goal->AsyncSendRequest(request);
    std::thread([runtime, promise, response_future]() {
        try {
            if (!response_future.valid()) {
                promise->set_value(false);
                return;
            }
            if (response_future.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
                throw std::runtime_error("cancel-all acknowledgment timeout; remote state unknown");
            auto response = response_future.get();
            promise->set_value(static_cast<bool>(response));
        } catch (const std::exception& e) {
            AERROR << "Error in AsyncCancelAllGoals response: " << e.what();
            try {
                promise->set_value(false);
            } catch (...) {
            }
        }
    }).detach();

    return future;
}

template <typename ActionT>
bool Client<ActionT>::ActionServerIsReady() const {
    bool ready = send_goal_client_ && send_goal_client_->ServiceIsReady() &&
                 cancel_goal_client_ && cancel_goal_client_->ServiceIsReady() &&
                 get_result_client_ && get_result_client_->ServiceIsReady();
    ADEBUG << "ActionServerIsReady: send_goal="
           << (send_goal_client_ && send_goal_client_->ServiceIsReady())
           << ", cancel_goal="
           << (cancel_goal_client_ && cancel_goal_client_->ServiceIsReady())
           << ", get_result="
           << (get_result_client_ && get_result_client_->ServiceIsReady())
           << ", overall=" << ready;
    return ready;
}

template <typename ActionT>
bool Client<ActionT>::WaitForActionServerNanoseconds(
    std::chrono::nanoseconds time_out) {
    if (ActionServerIsReady()) {
        return true;
    }
    if (!send_goal_client_ || !cancel_goal_client_ || !get_result_client_) {
        return false;
    }

    auto* topology = service_discovery::TopologyManager::Instance();
    struct ReadySignal {
        std::mutex mutex; ///< Protects event-driven readiness waits.
        std::condition_variable wake; ///< Safe for an already dispatched listener.
    };
    const auto signal = std::make_shared<ReadySignal>();
    auto conn = topology->AddChangeListener(
        [signal](const service_discovery::ChangeMsg& /*msg*/) {
            signal->wake.notify_all();
        });
    struct ListenerGuard {
        service_discovery::TopologyManager* topology;
        service_discovery::TopologyManager::ChangeConnection conn;
        ~ListenerGuard() {
            topology->RemoveChangeListener(conn);
        }
    } guard{topology, conn};

    const bool forever = time_out.count() < 0;
    const auto deadline =
        std::chrono::steady_clock::now() +
        (forever ? std::chrono::hours(24 * 365) : time_out);
    std::unique_lock<std::mutex> lock(signal->mutex);
    while (forever || std::chrono::steady_clock::now() < deadline) {
        if (ActionServerIsReady()) {
            return true;
        }
        if (forever) {
            signal->wake.wait_for(lock, std::chrono::milliseconds(50));
        } else {
            signal->wake.wait_until(lock, deadline);
        }
    }
    return ActionServerIsReady();
}

template <typename ActionT>
void Client<ActionT>::HandleFeedback(
    const std::shared_ptr<Runtime>& runtime,
    const std::shared_ptr<internal::FeedbackMessage<ActionT>>& feedback_msg) {
    if (!feedback_msg) {
        AWARN << "HandleFeedback: received null feedback message";
        return;
    }

    const GoalUUID& goal_id = feedback_msg->goal_id;
    ADEBUG << "HandleFeedback: received feedback message for goal ID: "
           << ToString(goal_id);
    std::shared_ptr<GoalHandle> goal_handle;
    {
        std::lock_guard<std::mutex> lock(runtime->mutex);
        const auto found = runtime->goal_handles.find(goal_id);
        if (found == runtime->goal_handles.end()) return;
        goal_handle = found->second.lock();
        if (!goal_handle) {
            runtime->goal_handles.erase(found);
            return;
        }
    }

    auto feedback = std::make_shared<Feedback>(feedback_msg->feedback);
    ADEBUG << "HandleFeedback: calling feedback callback for goal ID: "
           << ToString(goal_id) << ", progress: N/A";
    try { goal_handle->CallFeedbackCallback(goal_handle, feedback); }
    catch (...) { AERROR << "action feedback callback threw"; }
}

template <typename ActionT>
void Client<ActionT>::HandleStatus(
    const std::shared_ptr<Runtime>& runtime,
    const std::shared_ptr<internal::StatusMessage>& status_msg) {
    if (!status_msg) {
        return;
    }

    std::vector<std::pair<std::shared_ptr<GoalHandle>, int8_t>> updates;
    {
        std::lock_guard<std::mutex> lock(runtime->mutex);
        for (const auto& status_info : status_msg->status_list) {
            const auto found = runtime->goal_handles.find(status_info.goal_id);
            if (found == runtime->goal_handles.end()) continue;
            if (auto handle = found->second.lock())
                updates.emplace_back(std::move(handle), status_info.status);
            else runtime->goal_handles.erase(found);
        }
    }
    for (const auto& update : updates) update.first->SetStatus(update.second);
}

}  // namespace action
}  // namespace autolink
