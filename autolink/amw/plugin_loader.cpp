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

#include "autolink/amw/plugin_loader.hpp"

#include <dlfcn.h>

#include <cstdlib>
#include <mutex>
#include <utility>

#include "autolink/amw/plugin_abi.h"
#include "autolink/common/log.hpp"

namespace autolink {
namespace amw {
namespace {

using GetAbiFn = int (*)();
using GetIdFn = const char* (*)();
using CreateFn = void* (*)();
using DestroyFn = void (*)(void*);

struct DlHandleDeleter {
    void operator()(void* handle) const {
        if (handle != nullptr) {
            dlclose(handle);
        }
    }
};

class PluginProviderDeleter
{
public:
    PluginProviderDeleter(std::shared_ptr<void> handle, DestroyFn destroy)
        : handle_(std::move(handle)), destroy_(destroy) {}

    void operator()(ITransportProvider* p) const {
        if (p != nullptr && destroy_ != nullptr) {
            destroy_(p);
        }
    }

    void operator()(IDiscoveryProvider* p) const {
        if (p != nullptr && destroy_ != nullptr) {
            destroy_(p);
        }
    }

private:
    std::shared_ptr<void> handle_;
    DestroyFn destroy_ = nullptr;
};

std::string NormalizeImplName(std::string name) {
    name = ToLowerAscii(std::move(name));
    if (name.rfind("rmw_", 0) == 0) {
        const ProviderId id = ProviderIdFromName(name);
        if (IsNetworkProvider(id)) {
            return ImplementationName(id);
        }
    }
    return name;
}

bool IdentifiersMatch(const std::string& requested, const char* plugin_id) {
    if (plugin_id == nullptr) {
        return false;
    }
    const std::string req = NormalizeImplName(requested);
    const std::string got = NormalizeImplName(plugin_id);
    if (req == got) {
        return true;
    }
    return ProviderIdFromName(req) == ProviderIdFromName(got) &&
           IsNetworkProvider(ProviderIdFromName(req));
}

void AppendEnvSearchDirs(const char* env_name,
                         std::vector<std::string>* dirs) {
    if (dirs == nullptr) {
        return;
    }
    const char* value = std::getenv(env_name);
    if (value == nullptr || value[0] == '\0') {
        return;
    }
    auto parts = SplitSearchPath(value);
    dirs->insert(dirs->end(), parts.begin(), parts.end());
}

}  // namespace

std::vector<std::string> SplitSearchPath(const std::string& path) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : path) {
        if (c == ':' || c == ';') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

std::vector<std::string> ListBuiltinImplementations() {
    return {
        "amw_fastdds",
        "amw_cyclonedds",
        "amw_opendds",
        "amw_connextdds",
    };
}

std::vector<std::string> PluginLibraryCandidates(
    const std::string& implementation) {
    const std::string impl = NormalizeImplName(implementation);
    std::vector<std::string> names;
#if defined(__APPLE__)
    names.push_back("lib" + impl + ".dylib");
    names.push_back(impl + ".dylib");
#else
    names.push_back("lib" + impl + ".so");
    names.push_back(impl + ".so");
#endif
    const ProviderId id = ProviderIdFromName(impl);
    if (IsNetworkProvider(id)) {
        const std::string amw = ImplementationName(id);
        if (amw != impl) {
#if defined(__APPLE__)
            names.push_back("lib" + std::string(amw) + ".dylib");
#else
            names.push_back("lib" + std::string(amw) + ".so");
#endif
        }
    }
    return names;
}

bool TryLoadExternalPlugin(const std::string& implementation,
                           LoadedPlugin* out) {
    if (out == nullptr) {
        return false;
    }
    const std::string impl = NormalizeImplName(implementation);

    std::vector<std::string> search_dirs;
    AppendEnvSearchDirs("AUTOLINK_AMW_PLUGIN_PATH", &search_dirs);
    AppendEnvSearchDirs("LD_LIBRARY_PATH", &search_dirs);
#if defined(__APPLE__)
    AppendEnvSearchDirs("DYLD_LIBRARY_PATH", &search_dirs);
#endif

    std::vector<std::string> open_candidates;
    for (const auto& lib : PluginLibraryCandidates(impl)) {
        open_candidates.push_back(lib);
        for (const auto& dir : search_dirs) {
            if (dir.empty()) {
                continue;
            }
            const char sep =
#if defined(_WIN32)
                '\\';
#else
                '/';
#endif
            if (dir.back() == '/' || dir.back() == '\\') {
                open_candidates.push_back(dir + lib);
            } else {
                open_candidates.push_back(dir + sep + lib);
            }
        }
    }

    for (const auto& path : open_candidates) {
        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            continue;
        }

        auto get_abi = reinterpret_cast<GetAbiFn>(
            dlsym(handle, "amw_get_abi_version"));
        auto get_id = reinterpret_cast<GetIdFn>(
            dlsym(handle, "amw_get_implementation_identifier"));
        auto create_tx = reinterpret_cast<CreateFn>(
            dlsym(handle, "amw_create_transport_provider"));
        auto create_disc = reinterpret_cast<CreateFn>(
            dlsym(handle, "amw_create_discovery_provider"));
        auto destroy =
            reinterpret_cast<DestroyFn>(dlsym(handle, "amw_destroy_provider"));
        if (get_abi == nullptr || get_id == nullptr || create_tx == nullptr ||
            create_disc == nullptr || destroy == nullptr) {
            AWARN << "AMW plugin missing ABI symbols: " << path;
            dlclose(handle);
            continue;
        }
        const int abi = get_abi();
        if (abi != AUTOLINK_AMW_PLUGIN_ABI_VERSION) {
            AERROR << "AMW plugin ABI mismatch path=" << path
                   << " got=" << abi
                   << " expected=" << AUTOLINK_AMW_PLUGIN_ABI_VERSION;
            dlclose(handle);
            continue;
        }
        const char* id_str = get_id();
        if (!IdentifiersMatch(impl, id_str)) {
            AWARN << "AMW plugin identifier mismatch requested=" << impl
                  << " plugin=" << (id_str ? id_str : "(null)")
                  << " path=" << path;
            dlclose(handle);
            continue;
        }

        void* tx_raw = create_tx();
        void* disc_raw = create_disc();
        if (tx_raw == nullptr || disc_raw == nullptr) {
            AWARN << "AMW plugin failed to create providers: " << path;
            if (tx_raw) {
                destroy(tx_raw);
            }
            if (disc_raw) {
                destroy(disc_raw);
            }
            dlclose(handle);
            continue;
        }

        std::shared_ptr<void> keep_alive(handle, DlHandleDeleter{});
        PluginProviderDeleter deleter(keep_alive, destroy);
        out->handle = handle;
        out->library_path = path;
        out->identifier = id_str != nullptr ? id_str : impl;
        out->transport.reset(static_cast<ITransportProvider*>(tx_raw), deleter);
        out->discovery.reset(static_cast<IDiscoveryProvider*>(disc_raw),
                             deleter);
        AINFO << "Loaded external AMW plugin '" << out->identifier
              << "' from " << out->library_path;
        return true;
    }
    return false;
}

}  // namespace amw
}  // namespace autolink
