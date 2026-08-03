/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host parameter server (AMW / DDS). Pair with amw_param_client.
 *
 *   source scripts/amw_dual_host_env.sh <lan_ip> 0 amw_cyclonedds
 *   ./build/bin/examples/autolink_example_amw_param_server
 *****************************************************************************/

#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/parameter/parameter_server.hpp"

constexpr char kServerNodeName[] = "amw_param_server";

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }
    if (!autolink::amw::Amw::Instance()->IsNetworkMiddlewareReady()) {
        AERROR << "AMW network middleware is stub; rebuild with "
                  "AUTOLINK_ENABLE_FASTDDS / AUTOLINK_ENABLE_CYCLONEDDS.";
        return 2;
    }
    AINFO << "amw_param_server host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;

    auto node = autolink::CreateNode(kServerNodeName);
    auto server = std::make_shared<autolink::ParameterServer>(node);
    server->SetParameter(autolink::Parameter("amw_demo_int", 42));
    AINFO << "amw_param_server ready node=" << kServerNodeName
          << " preset amw_demo_int=42";
    autolink::WaitForShutdown();
    return 0;
}
