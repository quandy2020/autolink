/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace autolink {
namespace examples {

struct SimplePod {
    uint64_t seq;
    double value;
    uint64_t timestamp_ns;
};

static_assert(std::is_trivially_copyable<SimplePod>::value,
              "SimplePod must be trivially copyable");

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

}  // namespace examples
}  // namespace autolink
