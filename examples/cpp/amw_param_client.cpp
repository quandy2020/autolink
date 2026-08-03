/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Multi-host parameter client (AMW / DDS). Pair with amw_param_server.
 *
 *   source scripts/amw_dual_host_env.sh <lan_ip> 0 amw_cyclonedds
 *   ./build/bin/examples/autolink_example_amw_param_client
 *****************************************************************************/

#include <chrono>

#include "autolink/amw/amw.hpp"
#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/parameter/parameter_client.hpp"

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
    AINFO << "amw_param_client host_ip="
          << autolink::common::GlobalData::Instance()->HostIp()
          << " impl="
          << autolink::amw::Amw::Instance()->context().implementation;

    auto node = autolink::CreateNode("amw_param_client");
    auto client =
        std::make_shared<autolink::ParameterClient>(node, kServerNodeName);
    if (!client->WaitForService(std::chrono::seconds(15))) {
        AERROR << "amw_param_client services not ready within 15s";
        return 1;
    }
    AINFO << "amw_param_client services ready";

    if (!client->SetParameter(autolink::Parameter("amw_demo_str", "hello"))) {
        AERROR << "amw_param_client set amw_demo_str failed";
        return 1;
    }
    AINFO << "amw_param_client set amw_demo_str=hello ok";

    autolink::Parameter param;
    if (!client->GetParameter("amw_demo_int", &param)) {
        AERROR << "amw_param_client get amw_demo_int failed";
        return 1;
    }
    AINFO << "amw_param_client get amw_demo_int=" << param.AsInt64();

    if (!client->GetParameter("amw_demo_str", &param)) {
        AERROR << "amw_param_client get amw_demo_str failed";
        return 1;
    }
    AINFO << "amw_param_client get amw_demo_str=" << param.AsString();

    if (param.AsString() != "hello") {
        AERROR << "amw_param_client unexpected string value";
        return 1;
    }
    return 0;
}
