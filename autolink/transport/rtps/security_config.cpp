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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace autolink {
namespace transport {
namespace {

std::string Trim(std::string s) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

void SetErr(std::string* err, const std::string& msg) {
    if (err != nullptr) {
        *err = msg;
    }
}

}  // namespace

const char* SecurityConfig::kIdentityCa = "identity_ca.crt";
const char* SecurityConfig::kPermissionsCa = "permissions_ca.crt";
const char* SecurityConfig::kCert = "cert.pem";
const char* SecurityConfig::kKey = "key.pem";
const char* SecurityConfig::kGovernance = "governance.smime";
const char* SecurityConfig::kPermissions = "permissions.smime";

SecurityConfig SecurityConfig::FromEnv() {
    SecurityConfig cfg;
    const char* security = std::getenv("AUTOLINK_RTPS_SECURITY");
    if (security != nullptr && std::string(security) == "1") {
        cfg.enabled = true;
    }
    const char* dir_env = std::getenv("AUTOLINK_RTPS_SECURITY_DIR");
    if (dir_env != nullptr) {
        cfg.dir = Trim(dir_env);
    }
    return cfg;
}

std::string SecurityConfig::Path(const char* filename) const {
    return dir + "/" + filename;
}

bool SecurityConfig::Validate(std::string* err) const {
    if (!enabled) {
        return true;
    }
    if (dir.empty()) {
        SetErr(err, "AUTOLINK_RTPS_SECURITY_DIR is empty");
        return false;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec) || ec) {
        SetErr(err, "AUTOLINK_RTPS_SECURITY_DIR is not a directory: " + dir);
        return false;
    }

    static const char* kRequired[] = {
            kIdentityCa, kPermissionsCa, kCert,
            kKey,        kGovernance,    kPermissions,
    };
    for (const char* name : kRequired) {
        const fs::path path = Path(name);
        if (!fs::is_regular_file(path, ec) || ec) {
            SetErr(err, "missing security file: " + path.string());
            return false;
        }
    }
    return true;
}

}  // namespace transport
}  // namespace autolink
