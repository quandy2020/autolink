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

#include <signal.h>
#include <unistd.h>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "autolink/autolink.hpp"
#include "autolink/init.hpp"
#include "autolink/message/message_header.hpp"
#include "autolink/message/protobuf_factory.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/proto/topology_change.pb.h"
#include "autolink/service_discovery/specific_manager/channel_manager.hpp"
#include "autolink/service_discovery/topology_manager.hpp"
#include "autolink/state.hpp"
#include "autolink/time/time.hpp"

#include <CLI/CLI.hpp>

#include "autolink/tools/cli/cmd_channel.hpp"
#include "autolink/tools/cli/discovery_wait.hpp"
#include "autolink/tools/cli/proto_json.hpp"

namespace channel_cli {

constexpr int kDefaultBwWindowSize = 100;
constexpr int kDefaultHzWindowSize = 50000;
constexpr int kMaxWindowSize = 50000;


// -----------------------------------------------------------------------------
// list: list active channels (sorted)
// -----------------------------------------------------------------------------
void CmdList(bool verbose) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscovery();
    std::vector<std::string> channels;
    topology->channel_manager()->GetChannelNames(&channels);
    std::sort(channels.begin(), channels.end());
    std::cout << "The number of channels is: " << channels.size() << std::endl;
    for (const auto& ch : channels) {
        if (verbose) {
            std::string msg_type;
            topology->channel_manager()->GetMsgType(ch, &msg_type);
            std::cout << ch << " [" << msg_type << "]" << std::endl;
        } else {
            std::cout << ch << std::endl;
        }
    }
}

// -----------------------------------------------------------------------------
// type: print channel message type
// -----------------------------------------------------------------------------
void PrintChannelType(const std::string& channel_name) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscovery();
    std::string msg_type;
    topology->channel_manager()->GetMsgType(channel_name, &msg_type);
    std::cout << channel_name << " type is [ " << msg_type << " ]" << std::endl;
}

// -----------------------------------------------------------------------------
// info: print role attributes (roleid, hostname, processid, nodename, msgtype)
// -----------------------------------------------------------------------------
void PrintRole(const autolink::proto::RoleAttributes& attr) {
    std::cout << "\troleid\t\t" << attr.id() << std::endl;
    std::cout << "\thostname\t" << attr.host_name() << std::endl;
    std::cout << "\tprocessid\t" << attr.process_id() << std::endl;
    std::cout << "\tnodename\t" << attr.node_name() << std::endl;
    std::cout << "\tmsgtype\t\t" << attr.message_type() << std::endl;
}

void CmdInfo(const std::string& channel_name, bool all_channels) {
    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    autolink::tools::WaitForDiscovery();
    std::vector<autolink::proto::RoleAttributes> writers, readers;
    topology->channel_manager()->GetWriters(&writers);
    topology->channel_manager()->GetReaders(&readers);

    std::map<std::string, std::vector<autolink::proto::RoleAttributes>> info;
    for (auto& attr : writers) {
        info[attr.channel_name()].push_back(attr);
    }
    for (auto& attr : readers) {
        info[attr.channel_name()].push_back(attr);
    }

    if (info.empty()) {
        std::cout << "channelsinfo dict is null" << std::endl;
        return;
    }

    if (!channel_name.empty()) {
        auto it = info.find(channel_name);
        if (it != info.end()) {
            std::cout << channel_name << std::endl;
            for (const auto& attr : it->second) {
                PrintRole(attr);
            }
        }
        return;
    }

    std::vector<std::string> channels;
    for (const auto& p : info) {
        channels.push_back(p.first);
    }
    std::sort(channels.begin(), channels.end());
    std::cout << "The number of channels is: " << channels.size() << std::endl;
    for (const auto& ch : channels) {
        std::cout << ch << std::endl;
        for (const auto& attr : info[ch]) {
            PrintRole(attr);
        }
    }
}

