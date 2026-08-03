# Autolink CLI P1 调试闭环 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为统一 CLI 补齐 `param`、`service call`、可用的 `action send_goal`、`channel pub`，以及 `echo --once/-n`。

**Architecture:** 共享 `cli/proto_json`（JSON↔protobuf + 拓扑类型注册）；各 `cmd_*.cpp` 扩展子命令。动态消息走 `RawMessage` / `proto::SendGoalRequest`；param 走既有 `ParameterClient`。

**Tech Stack:** C++17、CLI11、protobuf `JsonStringToMessage` / `MessageToJsonString`、`ProtobufFactory`、autolink Node/Client/Writer

**Spec:** `docs/superpowers/specs/2026-08-03-autolink-cli-p1-debug-design.md`

**Commits:** 仅在用户明确要求时提交。

**Build:** Docker `SpaceHero`，仓库 `/workspace/autonomy/autolink`，目标 `autolink_cli`，产物 `build/bin/autolink`。

---

## File map

| Path | Role |
|------|------|
| `autolink/tools/cli/proto_json.{hpp,cpp}` | JSON↔Message、RegisterType、拓扑取 channel/service type |
| `autolink/tools/cli/cmd_param.{hpp,cpp}` | `SetupParam`：list/get/set |
| `autolink/tools/cli/cmd_service.cpp` | 增加 `call` |
| `autolink/tools/cli/cmd_action.cpp` | 重写 `send_goal` |
| `autolink/tools/cli/cmd_channel.cpp` | 增加 `pub`；`echo` 加 `--once`/`-n` |
| `autolink/tools/main.cpp` | `SetupParam` |
| `autolink/tools/CMakeLists.txt` | 编入 `proto_json.cpp`、`cmd_param.cpp` |

---

### Task 1: `proto_json` 工具

**Files:**
- Create: `autolink/autolink/tools/cli/proto_json.hpp`
- Create: `autolink/autolink/tools/cli/proto_json.cpp`
- Modify: `autolink/autolink/tools/CMakeLists.txt`（先只加 `cli/proto_json.cpp` 到 `AUTOLINK_CLI_SOURCES`；若尚无引用可暂不链，或在 Task 2 一起加）

- [ ] **Step 1: 写 `proto_json.hpp`**

```cpp
#pragma once

#include <memory>
#include <string>

#include "google/protobuf/message.h"

namespace autolink {
namespace tools {

// Register type with factory; desc may be empty (lookup by type name).
bool RegisterProtoType(const std::string& type, const std::string& desc = "");

// JSON -> serialized protobuf bytes for `type`. Returns false on failure; sets *err.
bool JsonToProtobufBytes(const std::string& type, const std::string& json,
                         std::string* out_bytes, std::string* err);

// Serialized bytes -> JSON string (protobuf JSON mapping).
bool ProtobufBytesToJson(const std::string& type, const std::string& bytes,
                         std::string* out_json, std::string* err);

// Serialized bytes -> DebugString (fallback printer).
bool ProtobufBytesToDebug(const std::string& type, const std::string& bytes,
                          std::string* out_debug, std::string* err);

// From channel topology writers: message_type + proto_desc.
bool ResolveChannelType(const std::string& channel_name, std::string* type,
                        std::string* proto_desc);

// From service topology servers: message_type + proto_desc (best-effort).
bool ResolveServiceType(const std::string& service_name, std::string* type,
                        std::string* proto_desc);

// Prefer --type if non-empty; else topology; register desc; return false if empty.
bool ResolveTypeOrFlag(const std::string& flag_type,
                       bool (*resolver)(const std::string&, std::string*,
                                        std::string*),
                       const std::string& name, std::string* type,
                       std::string* err);

}  // namespace tools
}  // namespace autolink
```

- [ ] **Step 2: 写 `proto_json.cpp`**

要点：

```cpp
#include "autolink/tools/cli/proto_json.hpp"

#include "google/protobuf/util/json_util.h"

#include "autolink/message/protobuf_factory.hpp"
#include "autolink/proto/role_attributes.pb.h"
#include "autolink/service_discovery/topology_manager.hpp"

bool RegisterProtoType(const std::string& type, const std::string& desc) {
    auto* factory = message::ProtobufFactory::Instance();
    if (!desc.empty()) {
        factory->RegisterMessage(desc);
    } else {
        std::string d;
        factory->GetDescriptorString(type, &d);
        if (!d.empty()) factory->RegisterMessage(d);
    }
    return factory->GenerateMessageByType(type) != nullptr;
}

bool JsonToProtobufBytes(...) {
    auto* factory = message::ProtobufFactory::Instance();
    std::unique_ptr<google::protobuf::Message> msg(
        factory->GenerateMessageByType(type));
    if (!msg) { *err = "unknown type: " + type; return false; }
    google::protobuf::util::JsonParseOptions opt;
    opt.ignore_unknown_fields = true;
    auto status = google::protobuf::util::JsonStringToMessage(json, msg.get(), opt);
    if (!status.ok()) { *err = status.ToString(); return false; }
    return msg->SerializeToString(out_bytes);
}

bool ProtobufBytesToJson(...) {
    // GenerateMessageByType + ParseFromString + MessageToJsonString
}

bool ResolveChannelType(const std::string& channel_name, ...) {
    sleep(1); // brief discovery; P2 will add --wait
    auto* cm = service_discovery::TopologyManager::Instance()->channel_manager();
    // GetWriters, find channel_name, copy message_type() and proto_desc()
}

bool ResolveServiceType(...) {
    sleep(1);
    // GetServers, match service_name, message_type / proto_desc
}
```

