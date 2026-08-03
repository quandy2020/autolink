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

#include "autolink/python/bridge/action_bridge.hpp"

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <utility>

#include "autolink/action/client.hpp"
#include "autolink/action/create_client.hpp"
#include "autolink/action/create_server.hpp"
#include "autolink/action/exceptions.hpp"
#include "autolink/action/server.hpp"
#include "autolink/action/simple_action_server.hpp"
#include "autolink/common/log.hpp"

namespace autolink {
namespace python_support {
namespace {

using BytesClient = action::Client<BytesActionTraits>;
using BytesClientGoalHandle = action::ClientGoalHandle<BytesActionTraits>;
using BytesSimpleServer = action::SimpleActionServer<BytesActionTraits>;
using BytesServer = action::Server<BytesActionTraits>;
using BytesServerGoalHandle = action::ServerGoalHandle<BytesActionTraits>;

message::RawMessage MakeRaw(const std::string& bytes) {
    return message::RawMessage(bytes);
}

std::string BytesFromRaw(
    const std::shared_ptr<const message::RawMessage>& raw) {
    return raw ? raw->message : std::string();
}

std::string BytesFromRawPtr(
    const std::shared_ptr<message::RawMessage>& raw) {
    return raw ? raw->message : std::string();
}

}  // namespace

std::string GoalUUIDToHex(const action::GoalUUID& uuid) {
    std::ostringstream oss;
    for (uint8_t b : uuid) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    }
    return oss.str();
}

action::GoalUUID GoalUUIDFromHex(const std::string& hex) {
    action::GoalUUID uuid{};
    if (hex.size() != 32) {
        return uuid;
    }
    for (size_t i = 0; i < uuid.size(); ++i) {
        unsigned int value = 0;
        std::istringstream iss(hex.substr(i * 2, 2));
        iss >> std::hex >> value;
        uuid[i] = static_cast<uint8_t>(value);
    }
    return uuid;
}

class ClientGoalHandleViewImpl : public ClientGoalHandleView {
public:
    explicit ClientGoalHandleViewImpl(
        std::shared_ptr<BytesClientGoalHandle> handle,
        std::shared_ptr<BytesClient> client)
        : handle_(std::move(handle)), client_(std::move(client)) {}

    std::string GoalIdHex() const override {
        return GoalUUIDToHex(handle_->GetGoalId());
    }

    int8_t Status() const override {
        return static_cast<int8_t>(handle_->GetStatus());
    }

    bool IsSucceeded() const override { return handle_->IsSucceeded(); }
    bool IsCanceled() const override { return handle_->IsCanceled(); }
    bool IsAborted() const override { return handle_->IsAborted(); }

    std::string GetResult(double timeout_sec) override {
        try {
            if (!handle_) {
                return {};
            }
            // Start / reuse the client-side result poller while the handle is
            // still tracked. After terminal state the client erases the UUID;
            // fall back to the handle's shared future in that case.
            if (client_) {
                try {
                    client_->AsyncGetResult(handle_);
                } catch (const action::UnknownGoalHandleError&) {
                    // Already completed and removed from the client map.
                }
            }
            auto future = handle_->AsyncGetResult();
            if (timeout_sec < 0) {
                wrapped_ = future.get();
            } else {
                auto status = future.wait_for(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::duration<double>(timeout_sec)));
                if (status != std::future_status::ready) {
                    return {};
                }
                wrapped_ = future.get();
            }
            result_code_ = static_cast<int8_t>(wrapped_.code);
            return BytesFromRawPtr(wrapped_.result);
        } catch (const std::exception& e) {
            AERROR << "ClientGoalHandleView::GetResult failed: " << e.what();
            return {};
        }
    }

    int8_t GetResultCode() const override { return result_code_; }

    bool Cancel(double timeout_sec) override {
        auto future = client_->AsyncCancelGoal(handle_);
        if (timeout_sec < 0) {
            future.get();
            return true;
        }
        auto status = future.wait_for(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(timeout_sec)));
        if (status != std::future_status::ready) {
            return false;
        }
        future.get();
        return true;
    }

    std::shared_ptr<BytesClientGoalHandle> Native() const { return handle_; }