// -----------------------------------------------------------------------------
// echo: subscribe and print messages as debug string by msg type
// -----------------------------------------------------------------------------
namespace {

constexpr char kRawMessageType[] = "autolink.message.RawMessage";

constexpr char kTwistStampedType[] =
    "autonomy.commsgs.proto.geometry_msgs.TwistStamped";

const char* KnownAutonomyChannelType(const std::string& channel_name) {
    if (channel_name == "/cmd_vel") {
        return kTwistStampedType;
    }
    return nullptr;
}

bool TryResolveChannelEchoSchema(
    const autolink::service_discovery::ChannelManagerPtr& channel_manager,
    const std::string& channel_name, std::string* msg_type,
    std::string* proto_desc) {
    if (!channel_manager || msg_type == nullptr || proto_desc == nullptr) {
        return false;
    }
    msg_type->clear();
    proto_desc->clear();

    if (channel_manager->HasWriter(channel_name)) {
        channel_manager->GetMsgType(channel_name, msg_type);
        channel_manager->GetProtoDesc(channel_name, proto_desc);
    }

    if (!msg_type->empty() && *msg_type != kRawMessageType) {
        if (proto_desc->empty()) {
            channel_manager->GetProtoDesc(channel_name, proto_desc);
        }
        return true;
    }

    std::vector<autolink::proto::RoleAttributes> writers;
    channel_manager->GetWritersOfChannel(channel_name, &writers);
    for (const auto& attr : writers) {
        if (!attr.message_type().empty() &&
            attr.message_type() != kRawMessageType) {
            *msg_type = attr.message_type();
            if (!attr.proto_desc().empty()) {
                *proto_desc = attr.proto_desc();
            }
            return true;
        }
        if (proto_desc->empty() && !attr.proto_desc().empty()) {
            *proto_desc = attr.proto_desc();
        }
    }

    if (msg_type->empty()) {
        if (const char* known = KnownAutonomyChannelType(channel_name)) {
            *msg_type = known;
        }
    }
    return !msg_type->empty() && *msg_type != kRawMessageType;
}

bool ResolveChannelEchoSchema(
    const autolink::service_discovery::ChannelManagerPtr& channel_manager,
    const std::string& channel_name, std::string* msg_type,
    std::string* proto_desc) {
    for (int retry = 0; retry < 30; ++retry) {
        if (TryResolveChannelEchoSchema(channel_manager, channel_name, msg_type,
                                        proto_desc)) {
            return true;
        }
        sleep(1);
    }
    return TryResolveChannelEchoSchema(channel_manager, channel_name, msg_type,
                                       proto_desc);
}

bool TryExtractHcPayload(const std::string& raw, std::string* type_out,
                         std::string* payload_out) {
    using autolink::message::MessageHeader;
    if (raw.size() < sizeof(MessageHeader)) {
        return false;
    }
    MessageHeader header;
    std::memcpy(&header, raw.data(), sizeof(MessageHeader));
    if (!header.is_magic_num_match("BDACBDAC", 8)) {
        return false;
    }
    const uint32_t content_size = header.content_size();
    const size_t header_size = sizeof(MessageHeader);
    if (raw.size() < header_size + content_size) {
        return false;
    }
    if (type_out != nullptr) {
        *type_out = header.msg_type();
    }
    if (payload_out != nullptr) {
        payload_out->assign(raw.data() + header_size, content_size);
    }
    return true;
}

void ApplyEchoSchema(const std::string& msg_type, const std::string& proto_desc,
                     std::string* schema, std::string* desc) {
    if (schema == nullptr || desc == nullptr || msg_type.empty()) {
        return;
    }
    *schema = msg_type;
    if (!proto_desc.empty()) {
        *desc = proto_desc;
    } else {
        autolink::message::ProtobufFactory::Instance()->GetDescriptorString(
            msg_type, desc);
    }
    if (!desc->empty()) {
        autolink::message::ProtobufFactory::Instance()->RegisterMessage(*desc);
    }
}

}  // namespace channel_cli

