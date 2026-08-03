# Python：生产级 Protobuf 示例 + Action 绑定

日期：2026-08-03  
状态：待审阅  
前置：`docs/superpowers/specs/2026-08-03-python-pybind11-design.md`（v1 已落地）  
范围：`autolink/python`、`examples/python`、`docs/source/autolink_python_api_cn.md`、CMake pb2 生成

## 1. 背景与目标

v1 已用 pybind11 完成 lifecycle / Node / PubSub / Service / Parameter / Record / Time / Utils，examples 默认 RawData。缺口：

- 无 `*_pb2.py` 生成流水线，protobuf 糖无法在构建树里验证
- Action 为设计非目标，仅有占位 `py_action_*.py`

**目标（本迭代）**

1. **生产级 Protobuf**：CMake 生成核心 pb2；talker/listener/service/client 用 protobuf 消息；单测覆盖 Channel protobuf round-trip  
2. **Action 完整面**：`ActionClient` + 低层 `ActionServer`（三回调）+ `SimpleActionServer`；Client 同步糖 + 异步回调；优先与 C++ `action_talker` / `action_listener` 互通  

**非目标**

- ROS 2 / rmw 互操作  
- 全量 `autolink/proto/*.proto` 的 Python 生成（仅核心集）  
- 把全部 protobuf C++ 类型绑进 pybind11  

## 2. 已确认决策

| 项 | 选择 |
|---|---|
| 范围 | 同迭代：pb2 + Action |
| Action API | 完整：Client + Server（三回调）+ SimpleActionServer |
| Client 调用模型 | 同步糖 + 异步回调（双轨） |
| pb2 生成集 | `unit_test` / `action` / `parameter` / `examples` |
| protobuf 运行时包 | 文档 `pip install` + 缺包时相关测试 skip（不阻塞 `_core` 构建） |
| 实现路径 | C++ `action_bridge`（bytes ActionT）+ Python 糖；模板不进 `_core.so` |

## 3. Protobuf 流水线

### 3.1 生成

`AUTOLINK_BUILD_PYTHON=ON` 时，用 `Protobuf_PROTOC_EXECUTABLE`：

| Proto | 用途 |
|---|---|
| `autolink/proto/unit_test.proto` | Chatter / ChatterBenchmark 示例与单测 |
| `autolink/proto/action.proto` | Action wire（如需 Python 侧调试） |
| `autolink/proto/parameter.proto` | 参数相关（可选使用） |
| `examples/cpp/proto/examples.proto` | SimpleMessageAction Goal/Feedback/Result |

输出根：`${CMAKE_BINARY_DIR}/python/`（protoc 按 `package` 生成目录，例如 `autolink/proto/unit_test_pb2.py`、`autolink/examples/examples_pb2.py`）。  
补充空 `__init__.py`；install 同步到前缀 `python/`。

导入示例（安装 protobuf 后）：

```python
from autolink.proto.unit_test_pb2 import Chatter, ChatterBenchmark
from autolink.examples.examples_pb2 import SimpleMessageAction
Goal = SimpleMessageAction.Goal
Feedback = SimpleMessageAction.Feedback
Result = SimpleMessageAction.Result
```

说明：`examples.proto` 的 `package autolink.examples` 使 pb2 落在包内子目录 `autolink/examples/`（与 `_core` 同属 `build/python/autolink/` 树）；需放置空 `__init__.py`。若 protoc 版本/路径有差异，以实际生成文件为准并在 bootstrap/文档中写死最终 import。

### 3.2 依赖

- 新增 `autolink/python/requirements.txt`：`protobuf>=4,<6`  
- 构建只生成 pb2，**不**因缺 Python protobuf 失败  
- 依赖 protobuf 的测试：`import google.protobuf` 失败则 skip并提示安装  
- examples：有 pb2 + protobuf 时用 protobuf；否则 **明确报错退出**（避免 RawData 假绿）

### 3.3 Examples / 测试

- 改造 `py_talker` / `py_listener` / `py_service` / `py_client` 为 protobuf  
- 新增 `tests/test_protobuf_pubsub.py`（及必要时 service protobuf）  
- 保留现有 RawData 测试  

## 4. Action 绑定

### 4.1 架构

```
Python 糖 (_action_sugar.py)
  → Serialize/Parse Goal·Feedback·Result
autolink._core (bind_action.cpp，无模板实例化)
  →
libautolink python/bridge/action_bridge.*
  → Client / Server / SimpleActionServer<BytesActionTraits>
  → autolink::action::*
```

