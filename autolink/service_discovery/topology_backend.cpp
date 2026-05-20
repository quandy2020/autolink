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

#include "autolink/service_discovery/topology_backend.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"

namespace autolink {
namespace service_discovery {

namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(100);
constexpr auto kHeartbeatInterval = std::chrono::seconds(1);
constexpr auto kCleanupInterval = std::chrono::seconds(2);
constexpr auto kProcessLease = std::chrono::seconds(5);
constexpr auto kCompactInterval = std::chrono::seconds(10);
constexpr off_t kMaxEventFileSize = 4 * 1024 * 1024;

std::string ToHex(const std::string& input) {
    static const char kHex[] = "0123456789abcdef";
    std::string output;
    output.reserve(input.size() * 2);
    for (unsigned char c : input) {
        output.push_back(kHex[c >> 4]);
        output.push_back(kHex[c & 0x0F]);
    }
    return output;
}

bool FromHex(const std::string& input, std::string* output) {
    if (output == nullptr || input.size() % 2 != 0) {
        return false;
    }
    auto hex_to_int = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };

    output->clear();
    output->reserve(input.size() / 2);
    for (size_t i = 0; i < input.size(); i += 2) {
        const int hi = hex_to_int(input[i]);
        const int lo = hex_to_int(input[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        output->push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

}  // namespace

bool LocalTopologyBackend::Start() {
    if (running_.exchange(true)) {
        return true;
    }
    {
        read_offset_ = 0;
    }
    self_host_name_ = common::GlobalData::Instance()->HostName();
    self_process_id_ = common::GlobalData::Instance()->ProcessId();
    self_process_key_ = self_host_name_ + "+" + std::to_string(self_process_id_);
    const auto now = std::chrono::steady_clock::now();
    next_heartbeat_at_ = now;
    next_cleanup_at_ = now + kCleanupInterval;
    next_compact_at_ = now + kCompactInterval;
    poll_thread_ = std::thread(&LocalTopologyBackend::PollLoop, this);
    return true;
}

int64_t LocalTopologyBackend::Subscribe(proto::ChangeType type,
                                        const ChangeCallback& callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    const int64_t subscription_id = next_subscription_id_++;
    subscribers_[subscription_id] = Subscriber{type, callback};
    return subscription_id;
}

void LocalTopologyBackend::Unsubscribe(int64_t subscription_id) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.erase(subscription_id);
}

bool LocalTopologyBackend::Publish(const proto::ChangeMsg& msg) {
    std::string payload;
    if (!msg.SerializeToString(&payload)) {
        AERROR << "Failed to serialize topology change message.";
        return false;
    }
    const std::string line = ToHex(payload) + "\n";

    std::lock_guard<std::mutex> lock(publish_mutex_);
    const int fd = open(event_file_path_.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        AERROR << "Failed to open topology backend file: " << event_file_path_;
        return false;
    }
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        AERROR << "Failed to lock topology backend file.";
        return false;
    }

    const ssize_t write_size = write(fd, line.data(), line.size());
    flock(fd, LOCK_UN);
    close(fd);

    if (write_size != static_cast<ssize_t>(line.size())) {
        AERROR << "Failed to write topology backend event.";
        return false;
    }
    return true;
}

void LocalTopologyBackend::Shutdown() {
    if (!running_.exchange(false)) {
        return;
    }
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.clear();
}

void LocalTopologyBackend::PollLoop() {
    while (running_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_heartbeat_at_) {
            PublishHeartbeat();
            next_heartbeat_at_ = now + kHeartbeatInterval;
        }
        if (now >= next_cleanup_at_) {
            CleanupStaleProcesses();
            next_cleanup_at_ = now + kCleanupInterval;
        }
        if (now >= next_compact_at_) {
            MaybeCompactEventFile();
            next_compact_at_ = now + kCompactInterval;
        }

        std::ifstream fin(event_file_path_, std::ios::binary);
        if (fin.good()) {
            fin.seekg(0, std::ios::end);
            const std::streamoff file_end =
                static_cast<std::streamoff>(fin.tellg());
            if (read_offset_ > file_end) {
                read_offset_ = 0;
            }
            fin.seekg(read_offset_, std::ios::beg);
            std::string line;
            while (std::getline(fin, line)) {
                std::string bytes;
                if (!FromHex(line, &bytes)) {
                    continue;
                }
                proto::ChangeMsg msg;
                if (!msg.ParseFromString(bytes)) {
                    continue;
                }
                HandleIncomingMessage(msg);
                DispatchMessage(msg);
            }
            fin.clear();
            fin.seekg(0, std::ios::end);
            read_offset_ = fin.tellg();
        }

        std::this_thread::sleep_for(kPollInterval);
    }
}

void LocalTopologyBackend::DispatchMessage(const proto::ChangeMsg& msg) {
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        for (const auto& entry : subscribers_) {
            if (entry.second.type == msg.change_type()) {
                callbacks.emplace_back(entry.second.callback);
            }
        }
    }
    for (const auto& callback : callbacks) {
        if (callback) {
            callback(msg);
        }
    }
}