std::string GetDebugStringRawMsg(const std::string& msg_type,
                                 const std::string& rawmsgdata,
                                 const std::string& proto_desc) {
    if (rawmsgdata.empty()) {
        return "";
    }
    std::string type = msg_type;
    std::string payload = rawmsgdata;
    if (type.empty()) {
        TryExtractHcPayload(rawmsgdata, &type, &payload);
    }
    if (type.empty() || type == kRawMessageType) {
        return "";
    }
    auto* factory = autolink::message::ProtobufFactory::Instance();
    std::string desc = proto_desc;
    if (desc.empty()) {
        factory->GetDescriptorString(type, &desc);
    }
    if (!desc.empty()) {
        factory->RegisterMessage(desc);
    }
    google::protobuf::Message* msg = factory->GenerateMessageByType(type);
    if (!msg) {
        return "";
    }
    std::string result;
    if (msg->ParseFromString(payload)) {
        result = msg->DebugString();
    }
    delete msg;
    return result;
}

void CmdEcho(const std::string& channel_name, int max_messages) {
    auto node = autolink::CreateNode("listener_node_echo");
    if (!node) {
        std::cerr << "Failed to create node" << std::endl;
        return;
    }

    auto* topology = autolink::service_discovery::TopologyManager::Instance();
    auto& channel_manager = topology->channel_manager();

    std::string msg_type;
    std::string proto_desc;
    if (!TryResolveChannelEchoSchema(channel_manager, channel_name, &msg_type,
                                     &proto_desc)) {
        for (int retry = 0; retry < 3; ++retry) {
            sleep(1);
            if (TryResolveChannelEchoSchema(channel_manager, channel_name,
                                            &msg_type, &proto_desc)) {
                break;
            }
        }
    }
    if (msg_type.empty()) {
        std::cerr << "warning: no active writer on channel [" << channel_name
                  << "]; start publisher or playback first" << std::endl;
    }

    if (!proto_desc.empty()) {
        autolink::message::ProtobufFactory::Instance()->RegisterMessage(
            proto_desc);
    } else if (!msg_type.empty() && msg_type != kRawMessageType) {
        autolink::message::ProtobufFactory::Instance()->GetDescriptorString(
            msg_type, &proto_desc);
        if (!proto_desc.empty()) {
            autolink::message::ProtobufFactory::Instance()->RegisterMessage(
                proto_desc);
        }
    }

    if (msg_type.empty()) {
        std::cerr << "warning: message type unknown for channel ["
                  << channel_name << "]" << std::endl;
    } else if (msg_type == kRawMessageType) {
        std::cerr << "warning: writer advertises RawMessage; rebuild and "
                     "restart autolink recorder play"
                  << std::endl;
    } else {
        std::cout << "message type: " << msg_type << std::endl;
    }

    auto schema = std::make_shared<std::string>(msg_type);
    auto desc = std::make_shared<std::string>(proto_desc);
    auto warned = std::make_shared<bool>(false);
    auto msg_count = std::make_shared<std::atomic<int>>(0);
    auto channel_manager_ptr = channel_manager;
    if (schema->empty()) {
        std::string mt;
        std::string pd;
        if (TryResolveChannelEchoSchema(channel_manager_ptr, channel_name, &mt,
                                        &pd)) {
            ApplyEchoSchema(mt, pd, schema.get(), desc.get());
        }
    }

    auto change_conn = topology->AddChangeListener(
        [schema, desc, warned, channel_manager_ptr, channel_name](
            const autolink::proto::ChangeMsg& change) {
            if (change.role_type() != autolink::proto::ROLE_WRITER) {
                return;
            }
            if (change.role_attr().channel_name() != channel_name) {
                return;
            }
            std::string mt;
            std::string pd;
            if (!TryResolveChannelEchoSchema(channel_manager_ptr, channel_name,
                                             &mt, &pd)) {
                return;
            }
            ApplyEchoSchema(mt, pd, schema.get(), desc.get());
            *warned = false;
            if (!mt.empty()) {
                std::cout << "message type: " << mt << std::endl;
            }
        });

    auto callback =
        [schema, desc, warned, channel_manager_ptr, channel_name, msg_count,
         max_messages](
            const std::shared_ptr<const autolink::message::RawMessage>&
                raw_msg) {
            std::string data = raw_msg ? raw_msg->message : "";
            if (data.empty()) {
                return;
            }
            if (schema->empty()) {
                std::string mt;
                std::string pd;
                if (TryResolveChannelEchoSchema(channel_manager_ptr,
                                                channel_name, &mt, &pd)) {
                    ApplyEchoSchema(mt, pd, schema.get(), desc.get());
                    std::cout << "message type: " << mt << std::endl;
                    *warned = false;
                }
            }
            std::string debug = GetDebugStringRawMsg(*schema, data, *desc);
            if (!debug.empty()) {
                std::cout << debug << std::endl;
            } else if (!*warned) {
                *warned = true;
                if (schema->empty() || *schema == kRawMessageType) {
                    std::cerr << "warning: received " << data.size()
                              << " bytes but message type is not available"
                              << std::endl;
                } else if (!desc->empty()) {
                    std::cerr << "warning: received " << data.size()
                              << " bytes but failed to parse [" << *schema
                              << "]" << std::endl;
                } else {
                    std::cerr << "warning: received " << data.size()
                              << " bytes but proto_desc is missing (type ["
                              << *schema << "])" << std::endl;
                }
            }
            const int n = ++(*msg_count);
            if (max_messages > 0 && n >= max_messages) {
                autolink::AsyncShutdown();
            }
        };

    auto reader = node->CreateReader<autolink::message::RawMessage>(
        channel_name, callback);
    if (!reader) {
        std::cerr << "Failed to create reader for channel: " << channel_name
                  << std::endl;
        return;
    }
    std::cout << "reader to [" << channel_name << "]" << std::endl;
    while (!autolink::IsShutdown()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    topology->RemoveChangeListener(change_conn);
}

void CmdPub(const std::string& channel, const std::string& json,
            const std::string& type_flag, const std::string& descriptor_set,
            double rate, int times) {
    std::string type;
    std::string desc;
    std::string err;
    if (!descriptor_set.empty()) {
        if (!autolink::tools::RegisterDescriptorSetFile(descriptor_set,
                                                        &err)) {
            std::cerr << err << std::endl;
            throw CLI::RuntimeError(1);
        }
    }
    if (!type_flag.empty()) {
        type = type_flag;
        autolink::tools::ResolveChannelType(channel, nullptr, &desc);
    } else if (!autolink::tools::ResolveChannelType(channel, &type, &desc) ||
               type.empty()) {
        std::cerr << "cannot resolve type; pass --type" << std::endl;
        throw CLI::RuntimeError(1);
    }
    if (!autolink::tools::RegisterProtoType(type, desc)) {
        std::cerr << "failed to register type: " << type
                  << " (try --descriptor-set from protoc)" << std::endl;
        throw CLI::RuntimeError(1);
    }
    std::string bytes;
    if (!autolink::tools::JsonToProtobufBytes(type, json, &bytes, &err)) {
        std::cerr << err << std::endl;
        throw CLI::RuntimeError(1);
    }
    auto node = autolink::CreateNode("autolink_cli_pub");
    if (!node) {
        throw CLI::RuntimeError(1);
    }
    autolink::proto::RoleAttributes attr;
    attr.set_channel_name(channel);
    attr.set_message_type(type);
    if (!desc.empty()) {
        attr.set_proto_desc(desc);
    }
    auto writer =
        node->CreateWriter<autolink::message::RawMessage>(attr);
    if (!writer) {
        throw CLI::RuntimeError(1);
    }
    if (times < 1) {
        times = 1;
    }
    const auto period = std::chrono::duration<double>(
        rate > 0.0 ? (1.0 / rate) : 1.0);
    for (int i = 0; i < times && !autolink::IsShutdown(); ++i) {
        writer->Write(std::make_shared<autolink::message::RawMessage>(bytes));
        if (i + 1 < times) {
            std::this_thread::sleep_for(period);
        }
    }
}

// -----------------------------------------------------------------------------
// bw: bandwidth stats (window of message sizes, print rate every second)
// -----------------------------------------------------------------------------
class ChannelBw
{
public:
    explicit ChannelBw(int window_size)
        : window_size_(window_size <= 0 || window_size > kMaxWindowSize
                           ? kDefaultBwWindowSize
                           : window_size) {
        std::cout << "bw window_size: " << window_size_ << std::endl;
    }

    void OnData(const std::string& rawdata) {
        std::lock_guard<std::mutex> lock(mutex_);
        double t = autolink::Time::Now().ToSecond();
        times_.push_back(t);
        sizes_.push_back(static_cast<int>(rawdata.size()));
        while (static_cast<int>(times_.size()) > window_size_) {
            times_.erase(times_.begin());
            sizes_.erase(sizes_.begin());
        }
    }

    void Print() {
        std::vector<double> times;
        std::vector<int> sizes;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (times_.size() < 2)
                return;
            times = times_;
            sizes = sizes_;
        }
        double tn = autolink::Time::Now().ToSecond();
        double t0 = times.front();
        int total = 0;
        for (int s : sizes)
            total += s;
        int n = static_cast<int>(sizes.size());
        double bytes_per_s = (tn > t0) ? (total / (tn - t0)) : 0;
        double mean = (n > 0) ? static_cast<double>(total) / n : 0;
        int max_s = *std::max_element(sizes.begin(), sizes.end());
        int min_s = *std::min_element(sizes.begin(), sizes.end());

        auto fmt = [](double v, double scale, const char* unit) -> std::string {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f%s", v / scale, unit);
            return buf;
        };
        std::string bw_str, mean_str, min_str, max_str;
        if (bytes_per_s < 1000) {
            bw_str = fmt(bytes_per_s, 1, "B");
            mean_str = fmt(mean, 1, "B");
            min_str = fmt(static_cast<double>(min_s), 1, "B");
            max_str = fmt(static_cast<double>(max_s), 1, "B");
        } else if (bytes_per_s < 1000000) {
            bw_str = fmt(bytes_per_s, 1000, "KB");
            mean_str = fmt(mean, 1000, "KB");
            min_str = fmt(static_cast<double>(min_s), 1000, "KB");
            max_str = fmt(static_cast<double>(max_s), 1000, "KB");
        } else {
            bw_str = fmt(bytes_per_s, 1000000, "MB");
            mean_str = fmt(mean, 1000000, "MB");
            min_str = fmt(static_cast<double>(min_s), 1000000, "MB");
            max_str = fmt(static_cast<double>(max_s), 1000000, "MB");
        }
        std::cout << "average: " << bw_str << "/s\n\tmean: " << mean_str
                  << " min: " << min_str << " max: " << max_str
                  << " window: " << n << std::endl;
    }

private:
    int window_size_;
    std::mutex mutex_;
    std::vector<double> times_;
    std::vector<int> sizes_;
};

