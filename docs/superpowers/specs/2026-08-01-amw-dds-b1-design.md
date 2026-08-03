# AMW DDS 完善设计（方案 B1）

- **日期**：2026-08-01
- **状态**：已完成（B1 范围内；OpenDDS/Connext 内置与 ROS 2 图互操作仍不做）
- **范围**：FastDDS 生产级完善 + CycloneDDS 内置真实现
- **不做**：OpenDDS / Connext 内置、ROS 2 图互操作、Discovery Server、多 Participant

## 1. 目标与验收

### 1.1 目标

在保持 Node / Writer / Reader / Service / Client / Action API 不变的前提下：

1. 将 **FastDDS** 从「最小可用 MVP」提升为可运维的跨机中间件；
2. 内置 **CycloneDDS** 真实现，与 FastDDS 共用同一套 AMW 接口与拓扑协议；
3. 加固插件 ABI，使 OpenDDS / Connext 仍可通过外挂 `libamw_*.so` 扩展。

### 1.2 验收标准

| 场景 | 条件 | 期望 |
|---|---|---|
| FastDDS loopback | `-DAUTOLINK_ENABLE_FASTDDS=ON` | `amw_test` 含 FastDDS 用例 PASS |
| Cyclone loopback | `-DAUTOLINK_ENABLE_CYCLONEDDS=ON` + SDK | `amw_test` 含 Cyclone 用例 PASS |
| 双机 Pub/Sub | `AUTOLINK_AMW_IMPLEMENTATION=amw_fastdds` 或 `amw_cyclonedds` | talker/listener 互通 |
| 双机 Service | 同上 | `amw_service` / `amw_client` 互通 |
| 默认构建 | 两开关均为 OFF | 构建通过；DDS vendor 为 stub |
| Hybrid 跨机失败 | RTPS 创建失败且 relation=DIFF_HOST | **不**静默降级 SHM；打 ERROR 日志 |

## 2. 架构

```
Node API
  → HYBRID (INTRA / SHM / RTPS)
      → Amw Router
          → LocalProvider          (INTRA/SHM)
          → ITransportProvider     (diff_host / RTPS)
                ├── FastDdsTransportProvider     [内置, AUTOLINK_ENABLE_FASTDDS]
                ├── CycloneDdsTransportProvider  [内置, AUTOLINK_ENABLE_CYCLONEDDS]
                └── DdsStubTransportProvider     [默认 / 未编译 vendor]
          → IDiscoveryProvider
                └── 专用通道 "/autolink/topology" (ChangeMsg protobuf over RawMessage)
```

两家 DDS 各自实现同一接口，**不**抽取统一 `DdsBackend`（避免大重构）。共享约定：

- 载荷：`message::RawMessage`（protobuf 序列化字节）
- 拓扑：固定 channel `/autolink/topology`
- 选型：`AUTOLINK_AMW_IMPLEMENTATION` / `RMW_IMPLEMENTATION` / `AmwConf.implementation`
- 环境：`AUTOLINK_DOMAIN_ID` / `ROS_DOMAIN_ID`、`AUTOLINK_IP`

## 3. 阶段 1 — FastDDS 生产级

### 3.1 Participant / 网络

**文件**：`amw/dds/fastdds_participant.{hpp,cpp}`

| 输入 | 行为 |
|---|---|
| `transport_conf.participant_attr.lease_duration` | 映射 participant discovery lease（秒；0 表示保持默认） |
| `announcement_period` | 映射 announcement period |
| `port_base` / `domain_id_gain` | 若 FastDDS API 可设则应用；否则文档说明忽略并打 DEBUG |
| `AUTOLINK_IP` / `GlobalData::HostIp()` | 配置默认 unicast locator；优先本机通告 IP |
| 多网卡 | 至少保证 HostIp 对应网卡可达；完整接口绑定可作为后续增强 |

实现注意：保持单 Participant + 共享 Publisher/Subscriber 模型；不在本阶段引入多 Participant。

### 3.2 拓扑发现 QoS

**文件**：`amw/dds/fastdds_provider.cpp`（`FastDdsDiscoveryProvider::Start`）

- Topic/Writer/Reader：`RELIABLE` + `TRANSIENT_LOCAL` + `KEEP_LAST`（depth ≥ 100，可与现有一致或略增）
- 目的：晚加入节点能收到近期 `ChangeMsg`，缓解「后起进程看不到已有角色」

### 3.3 Endpoint QoS 映射

**文件**：`amw/dds/fastdds_provider.cpp`（`ApplyWriterQos` / `ApplyReaderQos`）

已有：reliability / history+depth / durability。

补齐：

| Autolink | FastDDS |
|---|---|
| `*_SYSTEM_DEFAULT` | 使用 `QosProfileConf` 默认 profile 对应值，而非错误落到 BEST_EFFORT/VOLATILE |
| `mps > 0` | 映射到合理 resource / throughput 限制（无直接字段时：限制 history depth 或文档化近似） |
| `resource_limit.max_history_depth` | Writer/Reader history depth 上限 |
| 超大消息 | 序列化前检查相对 `fastdds_raw_type` 上限；失败返回 false + ERROR |

### 3.4 Hybrid 跨机失败策略

**文件**：`transport/transmitter/hybrid_transmitter.hpp`、`transport/receiver/hybrid_receiver.hpp`

- `diff_host` 映射为 `RTPS` 且 `Amw::Create*` 返回空：**禁止**创建 `ShmTransmitter/Receiver` 作为替身
- 同机 relation（`same_proc` / `diff_proc`）若配置为 RTPS 且失败：可保留 SHM fallback（仅同机有意义），并打 WARN
- 日志必须区分「跨机不可用」与「同机 fallback」

### 3.5 Enable / Disable