`ResolveTypeOrFlag`：若 `flag_type` 非空则 `*type = flag_type` 并 `RegisterProtoType`；否则调 `resolver`；失败写 `err`。

对照 `cmd_channel.cpp` 里已有的 `TryResolveChannelEchoSchema` / `ApplyEchoSchema`，逻辑对齐，避免重复踩坑（含 RawMessage 类型名跳过）。

- [ ] **Step 3: CMake 加入源文件**

在 `AUTOLINK_CLI_SOURCES` 增加：

```cmake
  cli/proto_json.cpp
  cli/cmd_param.cpp
```

（`cmd_param` 可在 Task 2 创建；若本步尚未有文件，可先只加 `proto_json.cpp`。）

- [ ] **Step 4: 编译**

```bash
docker exec SpaceHero bash -lc \
  'cd /workspace/autonomy/autolink/build && cmake .. >/dev/null && cmake --build . --target autolink_cli -j$(nproc)'
```

Expected: 成功（若仅 proto_json 且未被引用，仍应能链上）。

---

### Task 2: `param` 命令

**Files:**
- Create: `autolink/autolink/tools/cli/cmd_param.hpp`
- Create: `autolink/autolink/tools/cli/cmd_param.cpp`
- Modify: `autolink/autolink/tools/main.cpp`
- Modify: `autolink/autolink/tools/CMakeLists.txt`（确保含 `cli/cmd_param.cpp`）

- [ ] **Step 1: `cmd_param.hpp`** — 同其他 cmd：`void SetupParam(CLI::App& app);`

- [ ] **Step 2: 实现 `SetupParam`**

```cpp
void SetupParam(CLI::App& app) {
    auto* param = app.add_subcommand("param", "Get/set/list node parameters");
    param->require_subcommand(1);

    auto* list = param->add_subcommand("list", "List parameters of a node");
    auto node = std::make_shared<std::string>();
    list->add_option("node", *node, "Parameter server node name")->required();
    list->callback([node]() {
        // Init + CreateNode("autolink_cli_param") + ParameterClient(n, *node)
        // ListParameters -> print name + DebugString / TypeName
        // fail -> throw CLI::RuntimeError(1)
    });

    auto* get = param->add_subcommand("get", "Get one parameter");
    auto gnode = std::make_shared<std::string>();
    auto gname = std::make_shared<std::string>();
    get->add_option("node", *gnode)->required();
    get->add_option("name", *gname)->required();
    get->callback(... GetParameter + print DebugString ...);

    auto* set = param->add_subcommand("set", "Set one parameter");
    // node, name, value
    set->callback([...]() {
        Parameter p = ParseParamValue(*name, *value); // helpers below
        if (!client.SetParameter(p)) throw CLI::RuntimeError(1);
        std::cout << "set OK\n";
    });
}
```

**ParseParamValue**（匿名命名空间）：

```cpp
Parameter ParseParamValue(const std::string& name, const std::string& value) {
    std::string v = value;
    // trim optional
    std::string lower = v;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "true") return Parameter(name, true);
    if (lower == "false") return Parameter(name, false);
    if (v.find_first_of(".eE") == std::string::npos) {
        try { return Parameter(name, static_cast<int64_t>(std::stoll(v))); }
        catch (...) {}
    }
    try { return Parameter(name, std::stod(v)); } catch (...) {}
    return Parameter(name, v);
}
```

信号：与 `cmd_node` 一样 `SIGINT/SIGTERM` → `OnShutdown`；结束 `Clear()`。

- [ ] **Step 3: `main.cpp` 增加**

```cpp
#include "autolink/tools/cli/cmd_param.hpp"
// ...
autolink::tools::SetupParam(app);
```

- [ ] **Step 4: 编译 + help**

```bash
build/bin/autolink param -h
build/bin/autolink param list -h
```

Expected: 列出 list/get/set。

---

### Task 3: `service call`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_service.cpp`