void CmdBw(const std::string& channel_name, int window_size) {
    auto bw = std::make_shared<ChannelBw>(window_size);
    auto node = autolink::CreateNode("listener_node_bw");
    if (!node) {
        std::cerr << "Failed to create node" << std::endl;
        return;
    }
    auto cb =
        [bw](const std::shared_ptr<const autolink::message::RawMessage>& m) {
            if (m)
                bw->OnData(m->message);
        };
    auto reader =
        node->CreateReader<autolink::message::RawMessage>(channel_name, cb);
    if (!reader) {
        std::cerr << "Failed to create reader for channel: " << channel_name
                  << std::endl;
        return;
    }
    std::cout << "reader to [" << channel_name << "]" << std::endl;
    while (!autolink::IsShutdown()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        bw->Print();
    }
}

// -----------------------------------------------------------------------------
// hz: frequency stats (inter-message intervals, print rate every second)
// -----------------------------------------------------------------------------
class ChannelHz
{
public:
    explicit ChannelHz(int window_size)
        : window_size_(window_size <= 0 || window_size > kMaxWindowSize
                           ? kDefaultHzWindowSize
                           : window_size),
          last_printed_tn_(0),
          msg_t0_(-1),
          msg_tn_(0) {
        std::cout << "hz window_size: " << window_size_ << std::endl;
    }

