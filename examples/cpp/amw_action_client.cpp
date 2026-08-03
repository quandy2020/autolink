/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host action client (AMW / DDS). Pair with amw_action_server.
 *
 *   source scripts/amw_dual_host_env.sh <lan_ip> 0 amw_cyclonedds
 *   ./build/bin/examples/autolink_example_amw_action_client
 *****************************************************************************/

#include <chrono>
#include <memory>

#include "autolink/action/action.hpp"
#include "autolink/action/types.hpp"
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

using ActionClient = autolink::action::Client<SimpleMessageActionTraits>;
using GoalHandle =
    autolink::action::ClientGoalHandle<SimpleMessageActionTraits>;

constexpr char kActionName[] = "examples/amw_simple_action";
constexpr auto kServerReadyTimeout = std::chrono::seconds(15);
constexpr auto kAcceptTimeout = std::chrono::seconds(15);
constexpr auto kResultTimeout = std::chrono::seconds(30);

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
    AINFO << "amw_action_client host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;

    auto node = autolink::CreateNode("amw_action_client");
    auto client = autolink::action::CreateClient<SimpleMessageActionTraits>(
        node, kActionName);

    if (!client->WaitForActionServer(kServerReadyTimeout)) {
        AERROR << "amw_action_client action server not ready within timeout";
        return 1;
    }
    AINFO << "amw_action_client server ready";

    SimpleMessageActionTraits::Goal goal;
    goal.set_text("hello from amw_action_client");

    ActionClient::SendGoalOptions opts;
    opts.feedback_callback =
        [](std::shared_ptr<GoalHandle>,
           std::shared_ptr<const SimpleMessageActionTraits::Feedback> fb) {
            if (fb) {
                AINFO << "amw_action_client feedback index=" << fb->index();
            }
        };

    const auto accepted_future = client->AsyncSendGoal(goal, opts);
    if (accepted_future.wait_for(kAcceptTimeout) != std::future_status::ready) {
        AERROR << "amw_action_client timeout waiting for goal acceptance";
        return 1;
    }
    std::shared_ptr<GoalHandle> handle = accepted_future.get();
    if (!handle) {
        AERROR << "amw_action_client goal rejected or null handle";
        return 1;
    }

    auto result_future = handle->AsyncGetResult();
    if (result_future.wait_for(kResultTimeout) != std::future_status::ready) {
        AERROR << "amw_action_client timeout waiting for result";
        return 1;
    }

    try {
        const auto wr = result_future.get();
        const bool ok = wr.result && wr.result->success();
        AINFO << "amw_action_client result success="
              << (ok ? "true" : "false")
              << " code=" << autolink::action::ToString(wr.code);
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        AERROR << "amw_action_client result error: " << e.what();
        return 1;
    }
}
