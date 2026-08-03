# AMW（Autonomy Middleware）

在 Node / Writer / Reader / Service / Client / Action **稳定 API 之下**的可插拔中间件，用法对齐 ROS 2 RMW。跨机实现：**Fast DDS**、**Cyclone DDS**。

日常业务代码不直接调 AMW；通过环境变量选实现与 Domain。同机默认可走 SHM；跨机或「同机 DIFF_HOST」走选定 DDS。

## 环境变量

| 变量 | 作用 | ROS 2 对照 |
|---|---|---|
| `AUTOLINK_AMW_IMPLEMENTATION` | 选实现：`amw_cyclonedds` / `amw_fastdds` | `RMW_IMPLEMENTATION`（可同设） |
| `AUTOLINK_DOMAIN_ID` | DDS Domain（整数） | `ROS_DOMAIN_ID` |
| `AUTOLINK_IP` | 本机通告 IP（跨机必须为可达局域网 IP，勿用 `127.0.0.1`） | `ROS_IP` |
| `AUTOLINK_AMW_PLUGIN_PATH` | 外部 `libamw_*.so` 目录 | 插件路径 |
| `AUTOLINK_PATH` | 配置根（含 `conf/autolink.pb.conf`） | — |
| `LD_LIBRARY_PATH` | 需含 `build/lib`；Cyclone+Humble 时优先 Humble 的 `lib/x86_64-linux-gnu` | — |

启动日志应含 **`network_ready=1`**（或 `doctor` 里 `network_middleware_ready`）。为 `0` / stub 时跨机 RTPS 不可用。

一键双机环境：

```bash
# 每台机器各执行一次（换本机局域网 IP）
source scripts/amw_dual_host_env.sh <lan_ip> [domain_id=0] [amw_cyclonedds|amw_fastdds]
# 会设置 AUTOLINK_IP / DOMAIN_ID / IMPLEMENTATION（及 RMW 别名）
```

## 构建

```bash
cmake -B build \
  -DAUTOLINK_ENABLE_FASTDDS=ON \
  -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
  -DAUTOLINK_BUILD_EXAMPLES=ON \
  -DCMAKE_PREFIX_PATH="/opt/ros/humble/lib/x86_64-linux-gnu;/opt/ros/humble;/usr/local"
cmake --build build -j
```

| 实现 | 标识 | 说明 |
|---|---|---|
| Fast DDS | `amw_fastdds` | 需 Fast DDS **3.x**（Humble 自带常为 2.x，易链接失败） |
| Cyclone DDS | `amw_cyclonedds` | Ubuntu/Docker：ROS Humble 或 `cyclonedds-dev`；macOS：`./scripts/install_cyclonedds.sh` |
| OpenDDS / Connext | `amw_opendds` 等 | 内置 stub；可外挂 ABI v1 插件 |

Cyclone（Humble）运行时：

```bash
source /opt/ros/humble/setup.bash
export LD_LIBRARY_PATH=/opt/ros/humble/lib/x86_64-linux-gnu:$PWD/build/lib:$LD_LIBRARY_PATH
export AUTOLINK_AMW_IMPLEMENTATION=amw_cyclonedds
```

构建后检查：`./scripts/amw_preflight.sh build`、`autolink doctor`。

## 分层与 HYBRID

```text
Node / Writer / Reader / Service / Client / Action
        ↓ HYBRID
 同进程 INTRA | 同机异进程 SHM | 异机（或 DIFF_HOST）选定 DDS
        ↓
 Topology：network ready → DDS `/autolink/topology`；否则仅 Local
```

匹配到 peer 后只向**有接收端的 mode** 发送；未匹配不发送。`diff_host` 走 RTPS，失败不回退 SHM。

QoS：reliability / history+depth / durability 映射到 DDS；拓扑通道为 RELIABLE + TRANSIENT_LOCAL + KEEP_ALL。

## 双机联调

前提：两边同一 vendor、同一局域网、同一 Domain、防火墙放行 DDS（组播/发现端口）。

```bash
# 主机 A（示例 IP）
source scripts/amw_dual_host_env.sh 192.168.10.6 0 amw_cyclonedds
export AUTOLINK_PATH=/path/to/repo/autolink   # 内含 conf/autolink.pb.conf
export LD_LIBRARY_PATH=...   # 见上

# 主机 B
source scripts/amw_dual_host_env.sh 192.168.10.7 0 amw_cyclonedds
# 同样设置 AUTOLINK_PATH / LD_LIBRARY_PATH
```

| 场景 | 主机 A | 主机 B |
|---|---|---|
| Pub/Sub | `autolink_example_amw_talker` | `autolink_example_amw_listener` |
| Service | `autolink_example_amw_service` | `autolink_example_amw_client` |
| Action | `autolink_example_amw_action_server` | `autolink_example_amw_action_client` |
| Parameter | `autolink_example_amw_param_server` | `autolink_example_amw_param_client` |

二进制在 `build/bin/examples/`。两边通道名 / service / action 名须与示例一致。

检查清单：

1. 两边 `autolink doctor` → `network_middleware_ready` 为 yes  
2. `AUTOLINK_IP` 为对方可达的网卡地址  
3. `AUTOLINK_DOMAIN_ID` 与 `AUTOLINK_AMW_IMPLEMENTATION` 完全一致  
4. `autolink --wait 3 channel list` / `node list` 能看到对端  

| 现象 | 排查 |
|---|---|
| stub / `network_ready=0` | 未 `ENABLE_*`、链错库、或未设 IMPLEMENTATION |
| 同机可通、跨机不通 | IP、网段、防火墙、组播 |
| Humble 上 FastDDS 链接失败 | 改用 Cyclone + FastDDS 3.x 自建 |
| Service / Action 一直 not ready | Domain/实现不一致；discovery 等待不足 |

## 同机模拟 DIFF_HOST

两进程设不同 `AUTOLINK_IP`，强制走 DDS（无需两台物理机）：

```bash
./scripts/amw_preflight.sh build
./scripts/amw_sim_dual_host.sh amw_cyclonedds 0 10    # Pub/Sub，末参为秒
./scripts/amw_sim_service.sh   amw_cyclonedds 0 15
./scripts/amw_sim_action.sh    amw_cyclonedds 0 20
./scripts/amw_sim_param.sh     amw_cyclonedds 0 15
```

用法：`./scripts/amw_sim_*.sh <implementation> <domain_id> <timeout_sec>`。  
可用 `AUTOLINK_BUILD_DIR` 指定构建目录（默认 `build`）。

## 插件 ABI（v1）

外部实现导出：

- `amw_get_abi_version`
- `amw_get_implementation_identifier`
- `amw_create_transport_provider`
- `amw_create_discovery_provider`
- `amw_destroy_provider`

库名：`libamw_<vendor>.so`，放入 `AUTOLINK_AMW_PLUGIN_PATH`。参考：`examples/amw_plugin_stub`。

## 相关

- [快速开始](../guide/quickstart.md) · [CLI doctor](../tools/cli.md#doctor) · [FAQ](../faq.md)
