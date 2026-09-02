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

#include "autolink/tools/cli/proto_json.hpp"

#include <fstream>
#include <memory>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/util/json_util.h"

#include "autolink/message/protobuf_factory.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"

namespace autolink {
namespace tools {
namespace {

constexpr const char* kRawMessageType = "autolink.message.RawMessage";

}  // namespace

bool RegisterProtoType(const std::string& type, const std::string& desc) {
    if (type.empty() || type == kRawMessageType) {
        return false;
    }
    auto* factory = message::ProtobufFactory::Instance();
    if (!desc.empty()) {
        factory->RegisterMessage(desc);
    } else {
        std::string d;
        factory->GetDescriptorString(type, &d);
        if (!d.empty()) {
            factory->RegisterMessage(d);
        }
    }
    std::unique_ptr<google::protobuf::Message> msg(
        factory->GenerateMessageByType(type));
    return msg != nullptr;
}

bool RegisterDescriptorSetFile(const std::string& path, std::string* err) {
    if (path.empty()) {
        if (err) {
            *err = "empty descriptor-set path";
        }
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (err) {
            *err = "cannot open descriptor-set: " + path;
        }
        return false;
    }
    google::protobuf::FileDescriptorSet set;
    if (!set.ParseFromIstream(&in)) {
        if (err) {
            *err = "failed to parse FileDescriptorSet: " + path;
        }
        return false;
    }
    auto* factory = message::ProtobufFactory::Instance();
    for (int i = 0; i < set.file_size(); ++i) {
        // Already-registered files may return false; ignore and verify type later.
        (void)factory->RegisterMessage(set.file(i));
    }
    return set.file_size() > 0;
}

bool JsonToProtobufBytes(const std::string& type, const std::string& json,
                         std::string* out_bytes, std::string* err) {
    if (out_bytes == nullptr) {
        return false;
    }
    auto* factory = message::ProtobufFactory::Instance();
    std::unique_ptr<google::protobuf::Message> msg(
        factory->GenerateMessageByType(type));
    if (!msg) {
        if (err) {
            *err = "unknown type: " + type;
        }
        return false;
    }
    google::protobuf::util::JsonParseOptions opt;
    opt.ignore_unknown_fields = true;
    const auto status =
        google::protobuf::util::JsonStringToMessage(json, msg.get(), opt);
    if (!status.ok()) {
        if (err) {
            *err = status.ToString();
        }
        return false;
    }
    if (!msg->SerializeToString(out_bytes)) {
        if (err) {
            *err = "SerializeToString failed";
        }
        return false;
    }
    return true;
}

bool ProtobufBytesToJson(const std::string& type, const std::string& bytes,
                         std::string* out_json, std::string* err) {
    if (out_json == nullptr) {
        return false;
    }
    auto* factory = message::ProtobufFactory::Instance();
    std::unique_ptr<google::protobuf::Message> msg(
        factory->GenerateMessageByType(type));
    if (!msg || !msg->ParseFromString(bytes)) {
        if (err) {
            *err = "parse failed for type: " + type;
        }
        return false;
    }
    google::protobuf::util::JsonPrintOptions opt;
    opt.add_whitespace = true;
    // Protobuf <26: always_print_primitive_fields
    // Protobuf >=26: renamed to always_print_fields_with_no_presence
#if GOOGLE_PROTOBUF_VERSION >= 4026000
    opt.always_print_fields_with_no_presence = true;
#else
    opt.always_print_primitive_fields = true;
#endif
    const auto status =
        google::protobuf::util::MessageToJsonString(*msg, out_json, opt);
    if (!status.ok()) {
        if (err) {
            *err = status.ToString();
        }
        return false;
    }
    return true;
}

bool ProtobufBytesToDebug(const std::string& type, const std::string& bytes,
                          std::string* out_debug, std::string* err) {
    if (out_debug == nullptr) {
        return false;
    }
    auto* factory = message::ProtobufFactory::Instance();
    std::unique_ptr<google::protobuf::Message> msg(
        factory->GenerateMessageByType(type));
    if (!msg || !msg->ParseFromString(bytes)) {
        if (err) {
            *err = "parse failed for type: " + type;
        }
        return false;
    }
    *out_debug = msg->DebugString();
    return true;
}

bool ResolveChannelType(const std::string& channel_name, std::string* type,
                        std::string* proto_desc) {
    if (proto_desc) {
        proto_desc->clear();
    }
    if (type) {
        type->clear();
    }
    WaitForDiscovery();
    auto cm =
        service_discovery::TopologyManager::Instance()->channel_manager();
    std::string mt;
    std::string pd;
    if (cm->HasWriter(channel_name)) {
        cm->GetMsgType(channel_name, &mt);
        cm->GetProtoDesc(channel_name, &pd);
    }
    if (mt.empty() || mt == kRawMessageType) {
        std::vector<proto::RoleAttributes> writers;
        cm->GetWritersOfChannel(channel_name, &writers);
        for (const auto& attr : writers) {
            if (!attr.message_type().empty() &&
                attr.message_type() != kRawMessageType) {
                mt = attr.message_type();
                if (!attr.proto_desc().empty()) {
                    pd = attr.proto_desc();
                }
                break;
            }
            if (pd.empty() && !attr.proto_desc().empty()) {
                pd = attr.proto_desc();
            }
        }
    }
    if (mt.empty() || mt == kRawMessageType) {
        if (proto_desc) {
            *proto_desc = pd;
        }
        return false;
    }
    if (type) {
        *type = mt;
    }
    if (proto_desc) {
        *proto_desc = pd;
    }
    return type == nullptr || (type && !type->empty());
}

bool ResolveServiceType(const std::string& service_name, std::string* type,
                        std::string* proto_desc) {
    if (proto_desc) {
        proto_desc->clear();
    }
    if (type) {
        type->clear();
    }
    WaitForDiscovery();
    auto sm =
        service_discovery::TopologyManager::Instance()->service_manager();
    std::vector<proto::RoleAttributes> servers;
    sm->GetServers(&servers);
    for (const auto& attr : servers) {
        if (attr.service_name() != service_name) {
            continue;
        }
        if (!attr.message_type().empty() &&
            attr.message_type() != kRawMessageType) {
            if (type) {
                *type = attr.message_type();
            }
            if (proto_desc && !attr.proto_desc().empty()) {
                *proto_desc = attr.proto_desc();
            }
            return type == nullptr || (type && !type->empty());
        }
        if (proto_desc && proto_desc->empty() && !attr.proto_desc().empty()) {
            *proto_desc = attr.proto_desc();
        }
    }
    if (type) {
        return !type->empty();
    }
    return proto_desc && !proto_desc->empty();
}

}  // namespace tools
}  // namespace autolink
