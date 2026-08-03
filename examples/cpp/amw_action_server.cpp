/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host action server (AMW / DDS). Pair with amw_action_client.
 *
 *   source scripts/amw_dual_host_env.sh <lan_ip> 0 amw_cyclonedds
 *   ./build/bin/examples/autolink_example_amw_action_server
 *****************************************************************************/

#include <chrono>
#include <memory>
#include <thread>

#include "autolink/action/simple_action_server.hpp"
#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/time/time.hpp"
#include "examples.pb.h"

namespace {

namespace ae = autolink::examples;

struct SimpleMessageActionTraits {
    using Goal = ae::SimpleMessageAction_Goal;
    using Feedback = ae::SimpleMessageAction_Feedback;
    using Result = ae::SimpleMessageAction_Result;
};

using ActionServer =
    autolink::action::SimpleActionServer<SimpleMessageActionTraits>;
using GoalPtr = std::shared_ptr<const SimpleMessageActionTraits::Goal>;
using FeedbackPtr = std::shared_ptr<SimpleMessageActionTraits::Feedback>;
using ResultPtr = std::shared_ptr<SimpleMessageActionTraits::Result>;

constexpr char kActionName[] = "examples/amw_simple_action";
constexpr int kFeedbackSteps = 3;
constexpr auto kFeedbackPeriod = std::chrono::milliseconds(50);
constexpr auto kFeedbackReaderWait = std::chrono::seconds(5);
constexpr auto kFeedbackReaderPoll = std::chrono::milliseconds(20);

void RunAcceptedGoal(const std::shared_ptr<ActionServer>& server) {
    if (!server) {
        return;
    }
    GoalPtr goal = server->GetCurrentGoal();
    if (!goal) {
        AERROR << "No current goal in execute callback";
        return;
    }
    AINFO << "amw_action_server executing text=\"" << goal->text() << "\"";

    const auto wait_deadline =
        std::chrono::steady_clock::now() + kFeedbackReaderWait;
    while (autolink::OK() && !server->HasFeedbackSubscriber() &&
           std::chrono::steady_clock::now() < wait_deadline) {
        std::this_thread::sleep_for(kFeedbackReaderPoll);
    }

    for (int i = 0; i < kFeedbackSteps; ++i) {
        if (server->IsCancelRequested()) {
            ResultPtr aborted =
                std::make_shared<SimpleMessageActionTraits::Result>();
            aborted->set_success(false);
            server->TerminateCurrent(aborted);
            return;
        }
        FeedbackPtr fb =
            std::make_shared<SimpleMessageActionTraits::Feedback>();
        fb->set_index(i);
        server->PublishFeedback(fb);
        AINFO << "amw_action_server feedback index=" << i;
        std::this_thread::sleep_for(kFeedbackPeriod);
    }

    ResultPtr ok = std::make_shared<SimpleMessageActionTraits::Result>();
    ok->set_success(true);
    server->SucceededCurrent(ok);
    AINFO << "amw_action_server result success=true";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }
    if (!autolink::amw::Amw::Instance()->IsNetworkMiddlewareReady()) {
        AERROR << "AMW network middleware is stub; rebuild with "
                  "AUTOLINK_ENABLE_FASTDDS / AUTOLINK_ENABLE_CYCLONEDDS.";
        return 2;
    }
    AINFO << "amw_action_server host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;

    auto node = autolink::CreateNode("amw_action_server");
    std::shared_ptr<ActionServer> server;
    server = std::make_shared<ActionServer>(
        node, kActionName, [&server]() { RunAcceptedGoal(server); });
    AINFO << "amw_action_server ready action=\"" << kActionName << "\"";
    autolink::WaitForShutdown();
    return 0;
}
