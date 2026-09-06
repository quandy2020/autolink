/******************************************************************************
 * Copyright 2026 The Openbot Authors (duyongquan)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "autolink/transport/rtps/payload_limit.hpp"

#include <cstdlib>

#include "gtest/gtest.h"

namespace autolink {
namespace transport {
namespace {

class PayloadLimitEnvGuard {
public:
    PayloadLimitEnvGuard() {
        unsetenv("AUTOLINK_RTPS_MAX_PAYLOAD_BYTES");
        unsetenv("AUTOLINK_RTPS_REJECT_OVERSIZE");
    }
    ~PayloadLimitEnvGuard() {
        unsetenv("AUTOLINK_RTPS_MAX_PAYLOAD_BYTES");
        unsetenv("AUTOLINK_RTPS_REJECT_OVERSIZE");
    }
};

TEST(PayloadLimitTest, FromEnvDefaults) {
    PayloadLimitEnvGuard guard;
    const auto lim = PayloadLimit::FromEnv();
    EXPECT_EQ(lim.max_bytes, 4u * 1024u * 1024u);
    EXPECT_FALSE(lim.reject_oversize);
}

TEST(PayloadLimitTest, FromEnvParsesMaxAndReject) {
    PayloadLimitEnvGuard guard;
    setenv("AUTOLINK_RTPS_MAX_PAYLOAD_BYTES", "1024", 1);
    setenv("AUTOLINK_RTPS_REJECT_OVERSIZE", "1", 1);
    const auto lim = PayloadLimit::FromEnv();
    EXPECT_EQ(lim.max_bytes, 1024u);
    EXPECT_TRUE(lim.reject_oversize);
}

TEST(PayloadLimitTest, FromEnvZeroDisables) {
    PayloadLimitEnvGuard guard;
    setenv("AUTOLINK_RTPS_MAX_PAYLOAD_BYTES", "0", 1);
    const auto lim = PayloadLimit::FromEnv();
    EXPECT_EQ(lim.max_bytes, 0u);
}

TEST(PayloadLimitTest, FromEnvIgnoresInvalidMax) {
    PayloadLimitEnvGuard guard;
    setenv("AUTOLINK_RTPS_MAX_PAYLOAD_BYTES", "not-a-number", 1);
    const auto lim = PayloadLimit::FromEnv();
    EXPECT_EQ(lim.max_bytes, 4u * 1024u * 1024u);
}

TEST(PayloadLimitTest, CheckPayloadSizeBranches) {
    PayloadLimit lim;
    lim.max_bytes = 100;
    lim.reject_oversize = false;

    EXPECT_EQ(CheckPayloadSize(50, lim), PayloadCheck::kOk);
    EXPECT_EQ(CheckPayloadSize(100, lim), PayloadCheck::kOk);
    EXPECT_EQ(CheckPayloadSize(101, lim), PayloadCheck::kWarn);

    lim.reject_oversize = true;
    EXPECT_EQ(CheckPayloadSize(101, lim), PayloadCheck::kReject);

    lim.max_bytes = 0;
    EXPECT_EQ(CheckPayloadSize(1u << 30, lim), PayloadCheck::kOk);
}

}  // namespace
}  // namespace transport
}  // namespace autolink
