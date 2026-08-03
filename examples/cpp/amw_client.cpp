/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host client (AMW / FastDDS).
 *
 * Host B:
 *   source scripts/amw_dual_host_env.sh <lan_ip> 0 amw_fastdds
 *   ./bin/examples/autolink_example_amw_client
 *****************************************************************************/

#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/time/rate.hpp"
#include "autolink/time/time.hpp"
#include "examples.pb.h"

using autolink::Rate;
using autolink::Time;
using autolink::examples::Driver;

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }
    if (!autolink::amw::Amw::Instance()->IsNetworkMiddlewareReady()) {
        AERROR << "AMW network middleware is stub; rebuild with "
                  "AUTOLINK_ENABLE_FASTDDS / AUTOLINK_ENABLE_CYCLONEDDS.";
        return 2;
    }
    AINFO << "amw_client host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;
    auto node = autolink::CreateNode("amw_client");
    auto client = node->CreateClient<Driver, Driver>("amw/driver");
    if (client == nullptr) {
        AERROR << "failed to create amw client.";
        return 1;
    }
    if (!client->WaitForService(std::chrono::seconds(10))) {
        AERROR << "amw/driver service not ready within 10s.";
        return 1;
    }
    Rate rate(1.0);
    uint64_t req_id = 0;
    while (autolink::OK()) {
        auto req = std::make_shared<Driver>();
        req->set_msg_id(req_id++);
        req->set_timestamp(Time::Now().ToNanosecond());
        auto res = client->SendRequest(req);
        if (res != nullptr) {
            AINFO << "amw_client response msg_id=" << res->msg_id();
        } else {
            AWARN << "amw_client request timeout or no response.";
        }
        rate.Sleep();
    }
    return 0;
}
