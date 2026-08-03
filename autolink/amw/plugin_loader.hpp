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

#include <memory>
#include <string>
#include <vector>

#include "autolink/amw/provider.hpp"
#include "autolink/amw/types.hpp"

namespace autolink {
namespace amw {

struct LoadedPlugin {
    std::string identifier;
    std::string library_path;
    void* handle = nullptr;
    std::shared_ptr<ITransportProvider> transport;
    std::shared_ptr<IDiscoveryProvider> discovery;
};

// Try to dlopen lib<implementation>.so/.dylib (ROS 2 rmw_implementation style).
// Returns true when both transport and discovery providers were created.
bool TryLoadExternalPlugin(const std::string& implementation,
                           LoadedPlugin* out);

// Built-in identifiers always available (stubs or real, depending on CMake).
std::vector<std::string> ListBuiltinImplementations();

// Library name candidates for an implementation id.
std::vector<std::string> PluginLibraryCandidates(
    const std::string& implementation);

// Split colon/semicolon-separated search paths (exported for tests).
std::vector<std::string> SplitSearchPath(const std::string& path);

}  // namespace amw
}  // namespace autolink
