# 术语

Autolink 常用概念（与实现一一对应）。

| 术语 | 含义 |
|---|---|
| **Node** | 通信句柄；Writer / Reader / Service / Client / Action / ParameterServer 均挂在其上 |
| **Channel** | 发布/订阅的数据通道名；同名即互通 |
| **Writer / Reader** | 向 Channel 写 / 从 Channel 读（回调或轮询） |
| **Service / Client** | 请求–响应；Client 发请求，Service 回响应 |
| **Action** | 长任务：Goal / Feedback / Result（可取消）；名称为前缀（如 `examples/simple_message_action`） |
| **Parameter** | 基于 Service 的参数读写；CLI/`ParameterClient` 使用 **ParameterServer 的 Node 名** |
| **Message** | 通道上传输的数据单元（protobuf、POD 包装或 `RawMessage`） |
| **Component** | 可加载算法模块；`Init` + `Proc`，由 DAG 描述拓扑，经 `autolink_mainboard` 加载 |
| **DAG / Launch** | DAG 描述组件与通道 / `.so`；Launch XML 一次启动多个 mainboard |
| **Record** | 录制/回放 Channel 消息的文件（`RecordWriter`/`Reader` 或 `autolink recorder`） |
| **Task / CRoutine** | 异步计算任务；协程以降低线程开销 |
| **Scheduler** | 任务调度策略（`classic` / `choreography`），见 [调度器](scheduler.md) |
| **Service discovery** | 去中心化发现对端 Node / Channel / Service（本机拓扑） |
| **HYBRID** | 默认传输：同进程 INTRA、同机 SHM |
| **DIFF_HOST** | 判定为异机（含同机故意设不同 `AUTOLINK_IP`）；当前回退为 SHM（仅同机有效） |

上手步骤见 [快速开始](quickstart.md)。