private:
    std::shared_ptr<BytesClientGoalHandle> handle_;
    std::shared_ptr<BytesClient> client_;
    BytesClientGoalHandle::WrappedResult wrapped_;
    int8_t result_code_ = 0;
};

class ActionClientHandle::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& action_name)
        : client_(action::CreateClient<BytesActionTraits>(node, action_name)) {}

    bool ServerIsReady() const { return client_->ActionServerIsReady(); }

    bool WaitForServer(double timeout_sec) {
        if (timeout_sec < 0) {
            return client_->WaitForActionServer(
                std::chrono::milliseconds(-1));
        }
        return client_->WaitForActionServer(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(timeout_sec)));
    }

    std::shared_ptr<ClientGoalHandleView> SendGoalAsync(
        const std::string& goal_bytes, GoalResponseCallback goal_response_cb,
        FeedbackCallback feedback_cb, ResultCallback result_cb) {
        BytesClient::SendGoalOptions opts;
        if (feedback_cb) {
            opts.feedback_callback =
                [this, feedback_cb](
                    std::shared_ptr<BytesClientGoalHandle> handle,
                    std::shared_ptr<const message::RawMessage> feedback) {
                    auto view = std::make_shared<ClientGoalHandleViewImpl>(
                        handle, client_);
                    feedback_cb(view, BytesFromRaw(feedback));
                };
        }
        if (result_cb) {
            opts.result_callback =
                [result_cb](const BytesClientGoalHandle::WrappedResult& wr) {
                    result_cb(BytesFromRawPtr(wr.result),
                              static_cast<int8_t>(wr.code));
                };
        }
        if (goal_response_cb) {
            opts.goal_response_callback =
                [this, goal_response_cb](
                    std::shared_ptr<BytesClientGoalHandle> handle) {
                    if (!handle) {
                        goal_response_cb(nullptr);
                        return;
                    }
                    auto view = std::make_shared<ClientGoalHandleViewImpl>(
                        handle, client_);
                    goal_response_cb(view);
                };
        }

        auto future =
            client_->AsyncSendGoal(MakeRaw(goal_bytes), opts);
        // Non-blocking peek: return a placeholder view once accepted via
        // callback; for sync path callers use SendGoal.
        // Still return a view if already ready.
        if (future.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {
            auto handle = future.get();
            if (!handle) {
                return nullptr;
            }
            return std::make_shared<ClientGoalHandleViewImpl>(handle, client_);
        }
        // Store future for sync waiters; async users rely on callbacks.
        pending_futures_.push_back(future);
        return nullptr;
    }

    std::shared_ptr<ClientGoalHandleView> SendGoal(
        const std::string& goal_bytes, FeedbackCallback feedback_cb,
        double timeout_sec) {
        std::mutex mu;
        std::condition_variable cv;
        std::shared_ptr<BytesClientGoalHandle> accepted;
        bool done = false;

        BytesClient::SendGoalOptions opts;
        if (feedback_cb) {
            opts.feedback_callback =
                [feedback_cb, this](
                    std::shared_ptr<BytesClientGoalHandle> handle,
                    std::shared_ptr<const message::RawMessage> feedback) {
                    auto view = std::make_shared<ClientGoalHandleViewImpl>(
                        handle, client_);
                    feedback_cb(view, BytesFromRaw(feedback));
                };
        }
        opts.goal_response_callback =
            [&](std::shared_ptr<BytesClientGoalHandle> handle) {
                {
                    std::lock_guard<std::mutex> lock(mu);
                    accepted = handle;
                    done = true;
                }
                cv.notify_one();
            };

        auto future = client_->AsyncSendGoal(MakeRaw(goal_bytes), opts);
        std::unique_lock<std::mutex> lock(mu);
        if (timeout_sec < 0) {
            cv.wait(lock, [&] { return done; });
        } else {
            if (!cv.wait_for(
                    lock,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::duration<double>(timeout_sec)),
                    [&] { return done; })) {
                // Fallback: wait on the future itself.
                auto st = future.wait_for(
                    std::chrono::milliseconds(0));
                if (st == std::future_status::ready) {
                    accepted = future.get();
                } else {
                    return nullptr;
                }
            }
        }
        if (!accepted) {
            // Try future in case callback was not set/ran.
            if (future.wait_for(std::chrono::milliseconds(0)) ==
                std::future_status::ready) {
                accepted = future.get();
            }
        }
        if (!accepted) {
            return nullptr;
        }
        return std::make_shared<ClientGoalHandleViewImpl>(accepted, client_);
    }

    std::shared_ptr<BytesClient> client_;
    std::vector<std::shared_future<std::shared_ptr<BytesClientGoalHandle>>>
        pending_futures_;
};

