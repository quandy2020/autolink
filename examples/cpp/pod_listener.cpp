/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *****************************************************************************/

#include <string>
#include <unistd.h>

#include "autolink/autolink.hpp"
#include "autolink/time/time.hpp"
#include "examples/cpp/pod_packet.hpp"

namespace {

void OnPodMessage(const std::shared_ptr<autolink::examples::PodPacket>& msg) {
    if (!msg) {
        return;
    }
    AINFO << "RX pod seq=" << msg->pod.seq << " value=" << msg->pod.value
          << " ts=" << msg->pod.timestamp_ns;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }

    const std::string node_name = "pod_listener_" + std::to_string(getpid()) +
                                  "_" +
                                  std::to_string(autolink::Time::Now().ToNanosecond());
    auto node = autolink::CreateNode(node_name);
    auto reader = node->CreateReader<autolink::examples::PodPacket>(
        "channel/pod_demo", OnPodMessage);
    (void)reader;

    autolink::WaitForShutdown();
    return 0;
}
