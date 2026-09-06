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

#include <cstdlib>

#include "gtest/gtest.h"

#if AUTOLINK_ENABLE_FASTDDS
#include "autolink/proto/transport_conf.pb.h"
#include "autolink/transport/rtps/participant_hub.hpp"
#endif

namespace autolink {
namespace transport {

#if !AUTOLINK_ENABLE_FASTDDS

TEST(RtpsSecurityHubTest, DisabledSkip) {
    GTEST_SKIP() << "RTPS Security Hub requires -DAUTOLINK_ENABLE_FASTDDS=ON";
}

#else

TEST(RtpsSecurityHubTest, InitFailsWhenSecurityEnabledWithoutCerts) {
    setenv("AUTOLINK_RTPS_SECURITY", "1", 1);
    unsetenv("AUTOLINK_RTPS_SECURITY_DIR");
    RtpsParticipantHub::Instance().Shutdown();
    proto::RtpsParticipantAttr attr;
    attr.set_lease_duration(12);
    attr.set_announcement_period(3);
    attr.set_domain_id_gain(200);
    attr.set_port_base(10000);
    EXPECT_FALSE(RtpsParticipantHub::Instance().Init(attr));
    unsetenv("AUTOLINK_RTPS_SECURITY");
}

#endif  // AUTOLINK_ENABLE_FASTDDS

}  // namespace transport
}  // namespace autolink
