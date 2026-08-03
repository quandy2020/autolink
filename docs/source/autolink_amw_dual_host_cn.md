# AMW 双机联调清单

前提：两边以相同 vendor 编译（FastDDS：`-DAUTOLINK_ENABLE_FASTDDS=ON`；或 Cyclone：`-DAUTOLINK_ENABLE_CYCLONEDDS=ON`），且在同一局域网、同一 Domain。

## 1. 环境

主机 A（如 `192.168.10.6`）：

```bash
cd autolink
source scripts/amw_dual_host_env.sh 192.168.10.6 0 amw_cyclonedds   # 或 amw_fastdds
```

主机 B（如 `192.168.10.7`）：

```bash
cd autolink
source scripts/amw_dual_host_env.sh 192.168.10.7 0 amw_cyclonedds
```

检查：

- [ ] `AUTOLINK_IP` 为**本机可达局域网 IP**（勿用 `127.0.0.1`）
- [ ] 两边 `AUTOLINK_DOMAIN_ID` / `ROS_DOMAIN_ID` 相同
- [ ] 两边 `AUTOLINK_AMW_IMPLEMENTATION` 相同（`amw_fastdds` 或 `amw_cyclonedds`）
- [ ] 对应 vendor 已在编译时 ENABLE；日志 `network_ready=1`
- [ ] 防火墙放行 DDS 组播/单播（UDP；具体端口随 Domain 变化）
- [ ] Cyclone：`LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` 含 `libddsc`（ROS Humble 下先 `source /opt/ros/humble/setup.bash`）

## 2. 运行（Pub/Sub）

主机 A：

```bash
./build/bin/examples/autolink_example_amw_talker
```

主机 B：

```bash
./build/bin/examples/autolink_example_amw_listener
```

期望：B 周期性打印 `amw_listener seq=...`。

## 2b. 运行（Service）

主机 A：

```bash
./build/bin/examples/autolink_example_amw_service
```

主机 B：

```bash
./build/bin/examples/autolink_example_amw_client
```

期望：B 等到 `amw/driver` ready（HasService + request reader），再周期性打印 `amw_client response msg_id=...`。

## 3. 失败排查

| 现象 | 排查 |
|---|---|
| `network middleware is stub` / `network_ready=0` | 未 ENABLE 或链错库；`ldd`/`otool` 查 `libddsc`/`libfastdds` |
| 完全无日志 | glog 默认写 CWD；可设 `GLOG_logtostderr=1` |
| 同机可通、跨机不通 | `AUTOLINK_IP`、网段、防火墙、组播 |
| Domain 不一致 | 两边 `echo $AUTOLINK_DOMAIN_ID` |
| 拓扑对不上 | 应见 `TopologyManager using AMW network discovery`；远程 Node 加入时有 `republishing local channel/service roles` |
| Service `not ready within 10s` | 两边 Domain/实现一致；看是否收到对端 Channel Join（request reader） |
| 请求超时无响应 | Hybrid 无 matched peer 时不发送；确认 discovery 已 Enable 对端 |
| FastDDS 链接失败（Docker Humble） | 系统多为 FastRTPS **2.6**，与本仓库 Fast DDS **3.x** API 不兼容 → 用 Cyclone |

## 4. 同机模拟 DIFF_HOST（单机验收 RTPS）

无需两台机器：给两个进程设置**不同** `AUTOLINK_IP`，Hybrid 判定为异机并走 DDS：

```bash
./scripts/amw_preflight.sh
./scripts/amw_sim_dual_host.sh amw_cyclonedds 0 10
./scripts/amw_sim_service.sh amw_cyclonedds 0 15
./scripts/amw_sim_action.sh amw_cyclonedds 0 20
./scripts/amw_sim_param.sh amw_cyclonedds 0 15
```

期望：

- Pub/Sub：`PASS: listener received chatter over simulated DIFF_HOST (RTPS)`
- Service：`PASS: client received service response over simulated DIFF_HOST (RTPS)`

脚本会优先使用 `build-macos`（若存在），否则 `build`；可用 `AUTOLINK_BUILD_DIR` 覆盖。

## 5. 传输路径说明

匹配到对端后，Writer 只向**有 peer 的 mode** 发送：

- 同进程 → INTRA  
- 同机异进程 → SHM  
- 异机 → DDS（`diff_host: RTPS`；FastDDS 或 Cyclone）

未匹配到 peer 时不发送（等待 discovery），避免空广播。`diff_host==RTPS` 时 RTPS 创建失败**不会**回退到 SHM。
