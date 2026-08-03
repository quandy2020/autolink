/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host listener (ROS 2 RMW style selection).
 *
 * Host B:
 *   export AUTOLINK_AMW_IMPLEMENTATION=amw_fastdds
 *   export AUTOLINK_DOMAIN_ID=0
 *   export AUTOLINK_IP=<host_b_lan_ip>
 *   ./bin/examples/autolink_example_amw_listener
 *****************************************************************************/

#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "examples.pb.h"

void MessageCallback(const std::shared_ptr<autolink::examples::Chatter>& msg) {
    AINFO << "amw_listener seq=" << msg->seq()
          << " content=" << msg->content();
}

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }
    if (!autolink::amw::Amw::Instance()->IsNetworkMiddlewareReady()) {
        AERROR << "AMW network middleware is stub; rebuild with "
                  "AUTOLINK_ENABLE_FASTDDS / AUTOLINK_ENABLE_CYCLONEDDS.";
        return 2;
    }
    AINFO << "amw_listener host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;
    auto listener_node = autolink::CreateNode("amw_listener");
    auto listener = listener_node->CreateReader<autolink::examples::Chatter>(
        "channel/chatter", MessageCallback);
    (void)listener;
    AINFO << "amw_listener waiting on channel/chatter (DDS multi-host).";
    autolink::WaitForShutdown();
    return 0;
}
