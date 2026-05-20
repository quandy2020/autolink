/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * POD 消息发送步骤（SHM / Intra 通用）:
 * 1) 定义 POD 结构体，并保证是 trivially copyable。
 * 2) 定义一个消息包装类型（本例 PodPacket），实现:
 *    - TypeName / descriptor
 *    - ByteSizeLong
 *    - SerializeToArray / ParseFromArray
 *    - SerializeToString / ParseFromString
 * 3) 使用 CreateWriter<PodPacket>/CreateReader<PodPacket> 在同一 channel 通信。
 * 4) 发送前填充 POD 字段，调用 writer->Write(msg)。
 *
 * 参考文档: docs/source/autolink_pod_message_cn.md
 *****************************************************************************/

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <unistd.h>

#include "autolink/autolink.hpp"
#include "autolink/time/rate.hpp"
#include "autolink/time/time.hpp"

namespace {

using autolink::Rate;
using autolink::Time;

struct SimplePod {
    uint64_t seq;
    double value;
    uint64_t timestamp_ns;
};

static_assert(std::is_trivially_copyable<SimplePod>::value,
              "SimplePod must be trivially copyable");

// 包装类型：给框架提供序列化/反序列化接口。
struct PodPacket {
    SimplePod pod{};

    class Descriptor
    {
    public:
        std::string full_name() const {
            return "autolink.examples.SimplePodPacket";
        }
    };

    static const Descriptor* descriptor() {
        static Descriptor desc;
        return &desc;
    }

    static std::string TypeName() {
        return "autolink.examples.SimplePodPacket";
    }

    size_t ByteSizeLong() const {
        return sizeof(SimplePod);
    }

    bool SerializeToArray(void* data, int size) const {
        if (data == nullptr || size < static_cast<int>(sizeof(SimplePod))) {
            return false;
        }
        std::memcpy(data, &pod, sizeof(SimplePod));
        return true;
    }

    bool ParseFromArray(const void* data, int size) {
        if (data == nullptr || size < static_cast<int>(sizeof(SimplePod))) {
            return false;
        }
        std::memcpy(&pod, data, sizeof(SimplePod));
        return true;
    }

    bool SerializeToString(std::string* out) const {
        if (out == nullptr) {
            return false;
        }
        out->assign(reinterpret_cast<const char*>(&pod), sizeof(SimplePod));
        return true;
    }

    bool ParseFromString(const std::string& in) {
        if (in.size() < sizeof(SimplePod)) {
            return false;
        }
        std::memcpy(&pod, in.data(), sizeof(SimplePod));
        return true;
    }
};

void OnPodMessage(const std::shared_ptr<PodPacket>& msg) {
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

    const std::string node_name = "pod_talker_listener_" +
                                  std::to_string(getpid()) + "_" +
                                  std::to_string(Time::Now().ToNanosecond());
    auto node = autolink::CreateNode(node_name);
    // 第 3 步：创建 POD 类型 writer/reader。
    auto writer = node->CreateWriter<PodPacket>("channel/pod_demo");
    auto reader =
        node->CreateReader<PodPacket>("channel/pod_demo", OnPodMessage);
    (void)reader;

    uint64_t seq = 0;
    Rate rate(2.0);
    while (autolink::OK()) {
        // 第 4 步：填充 POD，发送。
        auto msg = std::make_shared<PodPacket>();
        msg->pod.seq = seq;
        msg->pod.value = static_cast<double>(seq) * 0.5;
        msg->pod.timestamp_ns = Time::Now().ToNanosecond();
        writer->Write(msg);
        AINFO << "TX pod seq=" << msg->pod.seq << " value=" << msg->pod.value
              << " ts=" << msg->pod.timestamp_ns;
        ++seq;
        rate.Sleep();
    }

    return 0;
}