    void OnMessage() {
        std::lock_guard<std::mutex> lock(mutex_);
        double curr = autolink::Time::Now().ToSecond();
        if (curr == 0) {
            if (!times_.empty()) {
                std::cout << "reset times." << std::endl;
                times_.clear();
            }
            return;
        }
        if (msg_t0_ < 0 || msg_t0_ > curr) {
            msg_t0_ = curr;
            msg_tn_ = curr;
            times_.clear();
        } else {
            times_.push_back(curr - msg_tn_);
            msg_tn_ = curr;
        }
        while (static_cast<int>(times_.size()) > window_size_ - 1) {
            times_.erase(times_.begin());
        }
    }

    void Print() {
        std::vector<double> times;
        double msg_tn_snapshot = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (times_.empty())
                return;
            if (msg_tn_ == last_printed_tn_) {
                std::cout << "no new messages" << std::endl;
                return;
            }
            times = times_;
            msg_tn_snapshot = msg_tn_;
            last_printed_tn_ = msg_tn_;
        }
        int n = static_cast<int>(times.size());
        double mean = 0;
        for (double t : times)
            mean += t;
        mean /= n;
        double rate = (mean > 0) ? (1.0 / mean) : 0;
        double var = 0;
        for (double t : times)
            var += (t - mean) * (t - mean);
        double std_dev = std::sqrt(var / n);
        double max_delta = *std::max_element(times.begin(), times.end());
        double min_delta = *std::min_element(times.begin(), times.end());
        (void)msg_tn_snapshot;
        std::cout << "average rate: " << std::fixed << std::setprecision(3)
                  << rate << "\n\tmin: " << std::setprecision(3) << min_delta
                  << "s max: " << max_delta
                  << "s std dev: " << std::setprecision(5) << std_dev
                  << "s window: " << (n + 1) << std::endl;
    }