- [ ] **Step 1: 在 `SetupService` 增加 `call` 子命令**

```cpp
auto* call = service->add_subcommand("call", "Call a service with JSON request");
auto svc = std::make_shared<std::string>();
auto json = std::make_shared<std::string>("{}");
auto type = std::make_shared<std::string>();
auto timeout = std::make_shared<double>(5.0);
call->add_option("service", *svc)->required();
call->add_option("json", *json, "Request JSON (default {})");
call->add_option("--type", *type, "Request protobuf type");
call->add_option("--timeout", *timeout, "Timeout seconds");
call->callback([=]() {
    InstallShutdownHandlers();
    autolink::Init("autolink");
    std::string req_type, err, desc;
    if (!type->empty()) {
        req_type = *type;
        ResolveServiceType(*svc, nullptr, &desc); // best-effort desc
        RegisterProtoType(req_type, desc);
    } else if (!ResolveServiceType(*svc, &req_type, &desc) || req_type.empty()) {
        throw CLI::ValidationError("call", "cannot resolve type; pass --type");
    } else {
        RegisterProtoType(req_type, desc);
    }
    std::string req_bytes;
    if (!JsonToProtobufBytes(req_type, *json, &req_bytes, &err)) {
        throw CLI::ValidationError("call", err);
    }
    auto node = CreateNode("autolink_cli_service_call");
    auto client = node->CreateClient<message::RawMessage, message::RawMessage>(*svc);
    // wait for service: client->ServiceIsReady() loop with timeout or sleep(1)
    auto req = std::make_shared<message::RawMessage>(req_bytes);
    auto resp = client->SendRequest(req, std::chrono::seconds(
        static_cast<int>(*timeout))); // check SendRequest signature
    if (!resp) throw CLI::RuntimeError(1);
    std::string out;
    // Prefer ProtobufBytesToJson with response type if known;
    // else try same type, else print raw size / hex-less Debug if parse works
    if (ProtobufBytesToJson(req_type, resp->message, &out, &err) ||
        ProtobufBytesToDebug(req_type, resp->message, &out, &err)) {
        std::cout << out << std::endl;
    } else {
        std::cout << "response bytes: " << resp->message.size() << std::endl;
    }
    Clear();
});
```

核对 `Client::SendRequest` 超时参数类型（`client.hpp`）：按实际签名改 `chrono`。

若响应 type ≠ 请求 type：P1 可先用 `--type` 只表示 **request** type；响应优先尝试 Debug/JSON 同 type，失败则打印长度。可在 help 注明。后续可加 `--response-type`（本轮不做除非联调阻塞）。

- [ ] **Step 2: 编译**；`autolink service call -h`。

---

### Task 4: `action send_goal` 可用化

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_action.cpp`

- [ ] **Step 1: 替换占位 `CmdSendGoal`**

```cpp
#include "autolink/proto/action.pb.h"
#include "autolink/tools/cli/proto_json.hpp"
#include <random>
// ...

void CmdSendGoal(const std::string& action_name, const std::string& json,
                 const std::string& type_flag) {
    std::string goal_type = type_flag;
    std::string desc, err;
    if (goal_type.empty()) {
        // Best-effort: resolve from service action+"/send_goal" message_type
        // (may be wrapper type — if unusable, require --type)
        if (!ResolveServiceType(action_name + "/send_goal", &goal_type, &desc) ||
            goal_type.empty() ||
            goal_type.find("SendGoal") != std::string::npos) {
            std::cerr << "goal --type required (could not infer goal message type)\n";
            throw CLI::RuntimeError(1);
        }
    }
    RegisterProtoType(goal_type, desc);
    std::string goal_bytes;
    const std::string payload = json.empty() ? "{}" : json;
    if (!JsonToProtobufBytes(goal_type, payload, &goal_bytes, &err)) {
        std::cerr << err << std::endl;
        throw CLI::RuntimeError(1);
    }
    proto::SendGoalRequest req;
    // 16 random bytes goal_id
    std::array<char, 16> uuid{};
    std::random_device rd;
    for (auto& b : uuid) b = static_cast<char>(rd());
    req.mutable_goal_id()->set_uuid(std::string(uuid.data(), uuid.size()));
    req.set_goal(goal_bytes);

    auto node = CreateNode("autolink_cli_action");
    auto client = node->CreateClient<proto::SendGoalRequest, proto::SendGoalResponse>(
        action_name + "/send_goal");
    auto resp = client->SendRequest(req, /*timeout*/ std::chrono::seconds(5));
    if (!resp) throw CLI::RuntimeError(1);
    std::cout << "accepted: " << (resp->accepted() ? "true" : "false") << "\n"
              << "stamp_sec: " << resp->stamp_sec() << "\n";
}
```

CLI11：`send_goal` 绑定 `action_name`、可选 `json`、`--type`。

- [ ] **Step 2: 编译**；`autolink action send_goal -h`。

---

### Task 5: `channel pub` + `echo --once/-n`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_channel.cpp`（`channel_cli` 命名空间内加 `CmdPub`；改 `CmdEcho` 签名；`SetupChannel` 绑定）

