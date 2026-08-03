# Autolink AMW（Autonomy Middleware）

AMW 在 Node / Writer / Reader / Service / Client / Action **稳定 API 之下**提供可插拔中间件，用法对齐 ROS 2 RMW。跨机真实实现：**eProsima Fast DDS**、**Eclipse Cyclone DDS**。

## 与 ROS 2 一致的用法

```bash
# FastDDS 和/或 Cyclone（Docker/ROS Humble 可用 Cyclone 0.10.x）
cmake -B build \
  -DAUTOLINK_ENABLE_FASTDDS=ON \
  -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
  -DCMAKE_PREFIX_PATH="/opt/ros/humble/lib/x86_64-linux-gnu;/opt/ros/humble;/usr/local"
cmake --build build -j

export AUTOLINK_AMW_IMPLEMENTATION=amw_cyclonedds  # 或 amw_fastdds / RMW_IMPLEMENTATION=...
export AUTOLINK_DOMAIN_ID=0                        # 或 ROS_DOMAIN_ID
export AUTOLINK_IP=<本机局域网IP>                  # 类似 ROS_IP

# 可执行文件在构建目录下（默认 build/）
./build/bin/examples/autolink_example_amw_talker    # 主机 A
./build/bin/examples/autolink_example_amw_listener  # 主机 B
```

双机逐步清单见 [AMW 双机联调](autolink_amw_dual_host_cn.md)（含 `scripts/amw_dual_host_env.sh`、同机模拟脚本）。

| 变量 | 作用 | ROS 2 对应 |
|---|---|---|
| `AUTOLINK_AMW_IMPLEMENTATION` / `RMW_IMPLEMENTATION` | 选实现 | `RMW_IMPLEMENTATION` |
| `AUTOLINK_DOMAIN_ID` / `ROS_DOMAIN_ID` | DDS Domain | `ROS_DOMAIN_ID` |
| `AUTOLINK_IP` | 本机通告 IP | `ROS_IP` |
| `AUTOLINK_AMW_PLUGIN_PATH` | 外部 `libamw_*.so` 搜索目录 | 插件搜索路径 |

启动后日志应出现：`AMW initialized: ... network_ready=1` 与 `TopologyManager using AMW network discovery`。若 `network_ready=0` / `STUB`，跨机 RTPS **不可用**。未用环境变量钉死实现、且当前选择为 stub 时，会自动选用已编译的真实实现（先 FastDDS，再 Cyclone）。

## 已完全接入 HYBRID 的 API

| API | 默认传输 |
|---|---|
| Writer / Reader | HYBRID（同进程 INTRA / 同机 SHM / 跨机 DDS） |
| Service / Client | HYBRID；req/res 加入 ChannelManager；`WaitForService` 需 Service + request reader |
| Action | Service RPC + Writer/Reader feedback/status → 均 HYBRID |

## 实现与插件

| 实现 | 标识 | 状态 |
|---|---|---|
| Fast DDS | `amw_fastdds` / `rmw_fastrtps_cpp` | 真实（`-DAUTOLINK_ENABLE_FASTDDS=ON`，需 Fast DDS **3.x**） |
| Cyclone DDS | `amw_cyclonedds` / `rmw_cyclonedds_cpp` | 真实（`-DAUTOLINK_ENABLE_CYCLONEDDS=ON`） |
| OpenDDS | `amw_opendds` | 内置 stub；可外挂插件（ABI v1） |
| Connext | `amw_connextdds` / `rmw_connextdds` | 内置 stub；可外挂插件（ABI v1） |

编译示例：

```bash
cmake -B build \
  -DAUTOLINK_ENABLE_FASTDDS=ON \
  -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
  -DCMAKE_PREFIX_PATH="/usr/local;/opt/homebrew;/opt/ros/humble/lib/x86_64-linux-gnu;/opt/ros/humble"
```

### Cyclone 安装

- **macOS**（Homebrew 无 formula）：`./scripts/install_cyclonedds.sh` → `~/.local/cyclonedds`
- **Ubuntu / Docker**：`apt install cyclonedds-dev`，或使用 ROS Humble 自带的 Cyclone **0.10.5**（推荐，与本仓库开发版本一致）：

```bash
source /opt/ros/humble/setup.bash
export CMAKE_PREFIX_PATH="/opt/ros/humble/lib/x86_64-linux-gnu;/opt/ros/humble:$CMAKE_PREFIX_PATH"
export LD_LIBRARY_PATH="/opt/ros/humble/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
```

macOS 运行时：

```bash
export DYLD_LIBRARY_PATH="$HOME/.local/cyclonedds/lib:$DYLD_LIBRARY_PATH"
cmake -B build-macos -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
  -DCMAKE_PREFIX_PATH="$HOME/.local/cyclonedds;/usr/local;/opt/homebrew"
```

### 外部插件 ABI（对齐 rmw 包形态）

共享库导出（见 `amw/plugin_abi.h`，**ABI version = 1**）：

- `amw_get_abi_version`
- `amw_get_implementation_identifier`
- `amw_create_transport_provider`
- `amw_create_discovery_provider`
- `amw_destroy_provider`

库名：`libamw_<vendor>.so` / `.dylib`。设置 `AUTOLINK_AMW_IMPLEMENTATION` 与 `AUTOLINK_AMW_PLUGIN_PATH` 后，优先 `dlopen` 外部实现。参考插件：`examples/amw_plugin_stub`（产出 `libamw_opendds.so`，stub 行为）。

## 分层

```
Node / Writer / Reader / Service / Client / Action
        ↓ HYBRID
 same_proc → INTRA | diff_proc → SHM | diff_host → 选定 DDS
        ↓
 Topology：network ready → DDS `/autolink/topology`；否则 Local backend
 （远程 Node Join 时会 Republish 本地 Channel/Service 角色）
```

## QoS 映射（DDS）

| Autolink | Fast DDS / Cyclone |
|---|---|
| `*_SYSTEM_DEFAULT` | 映射为 `QosProfileConf` 默认 profile |
| reliability | RELIABLE / BEST_EFFORT |
| history + depth | KEEP_LAST / KEEP_ALL（受 `mps`、`resource_limit.max_history_depth` 上限） |
| durability | TRANSIENT_LOCAL / VOLATILE |
| topology 通道 | RELIABLE + TRANSIENT_LOCAL + KEEP_ALL（递增 seq，防历史折叠） |

## 同机模拟验收

```bash
./scripts/amw_preflight.sh build
./scripts/amw_sim_dual_host.sh amw_cyclonedds 0 10   # Pub/Sub DIFF_HOST
./scripts/amw_sim_service.sh amw_cyclonedds 0 15     # Service/Client DIFF_HOST
./scripts/amw_sim_action.sh amw_cyclonedds 0 20      # Action Goal/Feedback/Result DIFF_HOST
./scripts/amw_sim_param.sh amw_cyclonedds 0 15       # Parameter Set/Get DIFF_HOST
```

## 路线图

1. FastDDS 生产级 + CycloneDDS 内置 + 插件 ABI v1（本阶段）
2. OpenDDS / Connext 官方插件包（或内置）
3. 更完整的动态发现与 ROS 2 图互操作
