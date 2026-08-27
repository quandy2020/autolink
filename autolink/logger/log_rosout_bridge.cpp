/******************************************************************************
 * Copyright 2026 The Openbot Authors (duyongquan)
 *****************************************************************************/

#include "autolink/logger/log_rosout_bridge.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>

#include "autolink/autolink.hpp"
#include "autolink/common/global_data.hpp"
#include "autolink/message/raw_message.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/state.hpp"

namespace autolink {
namespace logger {
namespace {

constexpr char kRosoutChannel[] = "/rosout";
constexpr char kLogMessageType[] = "foxglove.Log";

std::mutex g_mutex;
std::shared_ptr<Node> g_node;
std::shared_ptr<Writer<message::RawMessage>> g_writer;
std::atomic<bool> g_busy{false};

int FoxgloveLevel(int severity_level) {
  // AsyncLogger maps F/E/W/I -> 3/2/1/0
  switch (severity_level) {
    case 0:
      return 2;  // INFO
    case 1:
      return 3;  // WARN
    case 2:
      return 4;  // ERROR
    case 3:
      return 5;  // FATAL
    default:
      return 2;
  }
}

std::string EscapeJson(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 8);
  for (const char ch : input) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += ch;
        break;
    }
  }
  return out;
}

bool EnsureWriterLocked() {
  if (g_writer != nullptr) {
    return true;
  }
  if (GetState() != STATE_INITIALIZED) {
    return false;
  }
  const std::string process =
      common::GlobalData::Instance() != nullptr
          ? common::GlobalData::Instance()->ProcessGroup()
          : std::string("autolink");
  const std::string node_name = std::string("/") + process + "/rosout_bridge";
  g_node = CreateNode(node_name);
  if (g_node == nullptr) {
    return false;
  }
  proto::RoleAttributes attr;
  attr.set_channel_name(kRosoutChannel);
  attr.set_message_type(kLogMessageType);
  attr.mutable_qos_profile()->set_depth(50);
  g_writer = g_node->CreateWriter<message::RawMessage>(attr);
  return g_writer != nullptr;
}

}  // namespace

void BroadcastLogToRosout(int severity_level, const std::string& module_name,
                          const std::string& message, int64_t timestamp_sec) {
  if (message.empty()) {
    return;
  }
  if (g_busy.exchange(true)) {
    return;
  }

  std::shared_ptr<Writer<message::RawMessage>> writer;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!EnsureWriterLocked()) {
      g_busy.store(false);
      return;
    }
    writer = g_writer;
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
  const int64_t sec =
      timestamp_sec > 0 ? timestamp_sec : (ns / 1000000000LL);
  const int64_t nsec = ns % 1000000000LL;

  std::ostringstream json;
  json << "{\"timestamp\":{\"sec\":" << sec << ",\"nsec\":" << nsec
       << "},\"level\":" << FoxgloveLevel(severity_level) << ",\"message\":\""
       << EscapeJson(message) << "\",\"name\":\""
       << EscapeJson(module_name.empty() ? "autolink" : module_name) << "\"}";

  auto raw = std::make_shared<message::RawMessage>(json.str());
  writer->Write(raw);
  g_busy.store(false);
}

}  // namespace logger
}  // namespace autolink
