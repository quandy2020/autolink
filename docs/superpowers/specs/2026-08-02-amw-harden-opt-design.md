# AMW 加固与优化设计（B1 后续）

- **日期**：2026-08-02
- **状态**：已完成
- **前置**：`2026-08-01-amw-dds-b1-design.md`（已完成）
- **范围**：正确性加固 + 性能/稳健性（方案 C，两阶段落地）
- **不做**：OpenDDS / Connext 内置真实现、ROS 2 图互操作、Discovery Server、多 Participant、抽取统一 `DdsBackend`

## 1. 目标与验收

### 1.1 目标

在 Node / Writer / Reader / Service / Client / Action **对外 API 不变**的前提下：

1. 消除假就绪与 Disable 后门控失效；
2. 对齐 FastDDS / Cyclone 的 MessageInfo 与 Enable 语义；
3. 降低 Cyclone 订阅空转与 discovery 回调持锁风险；
4. 改善 `WaitForService` 等待效率；
5. 补齐回归单测与小范围文档/脚本一致性。

### 1.2 验收标准

| 场景 | 期望 |
|---|---|
| `amw_test` / `node_test` | 全部 PASS |
| 同机 sim Pub/Sub + Service（Cyclone） | `amw_sim_*.sh` PASS |
| Disable 后 | Publisher 不发送；Subscription 不向用户回调 |
| 无有效 RTPS endpoint | `NetworkTransmitter/Receiver` 不报告 enabled |
| PreferReady | 默认 stub → 自动切真实 builtin；env pin 不覆盖 |
| 插件 ABI | 可加载 `libamw_opendds`；ABI 不匹配拒绝 |
| framed MessageInfo | RTPS 往返后 `spare_id` / seq 仍在 |

## 2. 两阶段落地

| 阶段 | 内容 | 验收门槛 |
|---|---|---|
| **A 正确性** | 假就绪、FastDDS Enable 门控、MessageInfo 对称、RegisterBuiltin 清理、单测、文档小修 | `amw_test` + sim |
| **B 性能/稳健** | Cyclone WaitSet、discovery 解锁回调、`WaitForService` 事件唤醒 | 同上 + 无明显空转回归 |

阶段 A 合入逻辑完成后再做 B；本仓库暂不强制 git commit（由用户决定）。

## 3. 阶段 A — 正确性

### 3.1 假就绪（`network_adapter.hpp`）

| 组件 | 现状 | 目标 |
|---|---|---|
| `NetworkTransmitter` 构造 | `enabled_=true` | 仅当 `publisher_ != nullptr` 时为 true；`Enable()` 在无 publisher 时保持 false |
| `NetworkReceiver::Enable` | `subscription_==nullptr` 仍可 `enabled_=true` | 无 subscription 时 Enable 无效，保持 false |

### 3.2 Stub discovery

**文件**：`dds_stub_provider.cpp`

- `DdsStubDiscoveryProvider::Start`：保持可调用，但 **`IsStub()==true`** 已足以让 `AmwDiscoveryBackend` 拒绝；若当前 `Start` 返回 true 导致误用，改为返回 **false** 并打 WARN（与「stub 不可用于网络拓扑」一致）。
- 单测：stub Start 失败或 `IsStub` 路径覆盖。

### 3.3 FastDDS Enable / 收发门控

**文件**：`fastdds_provider.cpp`

- `FastDdsPublisher::Publish`：已检查 `enabled_`（保持）。
- `FastDdsReaderListener`：**必须**在回调前检查所属 subscription 的 `enabled_`（今日缺失）。
- `Enable`/`Disable`：优先调用 FastDDS `DataWriter`/`DataReader` 的 `enable()`/`disable()`（若头文件 API 可用）；否则仅原子门控（与今日一致，但 listener 必须门控）。
- 行为对齐 Cyclone：Disable 期间不向 AMW 上层投递。

### 3.4 MessageInfo 对称

**文件**：`fastdds_provider.cpp`、`cyclonedds_provider.cpp`

- 主路径仍由 `network_adapter` 的 framed `[MessageInfo][payload]` 负责完整字段（含 `spare_id`）。
- Listener 填充的「旁路」`MessageInfo` 统一为：
  - `seq_num` ← sample 时间戳/seq 字段；
  - `msg_seq_num` ← 同值（int32 截断策略两边一致）；
  - 不伪造不可逆的 Identity（`sender_id` 仅保留在 sample 内，framed 优先）。
- 单测：framed 往返（Cyclone 或当前可用 vendor）。

### 3.5 `RegisterBuiltinDdsProviders`

**文件**：`dds_stub_provider.cpp`、`amw_test.cpp`

