# AMW Action DIFF_HOST 验收设计

- **日期**：2026-08-03
- **状态**：已批准并实施
- **范围**：同机模拟 DIFF_HOST 下 Action（Goal / Feedback / Result）走 HYBRID→RTPS
- **不做**：改 Action 核心 API、Parameter sim、真双机签收

## 目标

新增精简示例 + `amw_sim_action.sh`，证明 Action 与 Pub/Sub、Service 一样可在跨机（模拟）路径工作。

## 交付

| 项 | 说明 |
|---|---|
| `examples/cpp/amw_action_server.cpp` | SimpleActionServer；3 步反馈 @50ms；Result success |
| `examples/cpp/amw_action_client.cpp` | 等 server → 发 goal → 日志 feedback/result |
| `scripts/amw_sim_action.sh` | 双假 IP；PASS = feedback + result |
| CMake / preflight / docs | 接入构建与文档一行命令 |

## 验收

```bash
./scripts/amw_sim_action.sh amw_cyclonedds 0 20
# PASS: ... feedback + result success
```

## 实现中修复的 DIFF_HOST 竞态

1. `Client::ServiceIsReady` 与 `WaitForService` 对齐：`HasService` + request reader + response writer  
2. `HybridTransmitter::Transmit`：RTPS 不依赖 Hybrid peer 记账（DDS 自行匹配）  
3. `Service::SendResponse`：发送前刷新 Enable 已知 response readers，并记录 Transmit 失败