void LocalTopologyBackend::HandleIncomingMessage(const proto::ChangeMsg& msg) {
    const auto& attr = msg.role_attr();
    if (attr.host_name().empty() || attr.process_id() == 0) {
        return;
    }
    const std::string process_key = ProcessKey(attr);
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(state_mutex_);
    auto& state = process_states_[process_key];
    state.last_seen = now;

    if (msg.change_type() == proto::ChangeType::CHANGE_PARTICIPANT &&
        msg.role_type() == proto::RoleType::ROLE_PARTICIPANT) {
        if (msg.operate_type() == proto::OperateType::OPT_LEAVE) {
            process_states_.erase(process_key);
        }
        return;
    }

    const std::string role_key = RoleKey(msg);
    if (msg.operate_type() == proto::OperateType::OPT_JOIN) {
        state.active_roles[role_key] = msg;
    } else if (msg.operate_type() == proto::OperateType::OPT_LEAVE) {
        state.active_roles.erase(role_key);
    }
}

void LocalTopologyBackend::PublishHeartbeat() {
    proto::ChangeMsg msg;
    msg.set_timestamp(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count()));
    msg.set_change_type(proto::ChangeType::CHANGE_PARTICIPANT);
    msg.set_operate_type(proto::OperateType::OPT_JOIN);
    msg.set_role_type(proto::RoleType::ROLE_PARTICIPANT);
    auto* attr = msg.mutable_role_attr();
    attr->set_host_name(self_host_name_);
    attr->set_process_id(self_process_id_);
    (void)Publish(msg);
}

void LocalTopologyBackend::CleanupStaleProcesses() {
    std::vector<proto::ChangeMsg> stale_leave_msgs;
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        for (auto it = process_states_.begin(); it != process_states_.end();) {
            if (it->first == self_process_key_) {
                ++it;
                continue;
            }
            if (now - it->second.last_seen <= kProcessLease) {
                ++it;
                continue;
            }
            for (const auto& role_entry : it->second.active_roles) {
                proto::ChangeMsg leave_msg = role_entry.second;
                leave_msg.set_operate_type(proto::OperateType::OPT_LEAVE);
                leave_msg.set_timestamp(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count()));
                stale_leave_msgs.emplace_back(std::move(leave_msg));
            }
            it = process_states_.erase(it);
        }
    }
    for (const auto& msg : stale_leave_msgs) {
        (void)Publish(msg);
    }
}

void LocalTopologyBackend::MaybeCompactEventFile() {
    struct stat st = {};
    if (stat(event_file_path_.c_str(), &st) != 0 || st.st_size <= kMaxEventFileSize) {
        return;
    }

    std::vector<proto::ChangeMsg> snapshot_msgs;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        for (const auto& proc_entry : process_states_) {
            for (const auto& role_entry : proc_entry.second.active_roles) {
                snapshot_msgs.push_back(role_entry.second);
            }
        }
    }

    std::lock_guard<std::mutex> write_lock(publish_mutex_);
    const int fd = open(event_file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return;
    }
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return;
    }

    std::streamoff snapshot_size = 0;
    for (const auto& msg : snapshot_msgs) {
        std::string payload;
        if (!msg.SerializeToString(&payload)) {
            continue;
        }
        const std::string line = ToHex(payload) + "\n";
        const ssize_t write_size = write(fd, line.data(), line.size());
        if (write_size == static_cast<ssize_t>(line.size())) {
            snapshot_size += static_cast<std::streamoff>(line.size());
        }
    }

    flock(fd, LOCK_UN);
    close(fd);
    read_offset_ = snapshot_size;
}

std::string LocalTopologyBackend::ProcessKey(
    const proto::RoleAttributes& attr) const {
    return attr.host_name() + "+" + std::to_string(attr.process_id());
}

std::string LocalTopologyBackend::RoleKey(const proto::ChangeMsg& msg) const {
    const auto& attr = msg.role_attr();
    std::ostringstream oss;
    oss << static_cast<int>(msg.change_type()) << ":"
        << static_cast<int>(msg.role_type()) << ":";
    switch (msg.role_type()) {
        case proto::RoleType::ROLE_NODE:
            oss << attr.node_id();
            break;
        case proto::RoleType::ROLE_WRITER:
        case proto::RoleType::ROLE_READER:
            oss << attr.channel_id() << ":" << attr.id();
            break;
        case proto::RoleType::ROLE_SERVER:
        case proto::RoleType::ROLE_CLIENT:
            oss << attr.service_id() << ":" << attr.id();
            break;
        default:
            oss << attr.id();
            break;
    }
    return oss.str();
}

}  // namespace service_discovery
}  // namespace autolink