- [ ] **Step 1: 扩展 `CmdEcho`**

将

```cpp
void CmdEcho(const std::string& channel_name);
```

改为：

```cpp
void CmdEcho(const std::string& channel_name, int max_messages /* 0 = infinite */);
```

在回调里对计数：`std::atomic<int> count{0}`；每条消息 `++count`；若 `max_messages > 0 && count >= max_messages` 则 `autolink::AsyncShutdown()` 或设 flag 跳出 wait 循环。

现有循环大致是 `while (!IsShutdown()) sleep`——在 reader callback 里达到 N 后调用 `OnShutdown(0)` / 项目既有的 shutdown API。

- [ ] **Step 2: 实现 `CmdPub`**

```cpp
void CmdPub(const std::string& channel, const std::string& json,
            const std::string& type_flag, double rate, int times) {
    std::string type, desc, err;
    if (!type_flag.empty()) {
        type = type_flag;
        ResolveChannelType(channel, nullptr, &desc);
    } else if (!ResolveChannelType(channel, &type, &desc) || type.empty()) {
        std::cerr << "cannot resolve type; pass --type\n";
        throw CLI::RuntimeError(1);
    }
    RegisterProtoType(type, desc);
    std::string bytes;
    if (!JsonToProtobufBytes(type, json, &bytes, &err)) {
        std::cerr << err << std::endl;
        throw CLI::RuntimeError(1);
    }
    auto node = CreateNode("autolink_cli_pub");
    proto::RoleAttributes attr;
    attr.set_channel_name(channel);
    attr.set_message_type(type);
    if (!desc.empty()) attr.set_proto_desc(desc);
    auto writer = node->CreateWriter<message::RawMessage>(attr);
    if (!writer) throw CLI::RuntimeError(1);
    if (times < 1) times = 1;
    const auto period = std::chrono::duration<double>(rate > 0 ? 1.0 / rate : 1.0);
    for (int i = 0; i < times && !IsShutdown(); ++i) {
        writer->Write(std::make_shared<message::RawMessage>(bytes));
        if (i + 1 < times) std::this_thread::sleep_for(period);
    }
}
```

核对 `RoleAttributes` 是否有 `proto_desc` 字段（echo 路径用过则照抄）。

- [ ] **Step 3: CLI11 绑定**

```cpp
// echo
auto once = std::make_shared<bool>(false);
auto n = std::make_shared<int>(0);
echo->add_flag("--once", *once);
echo->add_option("-n,--times", *n, "Exit after N messages");
echo->callback([...]() {
    int max = *once ? 1 : *n;
    CmdEcho(*echo_name, max);
});

// pub
auto* pub = channel->add_subcommand("pub", "Publish JSON message");
// channel, json required; --type; --rate default 1.0; --times default 1
```

- [ ] **Step 4: 编译 + help**

```bash
autolink channel -h   # 应含 pub
autolink channel pub -h
autolink channel echo -h
```

---

### Task 6: 冒烟与文档

**Files:**
- Modify: `autolink/README.md`（tools 段补一行示例，可选）
- Modify: `docs/source/autolink_developer_tools.md` 若有 channel/service 章节则补命令（有则改，无则跳过）

- [ ] **Step 1: Help 冒烟**

```bash
for c in "param -h" "param list -h" "service call -h" "action send_goal -h" \
         "channel pub -h" "channel echo -h"; do
  build/bin/autolink $c || exit 1
done
```

- [ ] **Step 2: 有环境时联调（可选但推荐）**

```bash
# terminal A: examples talker / service / param_server
# terminal B:
autolink channel echo <ch> --once
autolink channel pub <ch> '{"..."}' --type <Type>
autolink service call <svc> '{}' --type <ReqType>
autolink param list <node>
```

- [ ] **Step 3: 更新 README tools 列表**，加 2～3 条示例命令。

---

## Spec coverage

| Spec | Task |
|------|------|
| proto_json | 1 |
| param list/get/set | 2 |
| service call | 3 |
| action send_goal | 4 |
| channel pub + echo once/n | 5 |
| 验证 / 文档 | 6 |
| 非目标 P2–P4 | 不实现 |

## 自检

- 无 TBD；`SendRequest` / `RoleAttributes::proto_desc` / shutdown API 以实现代码为准，实现时打开头文件核对签名。
- CMake 目标仍为 `autolink_cli` / `OUTPUT_NAME autolink`。
- 不自动 commit。
