# AMW Parameter DIFF_HOST 验收设计

- **日期**：2026-08-03
- **状态**：已完成
- **范围**：同机模拟 DIFF_HOST 下 ParameterServer/Client（Set/Get）走 HYBRID→RTPS
- **不做**：改 Parameter 协议、真双机签收

## 交付

| 项 | 说明 |
|---|---|
| `amw_param_server.cpp` | 固定节点名 `amw_param_server`；预置 `amw_demo_int=42` |
| `amw_param_client.cpp` | `WaitForService` → Set `amw_demo_str` → Get int/str → 可 grep 日志 |
| `ParameterClient::WaitForService` | 对齐底层 Service ready |
| `amw_sim_param.sh` | 双假 IP；PASS = set ok + get int/str |

## 验收

```bash
./scripts/amw_sim_param.sh amw_cyclonedds 0 15
```
