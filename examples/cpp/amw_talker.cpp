/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host talker (ROS 2 RMW style selection).
 *
 * Host A:
 *   export AUTOLINK_AMW_IMPLEMENTATION=amw_fastdds   # or RMW_IMPLEMENTATION=rmw_fastrtps_cpp
 *   export AUTOLINK_DOMAIN_ID=0                     # or ROS_DOMAIN_ID
 *   export AUTOLINK_IP=<host_a_lan_ip>
 *   ./bin/examples/autolink_example_amw_talker
 *****************************************************************************/

#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/time/rate.hpp"
#include "autolink/time/time.hpp"
#include "examples.pb.h"

using autolink::Rate;
using autolink::Time;
using autolink::examples::Chatter;

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }
    if (!autolink::amw::Amw::Instance()->IsNetworkMiddlewareReady()) {
        AERROR << "AMW network middleware is stub; rebuild with "
                  "AUTOLINK_ENABLE_FASTDDS / AUTOLINK_ENABLE_CYCLONEDDS.";
        return 2;
    }
    AINFO << "amw_talker host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;
    auto talker_node = autolink::CreateNode("amw_talker");
    auto talker = talker_node->CreateWriter<Chatter>("channel/chatter");
    Rate rate(1.0);
    uint64_t seq = 0;
    while (autolink::OK()) {
        auto msg = std::make_shared<Chatter>();
        msg->set_timestamp(Time::Now().ToNanosecond());
        msg->set_lidar_timestamp(Time::Now().ToNanosecond());
        msg->set_seq(seq);
        msg->set_content("Hello, AMW/FastDDS!");
        talker->Write(msg);
        AINFO << "amw_talker sent seq=" << seq;
        seq++;
        rate.Sleep();
    }
    return 0;
}