硬约束：与 channel/parameter/utils 相同——Action 相关模板静态态只存在于 `libautolink.so`。

### 4.2 Bytes ActionT

Bridge 内定义 traits，Goal/Feedback/Result 为可序列化 bytes 载荷（与 `action.proto` 内嵌 payload 一致），使 Python protobuf 与 C++ `SimpleMessageAction` **payload bytes 互通**（同一 action 名、同一 wire）。

### 4.3 公开 API（snake_case）

枚举（`IntEnum` 或模块常量）：`GoalStatus` / `GoalResponse` / `CancelResponse` / `ResultCode`。

```python
client = autolink.ActionClient(node, name, Goal, Feedback, Result)
server = autolink.ActionServer(
    node, name, handle_goal, handle_cancel, handle_accepted,
    Goal, Feedback, Result)
simple = autolink.SimpleActionServer(
    node, name, execute_cb, Goal, Feedback, Result,
    completion_cb=None)
```

对象须挂在 Node（如 `_autolink_actions`）防止 GC。

**ActionClient**

| 方法 | 说明 |
|---|---|
| `wait_for_server(timeout_sec=-1)` | 释放 GIL |
| `server_is_ready()` | bool |
| `send_goal_async(goal, goal_response_cb=None, feedback_cb=None, result_cb=None)` | 异步 |
| `send_goal(goal, feedback_cb=None, timeout_sec=30)` | 同步等到 accept |
| handle.`get_result` / `get_result_async` / `cancel` | 同步/异步 |
| handle 只读 | `goal_id`, `status`, `is_succeeded`, … |

**ActionServer**  
`handle_goal(uuid, goal) -> GoalResponse`；`handle_cancel(handle) -> CancelResponse`；`handle_accepted(handle)`。  
`ServerGoalHandle`：`publish_feedback` / `succeed` / `abort` / `canceled` / `execute` / `get_goal` / 状态查询。

**SimpleActionServer**  
`get_current_goal` / `publish_feedback` / `succeeded_current` / `terminate_current` / `is_cancel_requested` / `is_preempt_requested` / `accept_pending_goal`；可选 `activate`/`deactivate`。

### 4.4 GIL

- 进入 Python 回调：`gil_scoped_acquire`  
- `wait_*` / 同步 RPC：`gil_scoped_release`  
- 持有 `py::function` 的对象析构须持 GIL  
- 回调未捕获异常：绑定层捕获、打日志；不穿过 C++ 工作线程导致崩溃  

### 4.5 Examples / 测试

- 实现 `py_action_server.py` / `py_action_client.py`（对齐 C++，action 名 `examples/simple_message_action`）  
- 同进程 Client ↔ SimpleActionServer 单测  
- 可选：与 C++ binary 互通的集成测试（环境不具备则 skip）  

## 5. 文档与清理

- 更新 `docs/source/autolink_python_api_cn.md`：pb2 构建、protobuf 示例、Action API  
- 更新 `examples/python/README.md`：requirements、action 运行步骤  
- 修订 v1 spec「非目标：Action」说明：由本 spec 覆盖  

## 6. 验收清单

1. `-DAUTOLINK_BUILD_PYTHON=ON` 生成核心 `*_pb2.py`，`import` 成功（已装 protobuf）  
2. protobuf talker/listener（及 service/client）可跑  
3. Action client/server examples 可跑；理想情况与 C++ action 示例互通  
4. `ctest -R autolink_python` 全绿；缺 protobuf 时相关用例 skip 而非 fail  
5. API 文档已更新；无回退到 `autolink_py3` / `_loader`  

## 7. 风险

| 风险 | 缓解 |
|---|---|
| Action 模板进 `_core` 再发段错误 | 仅 bridge 实例化；对照 channel_bridge |
| GIL 死锁（execute 线程回调 Python） | 统一 gil_utils；同步 API 释放 GIL |
| examples.proto 包路径与 import 不一致 | 以 protoc 输出为准写 `__init__` / 文档 |
| 与 C++ Action 互通失败 | 对齐 action 名与 payload 编码；先同进程 Python 绿再跨语言 |

## 8. 实施顺序（概要）

1. CMake pb2 + requirements + protobuf pubsub 单测 + 改造 channel examples  
2. `action_bridge` + `bind_action` + 枚举 + SimpleActionServer 最小闭环  
3. ActionClient 异步 + 同步糖 + ClientGoalHandle  
4. 低层 ActionServer 三回调 + ServerGoalHandle  
5. examples / 文档 / 验收  

详细任务拆解见后续 implementation plan（spec 批准后编写）。