**文件**：`amw/dds/fastdds_provider.cpp`

- `IPublisher::Enable/Disable`、`ISubscription::Enable/Disable` 映射到 DataWriter/DataReader 的 enable/disable（或等价生命周期控制）
- 未 Enable 时 `Publish` 返回 false

### 3.6 插件 ABI 加固

**文件**：`amw/plugin_abi.h`、`amw/plugin_loader.cpp`

1. 新增 `amw_get_abi_version()`，当前版本常量 `AUTOLINK_AMW_PLUGIN_ABI_VERSION = 1`
2. 加载时校验 version；不匹配则拒绝并 ERROR
3. 校验 `amw_get_implementation_identifier()` 与请求的 implementation 一致（允许 ROS 别名映射后一致）
4. 真正遍历 `AUTOLINK_AMW_PLUGIN_PATH` 与 `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` 分号或冒号分隔目录（修掉当前 `(void)search_dirs`）
5. 可选：仓库内 `examples/amw_plugin_stub/` 最小参考插件（可后置到阶段 2 末）

## 4. 阶段 2 — CycloneDDS 内置

### 4.1 构建

**文件**：根 `CMakeLists.txt`、`autolink/CMakeLists.txt`

```
option(AUTOLINK_ENABLE_CYCLONEDDS "Link Cyclone DDS for multi-host AMW" OFF)
```

- `ON` 时：`find_package(CycloneDDS REQUIRED)`（或 `CycloneDDS` C API 包名以实际 SDK 为准）
- 链接 `CycloneDDS::ddsc`（名称以 find 结果为准）
- `target_compile_definitions(... AUTOLINK_ENABLE_CYCLONEDDS=1)`
- 未安装 SDK 时：配置失败并给出安装提示（与 FastDDS 一致，不静默忽略 REQUIRED）

依赖获取：**不**强制 submodule；文档说明 apt / 源码安装路径。CI 可选 job 在有 SDK 镜像时开启。

### 4.2 源码布局

```
autolink/amw/dds/
  cyclonedds_participant.{hpp,cpp}
  cyclonedds_raw_type.{hpp,cpp}      # 或直接用 DDS 原生 octets / 自定义 topic
  cyclonedds_provider.{hpp,cpp}      # Transport + Discovery + factory
```

工厂：

- `CreateCycloneDdsTransportProvider()` / `CreateCycloneDdsDiscoveryProvider()`
- `#ifndef AUTOLINK_ENABLE_CYCLONEDDS` 时返回 `DdsStubTransportProvider(kCycloneDds)`

注册：`Amw::RegisterDefaults` 中 Cyclone 从 stub 改为上述 factory（与 FastDDS 对称）。

### 4.3 行为对齐

| 能力 | 要求 |
|---|---|
| Pub/Sub RawMessage | 与 FastDDS 相同 channel 名 / QoS 语义（可靠/历史/持久性） |
| Discovery | 发布/订阅 `/autolink/topology`，`ChangeMsg` 序列化不变 |
| Domain / IP | 同 env 变量 |
| QoS SYSTEM_DEFAULT | 同 FastDDS 映射策略 |
| Topology TRANSIENT_LOCAL | Cyclone 侧等价 durability |
| 单测 | `AmwTransportTest.CycloneDdsLoopback`（`#ifdef AUTOLINK_ENABLE_CYCLONEDDS`） |

实现策略：优先 Cyclone **C API**（`ddsc`），减少 C++ 绑定版本差异；若环境仅有 ROS 自带包，以能 `find_package` 为准。

### 4.4 文档与示例

- 更新 `docs/source/autolink_amw_cn.md`：Cyclone 状态改为「真实（需 ENABLE）」
- 更新 `docs/source/autolink_amw_dual_host_cn.md`：增加 `amw_cyclonedds` 环境示例
- `scripts/amw_dual_host_env.sh`：已支持 cyclonedds 别名则核对；不足则补齐

## 5. 测试计划

1. **单元**：扩展 `amw_test.cpp`（FastDDS / Cyclone loopback、插件 ABI version、Hybrid 不降级逻辑可用 transport 级或 hybrid 单测）
2. **回归**：`node_test`（Service 同进程）、`transport_test`、`topology_manager_test`
3. **手工双机**：文档 checklist（fastdds 与 cyclonedds 各跑一轮 talker/listener + service/client）

## 6. 风险与缓解

| 风险 | 缓解 |
|---|---|
| 本机无 Cyclone SDK | CMake 明确 REQUIRED + 文档安装步骤；默认 OFF |
| FastDDS / Cyclone QoS 语义不完全一致 | 文档列出已映射字段；未映射字段 DEBUG 忽略 |
| `AUTOLINK_IP` 与系统路由冲突 | 仅设置通告 locator；保留默认 discovery 作为 fallback |
| 拓扑 TRANSIENT_LOCAL 历史膨胀 | KEEP_LAST depth 上限；后续可加序列号去重（非本轮必须） |

## 7. 实现顺序（建议）

1. Hybrid 跨机禁止 SHM 降级（S，防误用）
2. FastDDS：拓扑 TRANSIENT_LOCAL + SYSTEM_DEFAULT QoS（S）
3. FastDDS：Participant conf + AUTOLINK_IP 网络（M）
4. FastDDS：Enable/Disable + 消息大小校验（S）
5. 插件 ABI v1 + 路径搜索（M）
6. Cyclone：CMake + participant/provider/discovery（L）
7. Cyclone：单测 + 文档（M）

## 8. 自检清单

- [x] 无「四家全内置」膨胀范围
- [x] 与现有 AMW 接口 / 拓扑协议一致
- [x] 默认 OFF 不破坏现有构建
- [x] 验收标准可测
- [x] OpenDDS/Connext 仍走插件，不在本设计实现
