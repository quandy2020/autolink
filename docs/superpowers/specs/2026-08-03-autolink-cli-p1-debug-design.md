# Autolink CLI P1 调试闭环补齐设计

日期：2026-08-03  
状态：待审阅  
范围：`autolink/autolink/tools/cli`（param / service call / action send_goal / channel pub + echo 增强）  
前置：`docs/superpowers/specs/2026-08-03-autolink-cli11-design.md`（统一 CLI 已落地）

## 1. 背景与目标

统一 `autolink` CLI 后，相对 ROS2 仍缺调试闭环：`param`、`service call`、可用的 `action send_goal`、`channel pub`，以及 `echo` 有限条数退出。

**目标（P1）：** 补齐上述能力，payload 使用 JSON，类型优先从拓扑推断，失败可用 `--type`。

**非目标（留给后续阶段）：**

- 全局 `--wait`、统一退出码大整顿、bash/zsh 补全（P2/P4）
- `doctor`、`launch list`（P3）
- `param dump/load`、action cancel/get_result
- 强制模板化强类型 Client（不用动态 RawMessage 路径）

## 2. 已确认决策

| 项 | 选择 |
|---|---|
| 本轮范围 | 仅 P1（调试闭环） |
| Payload | **A**：JSON → protobuf Message |
| 类型来源 | **A**：拓扑推断，推不出再用 `--type` |
| 实现路径 | **方案 1**：共享 `cli/proto_json` + 扩展各 cmd |

## 3. 命令面

```text
autolink param list <node>
autolink param get  <node> <name>
autolink param set  <node> <name> <value>

autolink service call <service> [json] [--type T] [--timeout S]

autolink action send_goal <action> [json] [--type T]

autolink channel pub <channel> <json> [--type T] [--rate Hz] [--times N]
autolink channel echo <channel> [--once] [-n N]   # 保留既有行为，新增退出条件
```

### 3.1 param set 值解析

按顺序：

1. `true` / `false`（大小写不敏感）→ bool  
2. 可解析为 int64 且无小数点 → int  
3. 可解析为 double → double  
4. 否则整段作为 string（若以 `{` / `[` 开头，仍按 string 存，不做嵌套 proto）

### 3.2 类型解析（共用）

1. 若用户给了 `--type`，使用之，并用 `ProtobufFactory` 取/注册 descriptor（拓扑 desc 优先）。  
2. 否则从拓扑取 `message_type`（channel：writer；service：server RoleAttributes；action goal：由 action 相关服务/约定推断或要求 `--type`）。  
3. 仍空 → stderr 报错，非 0 退出。

## 4. 架构与文件

```text
autolink/tools/cli/
  proto_json.hpp / proto_json.cpp   # 新建
  cmd_param.hpp / cmd_param.cpp     # 新建 SetupParam
  cmd_service.cpp                   # + call
  cmd_action.cpp                    # 重写 send_goal
  cmd_channel.cpp                   # + pub；echo --once/-n
  main.cpp                          # SetupParam
CMakeLists.txt                      # 编入新源文件
```

### 4.1 `proto_json` 职责

- `JsonToProtobuf(type, json, out_bytes | Message*)`：`GenerateMessageByType` + `JsonStringToMessage` + `SerializeToString`  
- `ProtobufBytesToJson/Debug(type, bytes)`：用于打印 service 响应  
- `RegisterType(type, optional_desc)`：包装 `ProtobufFactory::RegisterMessage`  
- 可选：从 channel/service topology 取 type+desc 的薄封装

复用仓库已有 `google/protobuf/util/json_util.h`（见 `common/file.cpp`）。

### 4.2 传输路径

| 命令 | 路径 |
|------|------|
| `channel pub` | JSON→bytes → `Writer<RawMessage>`；RoleAttributes 带上 message_type（及可得的 proto_desc） |
| `service call` | JSON→request bytes → `Client<RawMessage, RawMessage>` → 响应 bytes→JSON/DebugString |
| `action send_goal` | JSON→goal bytes → 填入 `proto::SendGoalRequest`（随机/生成 goal_id）→ `Client<SendGoalRequest, SendGoalResponse>` 调 `<action>/send_goal` |
| `param *` | `CreateNode` + `ParameterClient(node, service_node_name)` |

### 4.3 echo 增强

在现有 `CmdEcho` 上增加：

- `--once`：收到 1 条后退出  
- `-n,--times N`：收到 N 条后退出  
- 未指定则行为与现网一致（直到 Ctrl-C / shutdown）

## 5. 错误处理

- CLI 解析失败：CLI11 非 0  
- 缺类型 / JSON 非法 / 工厂无法生成 Message / SendRequest 失败或超时：stderr + 非 0  
- `param` 节点无参数服务 / get/set 失败：stderr + 非 0  
- 仅 `-h`：不 `Init` 运行时

## 6. 验证

- `autolink param -h` / `service call -h` / `channel pub -h` / `action send_goal -h`  
- 有运行图时：对已知 channel/service/param 节点做一次 smoke（可与 examples talker/service/param 联调）  
- `echo --once` 在有 publisher 时打印一条后退出

## 7. 风险与缓解

| 风险 | 缓解 |
|------|------|
| Service 拓扑未带齐 request/response type | `--type` 兜底；文档说明 |
| `Client<RawMessage,RawMessage>` 与 server 强类型不匹配 | 传输层按字节兼容（与 Cyber/Autolink Raw 路径一致）；联调 examples 验证 |
| Action goal 类型难从拓扑推断 | send_goal 允许/常需 `--type`；help 写清 |
| JSON 字段名与 proto 不一致 | 使用 protobuf JSON mapping；失败给出 JsonStringToMessage 错误信息 |
