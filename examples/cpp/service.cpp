/******************************************************************************
 * Copyright 2026 The Openbot Authors (duyongquan)
 *****************************************************************************/

#include "autolink/autolink.hpp"
#include "autolink/time/time.hpp"
#include "examples.pb.h"
#include <unistd.h>

using autolink::examples::Driver;

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0]))
        return 1;
    const std::string suffix =
        std::to_string(getpid()) + "_" +
        std::to_string(autolink::Time::Now().ToNanosecond());
    const std::string service_name = "test_server_" + suffix;
    auto server_node = autolink::CreateNode("service_server_" + suffix,
                                            "/examples");
    auto client_node = autolink::CreateNode("service_client_" + suffix,
                                            "/examples");
    AINFO << "service demo name: " << service_name;
    auto server = server_node->CreateService<Driver, Driver>(
        service_name, [](const std::shared_ptr<Driver>& request,
                          std::shared_ptr<Driver>& response) {
            AINFO << "server: i am driver server, request id: "
                  << request->msg_id();
            response->set_msg_id(request->msg_id());
            response->set_timestamp(autolink::Time::Now().ToNanosecond());
        });
    auto client = client_node->CreateClient<Driver, Driver>(service_name);
    if (server == nullptr || client == nullptr) {
        AERROR << "failed to create service/client.";
        return 1;
    }
    auto driver_msg = std::make_shared<Driver>();
    uint64_t req_id = 0;
    while (autolink::OK()) {
        driver_msg->set_msg_id(req_id++);
        driver_msg->set_timestamp(autolink::Time::Now().ToNanosecond());
        auto res = client->SendRequest(driver_msg);
        if (res != nullptr) {
            AINFO << "client: response msg_id=" << res->msg_id();
        } else {
            AINFO << "client: service may not ready.";
        }
        sleep(1);
    }
    autolink::WaitForShutdown();
    return 0;
}
