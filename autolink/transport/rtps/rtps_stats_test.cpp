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

#include "autolink/transport/rtps/rtps_stats.hpp"

#include "gtest/gtest.h"

namespace autolink {
namespace transport {
namespace {

TEST(RtpsStatsTest, CountersAndDump) {
    auto& stats = RtpsStats::Instance();
    stats.Reset();

    stats.AddSent(2);
    stats.AddRecv(3);
    stats.AddWriteFail();
    stats.AddOversize(4);
    stats.SetMatchedReaders(5);
    stats.SetMatchedWriters(6);

    EXPECT_EQ(stats.sent(), 2u);
    EXPECT_EQ(stats.recv(), 3u);
    EXPECT_EQ(stats.write_fail(), 1u);
    EXPECT_EQ(stats.oversize(), 4u);
    EXPECT_EQ(stats.matched_readers(), 5);
    EXPECT_EQ(stats.matched_writers(), 6);

    const std::string dump = stats.Dump();
    EXPECT_NE(dump.find("sent=2"), std::string::npos);
    EXPECT_NE(dump.find("recv=3"), std::string::npos);
    EXPECT_NE(dump.find("write_fail=1"), std::string::npos);
    EXPECT_NE(dump.find("oversize=4"), std::string::npos);
    EXPECT_NE(dump.find("matched_readers=5"), std::string::npos);
    EXPECT_NE(dump.find("matched_writers=6"), std::string::npos);

    stats.Reset();
    EXPECT_EQ(stats.sent(), 0u);
    EXPECT_EQ(stats.recv(), 0u);
}

}  // namespace
}  // namespace transport
}  // namespace autolink
