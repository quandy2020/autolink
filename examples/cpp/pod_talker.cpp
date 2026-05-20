/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *****************************************************************************/

#include <string>
#include <unistd.h>

#include "autolink/autolink.hpp"
#include "autolink/time/rate.hpp"
#include "autolink/time/time.hpp"
#include "examples/cpp/pod_packet.hpp"

int main(int argc, char* argv[]) {
    if (!autolink::Init(argv[0])) {
        return 1;
    }

    const std::string node_name = "pod_talker_" + std::to_string(getpid()) +
                                  "_" +
                                  std::to_string(autolink::Time::Now().ToNanosecond());
    auto node = autolink::CreateNode(node_name);
    auto writer =
        node->CreateWriter<autolink::examples::PodPacket>("channel/pod_demo");

    uint64_t seq = 0;
    autolink::Rate rate(2.0);
    while (autolink::OK()) {
        auto msg = std::make_shared<autolink::examples::PodPacket>();
        msg->pod.seq = seq;
        msg->pod.value = static_cast<double>(seq) * 0.5;
        msg->pod.timestamp_ns = autolink::Time::Now().ToNanosecond();
        writer->Write(msg);
        AINFO << "TX pod seq=" << msg->pod.seq << " value=" << msg->pod.value
              << " ts=" << msg->pod.timestamp_ns;
        ++seq;
        rate.Sleep();
    }

    return 0;
}
