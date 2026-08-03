/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
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

#pragma once

#include <unistd.h>

namespace autolink {
namespace tools {

// Global discovery wait (seconds). Set via root `autolink --wait N`.
inline int& DiscoveryWaitSeconds() {
    static int seconds = 2;
    return seconds;
}

inline void WaitForDiscovery() {
    const int seconds = DiscoveryWaitSeconds();
    if (seconds > 0) {
        sleep(static_cast<unsigned int>(seconds));
    }
}

// Prefer this over hardcoded sleep(N) in discovery paths.
// When sleep_s == 0, skip; otherwise use global `--wait` seconds.
inline void WaitForDiscoveryIf(unsigned sleep_s) {
    if (sleep_s > 0) {
        WaitForDiscovery();
    }
}

// Light exit-code conventions for CLI callbacks.
namespace ExitCode {
constexpr int kOk = 0;
constexpr int kError = 1;
constexpr int kNotFound = 2;
}  // namespace ExitCode

}  // namespace tools
}  // namespace autolink