ActionClientHandle::ActionClientHandle(const std::shared_ptr<Node>& node,
                                       const std::string& action_name)
    : impl_(std::make_unique<Impl>(node, action_name)) {}

ActionClientHandle::~ActionClientHandle() = default;

bool ActionClientHandle::ServerIsReady() const {
    return impl_->ServerIsReady();
}

bool ActionClientHandle::WaitForServer(double timeout_sec) {
    return impl_->WaitForServer(timeout_sec);
}

std::shared_ptr<ClientGoalHandleView> ActionClientHandle::SendGoalAsync(
    const std::string& goal_bytes, GoalResponseCallback goal_response_cb,
    FeedbackCallback feedback_cb, ResultCallback result_cb) {
    return impl_->SendGoalAsync(goal_bytes, std::move(goal_response_cb),
                                std::move(feedback_cb), std::move(result_cb));
}

std::shared_ptr<ClientGoalHandleView> ActionClientHandle::SendGoal(
    const std::string& goal_bytes, FeedbackCallback feedback_cb,
    double timeout_sec) {
    return impl_->SendGoal(goal_bytes, std::move(feedback_cb), timeout_sec);
}

class SimpleActionServerHandle::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& action_name,
         ExecuteCallback execute_cb, CompletionCallback completion_cb)
        : server_(std::make_shared<BytesSimpleServer>(
              node, action_name, std::move(execute_cb),
              std::move(completion_cb))) {}

    std::string GetCurrentGoal() const {
        return BytesFromRaw(server_->GetCurrentGoal());
    }

    bool IsCancelRequested() const { return server_->IsCancelRequested(); }
    bool IsPreemptRequested() const { return server_->IsPreemptRequested(); }
    bool AcceptPendingGoal() {
        return static_cast<bool>(server_->AcceptPendingGoal());
    }

    void PublishFeedback(const std::string& feedback_bytes) {
        server_->PublishFeedback(
            std::make_shared<message::RawMessage>(feedback_bytes));
    }

    void SucceededCurrent(const std::string& result_bytes) {
        server_->SucceededCurrent(
            std::make_shared<message::RawMessage>(result_bytes));
    }

    void TerminateCurrent(const std::string& result_bytes) {
        server_->TerminateCurrent(
            std::make_shared<message::RawMessage>(result_bytes));
    }

    bool HasFeedbackSubscriber() const {
        return server_->HasFeedbackSubscriber();
    }

    void Activate() { server_->Activate(); }
    void Deactivate() { server_->Deactivate(); }

    std::shared_ptr<BytesSimpleServer> server_;
};

SimpleActionServerHandle::SimpleActionServerHandle(
    const std::shared_ptr<Node>& node, const std::string& action_name,
    ExecuteCallback execute_cb, CompletionCallback completion_cb)
    : impl_(std::make_unique<Impl>(node, action_name, std::move(execute_cb),
                                   std::move(completion_cb))) {}

SimpleActionServerHandle::~SimpleActionServerHandle() = default;

std::string SimpleActionServerHandle::GetCurrentGoal() const {
    return impl_->GetCurrentGoal();
}

bool SimpleActionServerHandle::IsCancelRequested() const {
    return impl_->IsCancelRequested();
}

bool SimpleActionServerHandle::IsPreemptRequested() const {
    return impl_->IsPreemptRequested();
}

