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

#include "autolink/transport/rtps/security_config.hpp"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>

#include "gtest/gtest.h"

namespace autolink {
namespace transport {
namespace {

class SecurityConfigEnvGuard {
public:
    SecurityConfigEnvGuard() {
        unsetenv("AUTOLINK_RTPS_SECURITY");
        unsetenv("AUTOLINK_RTPS_SECURITY_DIR");
    }
    ~SecurityConfigEnvGuard() {
        unsetenv("AUTOLINK_RTPS_SECURITY");
        unsetenv("AUTOLINK_RTPS_SECURITY_DIR");
    }
};

TEST(SecurityConfigTest, FromEnvDisabledByDefault) {
    SecurityConfigEnvGuard guard;
    const auto c = SecurityConfig::FromEnv();
    EXPECT_FALSE(c.enabled);
    std::string err;
    EXPECT_TRUE(c.Validate(&err));
}

TEST(SecurityConfigTest, EnabledMissingDirFailsValidate) {
    SecurityConfigEnvGuard guard;
    setenv("AUTOLINK_RTPS_SECURITY", "1", 1);
    unsetenv("AUTOLINK_RTPS_SECURITY_DIR");
    const auto c = SecurityConfig::FromEnv();
    ASSERT_TRUE(c.enabled);
    std::string err;
    EXPECT_FALSE(c.Validate(&err));
    EXPECT_FALSE(err.empty());
}

TEST(SecurityConfigTest, EnabledEmptyDirFails) {
    SecurityConfigEnvGuard guard;
    namespace fs = std::filesystem;
    const auto temp =
            fs::temp_directory_path() / "autolink_security_config_empty_XXXXXX";
    fs::create_directories(temp);
    setenv("AUTOLINK_RTPS_SECURITY", "1", 1);
    setenv("AUTOLINK_RTPS_SECURITY_DIR", temp.string().c_str(), 1);

    const auto c = SecurityConfig::FromEnv();
    ASSERT_TRUE(c.enabled);
    EXPECT_EQ(c.dir, temp.string());
    std::string err;
    EXPECT_FALSE(c.Validate(&err));
    EXPECT_FALSE(err.empty());

    fs::remove_all(temp);
}

TEST(SecurityConfigTest, EnabledValidDirPasses) {
    SecurityConfigEnvGuard guard;
    namespace fs = std::filesystem;
    const auto temp =
            fs::temp_directory_path() / "autolink_security_config_ok_XXXXXX";
    fs::create_directories(temp);

    const char* files[] = {
            SecurityConfig::kIdentityCa, SecurityConfig::kPermissionsCa,
            SecurityConfig::kCert,       SecurityConfig::kKey,
            SecurityConfig::kGovernance, SecurityConfig::kPermissions,
    };
    for (const char* name : files) {
        std::ofstream((temp / name).string()) << "";
    }

    setenv("AUTOLINK_RTPS_SECURITY", "1", 1);
    setenv("AUTOLINK_RTPS_SECURITY_DIR", temp.string().c_str(), 1);

    const auto c = SecurityConfig::FromEnv();
    ASSERT_TRUE(c.enabled);
    std::string err;
    EXPECT_TRUE(c.Validate(&err)) << err;
    EXPECT_EQ(c.Path(SecurityConfig::kCert),
              temp.string() + "/" + SecurityConfig::kCert);

    fs::remove_all(temp);
}

TEST(SecurityConfigTest, FromEnvOtherValuesDisable) {
    SecurityConfigEnvGuard guard;
    setenv("AUTOLINK_RTPS_SECURITY", "true", 1);
    const auto c = SecurityConfig::FromEnv();
    EXPECT_FALSE(c.enabled);
}

TEST(SecurityConfigTest, FromEnvTrimsDir) {
    SecurityConfigEnvGuard guard;
    setenv("AUTOLINK_RTPS_SECURITY", "0", 1);
    setenv("AUTOLINK_RTPS_SECURITY_DIR", "  /tmp/certs  ", 1);
    const auto c = SecurityConfig::FromEnv();
    EXPECT_FALSE(c.enabled);
    EXPECT_EQ(c.dir, "/tmp/certs");
}

}  // namespace
}  // namespace transport
}  // namespace autolink
