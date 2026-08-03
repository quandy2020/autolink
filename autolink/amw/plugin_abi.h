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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ROS 2 RMW-style C ABI for out-of-tree AMW DDS plugins.
 *
 * Build a shared library named libamw_<vendor>.so / .dylib that exports:
 *   - amw_get_abi_version
 *   - amw_get_implementation_identifier
 *   - amw_create_transport_provider
 *   - amw_create_discovery_provider
 *   - amw_destroy_provider
 *
 * Select at runtime with AUTOLINK_AMW_IMPLEMENTATION / RMW_IMPLEMENTATION.
 * Opaque handles are cast to autolink::amw::ITransportProvider /
 * IDiscoveryProvider inside the loader.
 */

#define AUTOLINK_AMW_PLUGIN_ABI_VERSION 1

/** Must return AUTOLINK_AMW_PLUGIN_ABI_VERSION. */
int amw_get_abi_version(void);

const char* amw_get_implementation_identifier(void);

void* amw_create_transport_provider(void);
void* amw_create_discovery_provider(void);
void amw_destroy_provider(void* provider);

#ifdef __cplusplus
}
#endif