private:
    int window_size_;
    std::mutex mutex_;
    double last_printed_tn_;
    double msg_t0_;
    double msg_tn_;
    std::vector<double> times_;
};

void CmdHz(const std::string& channel_name, int window_size) {
    auto hz = std::make_shared<ChannelHz>(window_size);
    auto node = autolink::CreateNode("listener_node_hz");
    if (!node) {
        std::cerr << "Failed to create node" << std::endl;
        return;
    }
    auto cb =
        [hz](const std::shared_ptr<const autolink::message::RawMessage>&) {
            hz->OnMessage();
        };
    auto reader =
        node->CreateReader<autolink::message::RawMessage>(channel_name, cb);
    if (!reader) {
        std::cerr << "Failed to create reader for channel: " << channel_name
                  << std::endl;
        return;
    }
    std::cout << "reader to [" << channel_name << "]" << std::endl;
    while (!autolink::IsShutdown()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        hz->Print();
    }
}

void InstallShutdownHandlers() {
    std::signal(SIGINT, [](int sig) { autolink::OnShutdown(sig); });
    std::signal(SIGTERM, [](int sig) { autolink::OnShutdown(sig); });
}

void InitRuntime() {
    InstallShutdownHandlers();
    FLAGS_minloglevel = 3;
    FLAGS_alsologtostderr = 0;
    FLAGS_colorlogtostderr = 0;
    autolink::Init("autolink");
}

}  // namespace channel_cli

