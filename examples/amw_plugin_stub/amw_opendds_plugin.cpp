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

// Reference out-of-tree AMW plugin (OpenDDS identifier, stub providers).
// Build: libamw_opendds.so — load via AUTOLINK_AMW_IMPLEMENTATION=amw_opendds
// and AUTOLINK_AMW_PLUGIN_PATH=<dir containing the .so>.

#include <mutex>
#include <unordered_set>

#include "autolink/amw/dds/dds_stub_provider.hpp"
#include "autolink/amw/plugin_abi.h"

namespace {

std::mutex g_mu;
std::unordered_set<void*> g_transports;
std::unordered_set<void*> g_discoveries;

}  // namespace

extern "C" {

int amw_get_abi_version(void) {
    return AUTOLINK_AMW_PLUGIN_ABI_VERSION;
}

const char* amw_get_implementation_identifier(void) {
    return "amw_opendds";
}

void* amw_create_transport_provider(void) {
    auto* p = new autolink::amw::DdsStubTransportProvider(
        autolink::amw::ProviderId::kOpenDds);
    std::lock_guard<std::mutex> lock(g_mu);
    g_transports.insert(p);
    return p;
}

void* amw_create_discovery_provider(void) {
    auto* p = new autolink::amw::DdsStubDiscoveryProvider(
        autolink::amw::ProviderId::kOpenDds);
    std::lock_guard<std::mutex> lock(g_mu);
    g_discoveries.insert(p);
    return p;
}

void amw_destroy_provider(void* provider) {
    if (provider == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_transports.erase(provider) > 0) {
        delete static_cast<autolink::amw::DdsStubTransportProvider*>(provider);
        return;
    }
    if (g_discoveries.erase(provider) > 0) {
        delete static_cast<autolink::amw::DdsStubDiscoveryProvider*>(provider);
    }
}

}  // extern "C"