bool SimpleActionServerHandle::AcceptPendingGoal() {
    return impl_->AcceptPendingGoal();
}

void SimpleActionServerHandle::PublishFeedback(
    const std::string& feedback_bytes) {
    impl_->PublishFeedback(feedback_bytes);
}

void SimpleActionServerHandle::SucceededCurrent(
    const std::string& result_bytes) {
    impl_->SucceededCurrent(result_bytes);
}

void SimpleActionServerHandle::TerminateCurrent(
    const std::string& result_bytes) {
    impl_->TerminateCurrent(result_bytes);
}

bool SimpleActionServerHandle::HasFeedbackSubscriber() const {
    return impl_->HasFeedbackSubscriber();
}

void SimpleActionServerHandle::Activate() { impl_->Activate(); }
void SimpleActionServerHandle::Deactivate() { impl_->Deactivate(); }

class ServerGoalHandleViewImpl : public ServerGoalHandleView {
public:
    explicit ServerGoalHandleViewImpl(
        std::shared_ptr<BytesServerGoalHandle> handle)
        : handle_(std::move(handle)) {}

    std::string GetGoal() const override {
        return BytesFromRaw(handle_->GetGoal());
    }

    std::string GoalIdHex() const override {
        return GoalUUIDToHex(handle_->GetGoalId());
    }

    void PublishFeedback(const std::string& feedback_bytes) override {
        handle_->PublishFeedback(
            std::make_shared<message::RawMessage>(feedback_bytes));
    }

    void Succeed(const std::string& result_bytes) override {
        handle_->Succeed(std::make_shared<message::RawMessage>(result_bytes));
    }

    void Abort(const std::string& result_bytes) override {
        handle_->Abort(std::make_shared<message::RawMessage>(result_bytes));
    }

    void Canceled(const std::string& result_bytes) override {
        handle_->Canceled(std::make_shared<message::RawMessage>(result_bytes));
    }

    void Execute() override { handle_->Execute(); }

    bool IsActive() const override { return handle_->IsActive(); }
    bool IsExecuting() const override { return handle_->IsExecuting(); }
    bool IsCanceling() const override { return handle_->IsCanceling(); }
    int8_t GetStatus() const override {
        return static_cast<int8_t>(handle_->GetStatus());
    }

private:
    std::shared_ptr<BytesServerGoalHandle> handle_;
};

class ActionServerHandle::Impl {
public:
    Impl(const std::shared_ptr<Node>& node, const std::string& action_name,
         GoalCallback handle_goal, CancelCallback handle_cancel,
         AcceptedCallback handle_accepted) {
        server_ = action::CreateServer<BytesActionTraits>(
            node, action_name,
            [handle_goal](const action::GoalUUID& uuid,
                          std::shared_ptr<const message::RawMessage> goal) {
                return handle_goal(GoalUUIDToHex(uuid), BytesFromRaw(goal));
            },
            [handle_cancel](std::shared_ptr<BytesServerGoalHandle> handle) {
                return handle_cancel(
                    std::make_shared<ServerGoalHandleViewImpl>(handle));
            },
            [handle_accepted](std::shared_ptr<BytesServerGoalHandle> handle) {
                handle_accepted(
                    std::make_shared<ServerGoalHandleViewImpl>(handle));
            });
    }

    bool HasFeedbackSubscriber() const {
        return server_ && server_->HasFeedbackSubscriber();
    }

    std::shared_ptr<BytesServer> server_;
};

ActionServerHandle::ActionServerHandle(const std::shared_ptr<Node>& node,
                                       const std::string& action_name,
                                       GoalCallback handle_goal,
                                       CancelCallback handle_cancel,
                                       AcceptedCallback handle_accepted)
    : impl_(std::make_unique<Impl>(node, action_name, std::move(handle_goal),
                                   std::move(handle_cancel),
                                   std::move(handle_accepted))) {}

ActionServerHandle::~ActionServerHandle() = default;

bool ActionServerHandle::HasFeedbackSubscriber() const {
    return impl_->HasFeedbackSubscriber();
}

}  // namespace python_support
}  // namespace autolink