namespace autolink {
namespace tools {

void SetupChannel(CLI::App& app) {
    auto* channel =
        app.add_subcommand("channel", "Introspect Autolink channels");
    channel->require_subcommand(1);

    auto* list = channel->add_subcommand("list", "List active channels");
    auto list_verbose = std::make_shared<bool>(false);
    list->add_flag("-v,--verbose", *list_verbose, "Show message type");
    list->callback([list_verbose]() {
        channel_cli::InitRuntime();
        channel_cli::CmdList(*list_verbose);
        autolink::Clear();
    });

    auto* type = channel->add_subcommand("type", "Print channel type");
    auto type_name = std::make_shared<std::string>();
    type->add_option("channel", *type_name, "Channel name")->required();
    type->callback([type_name]() {
        channel_cli::InitRuntime();
        channel_cli::PrintChannelType(*type_name);
        autolink::Clear();
    });

    auto* info = channel->add_subcommand("info", "Print channel info");
    auto all = std::make_shared<bool>(false);
    auto info_name = std::make_shared<std::string>();
    info->add_flag("-a,--all", *all, "Show all channels");
    info->add_option("channel", *info_name, "Channel name");
    info->callback([all, info_name]() {
        if (!*all && info_name->empty()) {
            throw CLI::ValidationError(
                "info", "channelname must be specified (or use -a/--all)");
        }
        channel_cli::InitRuntime();
        channel_cli::CmdInfo(*info_name, *all);
        autolink::Clear();
    });

    auto* echo = channel->add_subcommand("echo", "Print messages to screen");
    auto echo_name = std::make_shared<std::string>();
    auto echo_once = std::make_shared<bool>(false);
    auto echo_times = std::make_shared<int>(0);
    echo->add_option("channel", *echo_name, "Channel name")->required();
    echo->add_flag("--once", *echo_once, "Exit after one message");
    echo->add_option("-n,--times", *echo_times, "Exit after N messages");
    echo->callback([echo_name, echo_once, echo_times]() {
        channel_cli::InitRuntime();
        int max_messages = *echo_once ? 1 : *echo_times;
        channel_cli::CmdEcho(*echo_name, max_messages);
        autolink::Clear();
    });

    auto* pub = channel->add_subcommand("pub", "Publish a JSON message");
    auto pub_channel = std::make_shared<std::string>();
    auto pub_json = std::make_shared<std::string>();
    auto pub_type = std::make_shared<std::string>();
    auto pub_fdset = std::make_shared<std::string>();
    auto pub_rate = std::make_shared<double>(1.0);
    auto pub_times = std::make_shared<int>(1);
    pub->add_option("channel", *pub_channel, "Channel name")->required();
    pub->add_option("json", *pub_json, "Message JSON")->required();
    pub->add_option("--type", *pub_type, "Protobuf message type");
    pub->add_option("--descriptor-set", *pub_fdset,
                    "FileDescriptorSet from protoc --descriptor_set_out");
    pub->add_option("--rate", *pub_rate, "Publish rate (Hz)");
    pub->add_option("--times", *pub_times, "Number of messages");
    pub->callback(
        [pub_channel, pub_json, pub_type, pub_fdset, pub_rate, pub_times]() {
            channel_cli::InitRuntime();
            try {
                channel_cli::CmdPub(*pub_channel, *pub_json, *pub_type,
                                    *pub_fdset, *pub_rate, *pub_times);
            } catch (const CLI::RuntimeError&) {
                autolink::Clear();
                throw;
            }
            autolink::Clear();
        });

    auto* bw = channel->add_subcommand("bw", "Display bandwidth");
    auto bw_name = std::make_shared<std::string>();
    auto bw_window = std::make_shared<int>(channel_cli::kDefaultBwWindowSize);
    bw->add_option("channel", *bw_name, "Channel name")->required();
    bw->add_option("-w,--window", *bw_window, "Window size");
    bw->callback([bw_name, bw_window]() {
        channel_cli::InitRuntime();
        channel_cli::CmdBw(*bw_name, *bw_window);
        autolink::Clear();
    });

    auto* hz = channel->add_subcommand("hz", "Display publishing rate");
    auto hz_name = std::make_shared<std::string>();
    auto hz_window = std::make_shared<int>(channel_cli::kDefaultHzWindowSize);
    hz->add_option("channel", *hz_name, "Channel name")->required();
    hz->add_option("-w,--window", *hz_window, "Window size");
    hz->callback([hz_name, hz_window]() {
        channel_cli::InitRuntime();
        channel_cli::CmdHz(*hz_name, *hz_window);
        autolink::Clear();
    });
}

}  // namespace tools
}  // namespace autolink
