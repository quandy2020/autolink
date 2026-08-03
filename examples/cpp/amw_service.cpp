/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host service (AMW / FastDDS).
 *
 * Host A:
 *   source scripts/amw_dual_host_env.sh <lan_ip> 0 amw_fastdds
 *   ./bin/examples/autolink_example_amw_service
 *****************************************************************************/

#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/time/time.hpp"
#include "examples.pb.h"

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
    AINFO << "amw_service host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;
    auto node = autolink::CreateNode("amw_service");
    auto server = node->CreateService<Driver, Driver>(
        "amw/driver", [](const std::shared_ptr<Driver>& request,
                         std::shared_ptr<Driver>& response) {
            AINFO << "amw_service request msg_id=" << request->msg_id();
            response->set_msg_id(request->msg_id());
            response->set_timestamp(autolink::Time::Now().ToNanosecond());
        });
    if (server == nullptr) {
        AERROR << "failed to create amw service.";
        return 1;
    }
    AINFO << "amw_service ready on amw/driver";
    autolink::WaitForShutdown();
    return 0;
}