- Helper **仅**为 OpenDDS / Connext（及未编译的 vendor）注册 stub。
- FastDDS / Cyclone：若已由 factory 注册则**不覆盖**；若未注册则调用 `Create*Provider()`（真实或 stub 由编译开关决定）。
- 更新 `ProviderRegistryTest.RegisterAndGet` 期望与注释。

### 3.6 单测（`amw_test.cpp`）

| 用例 | 要点 |
|---|---|
| PreferReady | Shutdown → 清 env → Init；仅 Cyclone ON 时 implementation 变为 `amw_cyclonedds`；再设 env=`amw_fastdds` 时保持 pin（即便 stub） |
| Framed MessageInfo | NetworkTransmitter/Receiver 或 Transport RTPS：设置 `spare_id`，对端收到一致 |
| Plugin ABI | `AUTOLINK_AMW_PLUGIN_PATH=build/lib` + `TryLoadExternalPlugin("amw_opendds")` 成功且 `IsStub`；篡改 ABI 版本用例可用本地假符号或文档化手工项（优先能自动化的加载成功路径） |
| Enable 门控 | Publisher Disable 后 Publish 返回 false；Subscription Disable 后不增加回调计数 |

### 3.7 文档 / 脚本小修

- `autolink_amw_cn.md`：示例路径与 `${AUTOLINK_BUILD_DIR}/bin/examples` 对齐。
- `amw_preflight.sh`：service/client 缺失时硬失败（与 talker/listener 一致），或明确文档「可选」。
- `topology_manager.hpp`：去掉过时 “fast-rtps Participant” 表述，改为 AMW network discovery。

## 4. 阶段 B — 性能与稳健性

### 4.1 Cyclone WaitSet

**文件**：`cyclonedds_provider.cpp`（`CycloneDdsSubscription`）

- 用 `dds_create_waitset` + `dds_waitset_attach`（reader）替代固定 `sleep(5ms)` 忙等。
- 超时唤醒用于检查 `running_` / `enabled_`（例如 50–100ms wait 超时），避免无法退出。
- 每 subscription 仍可一线程；目标是 **有数据时低延迟、无数据时不空转**。
- Disable 时：不 take / 不回调（与今日一致）。

### 4.2 Discovery 回调解锁

**文件**：`fastdds_provider.cpp`、`cyclonedds_provider.cpp`（`OnTopologyRaw`）

```
lock → 解析 ChangeMsg → 拷贝匹配的 callback 列表 → unlock → 逐个调用
```

避免 `RepublishLocalRoles` → `PublishChange` → 再入 discovery 时死锁。

### 4.3 `WaitForService` 事件唤醒

**文件**：`client_base.hpp`（及必要时 `TopologyManager` / channel·service manager 通知点）

- 保留 `HasService && HasReader` 判定条件不变。
- 实现：在等待循环中注册短期 topology 监听（CHANNEL + SERVICE 的 JOIN/LEAVE），用 `condition_variable` 唤醒；超时仍用 deadline。
- 若改动 TopologyManager 过大：允许 **最小实现**——将 sleep 从 5ms 改为可配置/退避（5ms→20ms），并预留 listener 钩子；但本设计的目标态是 **事件唤醒**。
- 推荐落地：ClientBase 内 `std::condition_variable` + TopologyManager 已有的 change 回调 API（若有）；没有则在 `ChannelManager`/`ServiceManager` 增加轻量 `AddChangeListener`（进程内，不进 DDS）。

## 5. 风险与缓解

| 风险 | 缓解 |
|---|---|
| WaitSet API 在 ROS Humble Cyclone 0.10 差异 | 以容器内头文件为准；失败则保留超时 wait 循环 |
| WaitForService 监听泄漏 | 作用域守卫：离开 Wait 必 Unsubscribe |
| PreferReady 单测污染全局 Amw 单例 | 每测 Shutdown + 恢复 env |
| stub Start 改返回 false 破坏旧测试 | 同步改 `DdsStubProviderTest` |

## 6. 实现顺序

1. A3.1–A3.2 假就绪 + stub Start  
2. A3.3–A3.4 FastDDS 门控 + MessageInfo  
3. A3.5 RegisterBuiltin  
4. A3.6 单测  
5. A3.7 文档脚本  
6. B4.2 discovery 解锁（低风险，可先于 WaitSet）  
7. B4.1 Cyclone WaitSet  
8. B4.3 WaitForService 事件唤醒  
9. 全量 Docker 回归（`amw_test` / `node_test` / 两 sim）

## 7. 自检清单

- [x] 无扩大到 OpenDDS/Connext 内置 / ROS2 互操作
- [x] API 对外不变
- [x] 两阶段可独立验收
- [x] 验收可测、文件路径明确
- [x] 无 TBD 占位实现细节（WaitForService 允许 Topology 轻量 listener 作为明确备选）
