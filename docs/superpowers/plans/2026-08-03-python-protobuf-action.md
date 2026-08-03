# Python Protobuf + Action Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 autolink Python 增加核心 `*_pb2.py` 生成与生产级 protobuf 示例，并绑定完整 Action 面（Client + Server 三回调 + SimpleActionServer，同步糖 + 异步回调）。

**Architecture:** CMake 用 `protoc --python_out` 生成核心 pb2 到 `build/python/`；Action 的 `Client`/`Server`/`SimpleActionServer` 模板在 `libautolink` 的 `python/bridge/action_bridge.*` 用 `RawMessage` 作为 Goal/Feedback/Result 实例化；`_core` 仅包装 bridge；Python `_action_sugar.py` 做 protobuf Serialize/Parse 与同步 API。

**Tech Stack:** C++17、pybind11、CMake、protoc、Python3、`protobuf` PyPI、pytest/ctest、既有 `autolink::action`

**Spec:** `docs/superpowers/specs/2026-08-03-python-protobuf-action-design.md`

**Notes:** 验证环境优先 Docker `SpaceHero`，路径 `/workspace/autonomy/autolink`；`_core` 链接用 `-j1`；**不要自动 git commit**，除非用户明确要求。

---

## File Structure

| Path | Responsibility |
|---|---|
| `autolink/python/CMakeLists.txt` | pb2 生成规则、新测试注册、`bind_action.cpp` |
| `autolink/CMakeLists.txt` | `action_bridge.cpp` 编入 `autolink` |
| `autolink/python/bridge/action_bridge.hpp/.cpp` | Bytes Action 非模板 API |
| `autolink/python/bindings/bind_action.hpp/.cpp` | pybind 枚举 + Client/Server/Simple + handles |
| `autolink/python/autolink/_action_sugar.py` | protobuf 糖 + 同步 `send_goal`/`get_result` |
| `autolink/python/autolink/__init__.py` | 再导出 Action API |
| `autolink/python/requirements.txt` | `protobuf>=4,<6` |
| `autolink/python/tests/test_protobuf_pubsub.py` | Channel protobuf round-trip（缺包 skip） |
| `autolink/python/tests/test_action.py` | 同进程 SimpleActionServer ↔ Client |
| `examples/python/py_{talker,listener,service,client}.py` | 改为 protobuf |
| `examples/python/py_action_{server,client}.py` | 对齐 C++ action 示例 |
| `docs/source/autolink_python_api_cn.md` | pb2 + Action 文档 |

**Bytes traits（bridge 内）：**

```cpp
struct BytesActionTraits {
  using Goal = message::RawMessage;
  using Feedback = message::RawMessage;
  using Result = message::RawMessage;
};
```

Wire 上 `action.proto` 的 `goal`/`feedback`/`result` 字段为 payload bytes；与 C++ `SimpleMessageAction_*` 的 `SerializeToString` 字节一致即可互通。

---

### Task 1: CMake 生成核心 pb2 + requirements

**Files:**
- Create: `autolink/python/requirements.txt`
- Modify: `autolink/python/CMakeLists.txt`
- Create: empty `__init__.py` generation for `proto/` and `examples/` under build python tree

- [ ] **Step 1: 写 requirements**

```text
# autolink/python/requirements.txt
protobuf>=4,<6
```

- [ ] **Step 2: 在 `python/CMakeLists.txt` 增加 pb2 生成（`AUTOLINK_BUILD_PYTHON` 已开启的上下文内）**

```cmake
set(AUTOLINK_PY_OUT "${CMAKE_BINARY_DIR}/python")
set(AUTOLINK_PY_PROTO_SRCS
  "${AUTOLINK_ROOT_DIR}/autolink/proto/unit_test.proto"
  "${AUTOLINK_ROOT_DIR}/autolink/proto/action.proto"
  "${AUTOLINK_ROOT_DIR}/autolink/proto/parameter.proto"
  "${AUTOLINK_ROOT_DIR}/examples/cpp/proto/examples.proto"
)
add_custom_command(
  OUTPUT
    "${AUTOLINK_PY_OUT}/autolink/proto/unit_test_pb2.py"
    "${AUTOLINK_PY_OUT}/autolink/proto/action_pb2.py"
    "${AUTOLINK_PY_OUT}/autolink/proto/parameter_pb2.py"
    "${AUTOLINK_PY_OUT}/autolink/examples/examples_pb2.py"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOLINK_PY_OUT}"
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
          --proto_path=${AUTOLINK_ROOT_DIR}/autolink/proto
          --proto_path=${AUTOLINK_ROOT_DIR}/examples/cpp/proto
          --python_out=${AUTOLINK_PY_OUT}
          ${AUTOLINK_PY_PROTO_SRCS}
  COMMAND ${CMAKE_COMMAND} -E touch
          "${AUTOLINK_PY_OUT}/autolink/proto/__init__.py"
          "${AUTOLINK_PY_OUT}/autolink/examples/__init__.py"
  DEPENDS ${AUTOLINK_PY_PROTO_SRCS}
  COMMENT "Generating Python protobuf (*_pb2.py)"
)
add_custom_target(autolink_python_pb2 ALL DEPENDS
  "${AUTOLINK_PY_OUT}/autolink/proto/unit_test_pb2.py"
  "${AUTOLINK_PY_OUT}/autolink/examples/examples_pb2.py")
add_dependencies(_core autolink_python_pb2)
```

注意：`unit_test.proto` 的 `package autolink.proto` → 输出在 `python/autolink/proto/`；`examples.proto` 的 `package autolink.examples` → `python/autolink/examples/`。若 `protoc` 对多 `--proto_path` 行为异常，拆成两次 `protoc` 调用。

- [ ] **Step 3: 配置并生成，验证文件存在**

```bash
cmake -S . -B build -DAUTOLINK_BUILD_PYTHON=ON
cmake --build build -j1 --target autolink_python_pb2
ls build/python/autolink/proto/unit_test_pb2.py \
   build/python/autolink/examples/examples_pb2.py
```

Expected: 文件存在。

- [ ] **Step 4: 安装 protobuf 并验证 import（Docker）**

```bash
pip3 install -r autolink/python/requirements.txt
PYTHONPATH=build/python python3 -c \
  "from autolink.proto.unit_test_pb2 import Chatter; print(Chatter)"
```

Expected: 打印类型；失败则修 CMake 输出路径。

- [ ] **Step 5: Commit（仅当用户要求）**

---

### Task 2: protobuf Channel 单测 + 改造 talker/listener

**Files:**
- Create: `autolink/python/tests/test_protobuf_pubsub.py`
- Modify: `examples/python/py_talker.py`, `py_listener.py`
- Modify: `autolink/python/CMakeLists.txt`（注册测试）

- [ ] **Step 1: 写单测（缺包 skip）**

```python
# autolink/python/tests/test_protobuf_pubsub.py
import threading
import time

import pytest

pytest.importorskip("google.protobuf")

import autolink
from autolink.proto.unit_test_pb2 import Chatter


def test_protobuf_pubsub_same_process():
    autolink.init("py_test_pb_pubsub")
    received = []
    ev = threading.Event()
    sub = autolink.Node("pb_sub")
    pub = autolink.Node("pb_pub")

    def on_msg(msg):
        received.append(msg)
        ev.set()

    sub.create_reader("py/pb_chatter", on_msg, data_type=Chatter)
    writer = pub.create_writer("py/pb_chatter", Chatter, qos_depth=6)
    msg = Chatter(timestamp=1, lidar_timestamp=1, seq=7, content=b"pb-hi")
    deadline = time.monotonic() + 3.0
    while not ev.is_set() and time.monotonic() < deadline:
        writer.write(msg)
        ev.wait(0.05)
    assert ev.is_set()
    assert received[0].seq == 7
    assert received[0].content == b"pb-hi"
    autolink.shutdown()


if __name__ == "__main__":
    test_protobuf_pubsub_same_process()
    print("ok")
```

- [ ] **Step 2: 注册 ctest（与现有测试同样 ENVIRONMENT）**

- [ ] **Step 3: 改造 `py_talker.py` / `py_listener.py` 用 Chatter**

```python
# talker 核心
from autolink.proto.unit_test_pb2 import Chatter
writer = test_node.create_writer("channel/chatter", Chatter, qos_depth=6)
msg = Chatter()
msg.timestamp = autolink.Time.now().to_nsec()
msg.content = b"I am python talker."
writer.write(msg)
```

缺 `google.protobuf` 时：`sys.exit("pip install -r autolink/python/requirements.txt")`。

- [ ] **Step 4: 跑测**

```bash
pip3 install -r autolink/python/requirements.txt
cmake --build build -j1 --target _core autolink_python_pb2
cp -f autolink/python/autolink/*.py build/python/autolink/
PYTHONPATH=build/python python3 autolink/python/tests/test_protobuf_pubsub.py
# Expected: ok
```

- [ ] **Step 5: Commit（仅当用户要求）**

---

### Task 3: protobuf service/client examples

**Files:**
- Modify: `examples/python/py_service.py`, `py_client.py`
- Create (optional): `autolink/python/tests/test_protobuf_service.py`

- [ ] **Step 1: service/client 用 `ChatterBenchmark`**

```python
from autolink.proto.unit_test_pb2 import ChatterBenchmark

def callback(data):
    return ChatterBenchmark(content="svr: Hello client!", seq=data.seq + 2)

node.create_service("server_01", callback, req_type=ChatterBenchmark,
                    res_type=ChatterBenchmark)
# client:
client = node.create_client("server_01", req_type=ChatterBenchmark,
                            res_type=ChatterBenchmark)
```

- [ ] **Step 2: 可选单测同进程往返（缺包 skip）；注册 ctest**

- [ ] **Step 3: 跑通并 Commit（仅当用户要求）**

---

### Task 4: `action_bridge` — SimpleActionServer + Client 最小面

**Files:**
- Create: `autolink/python/bridge/action_bridge.hpp`
- Create: `autolink/python/bridge/action_bridge.cpp`
- Modify: `autolink/CMakeLists.txt`（`target_sources` 增加 `action_bridge.cpp`）

- [ ] **Step 1: 定义非模板句柄 API（头文件骨架）**

```cpp
// action_bridge.hpp — 关键符号，实现细节在 .cpp
namespace autolink::python_support {

struct BytesActionTraits {
  using Goal = message::RawMessage;
  using Feedback = message::RawMessage;
  using Result = message::RawMessage;
};

class ActionClientHandle { /* wait, send async/sync helpers, cancel */ };
class ClientGoalHandleView { /* status, get_result, cancel */ };
class SimpleActionServerHandle { /* execute_cb wired in ctor */ };
class ActionServerHandle { /* three callbacks */ };
class ServerGoalHandleView { /* feedback/succeed/abort/... */ };

}  // namespace
```

具体方法签名对齐 spec §4.3；`SendGoal` 等接受 `std::string` payload，内部包进 `RawMessage`。

- [ ] **Step 2: 在 `.cpp` 中 `#include` action 头并实例化 `Client`/`SimpleActionServer`/`Server<BytesActionTraits>`**

禁止在 bindings TU 包含会触发隐式模板析构实例化的完整 `Client<>` 头——bindings 只包含 `action_bridge.hpp`。

- [ ] **Step 3: 编译 `autolink`**

```bash
cmake --build build -j1 --target autolink
# Expected: success
```

- [ ] **Step 4: Commit（仅当用户要求）**

---

### Task 5: `bind_action` + 枚举 + SimpleActionServer 闭环

**Files:**
- Create: `autolink/python/bindings/bind_action.hpp/.cpp`
- Modify: `bindings/module.cpp`, `python/CMakeLists.txt`
- Create: `autolink/python/autolink/_action_sugar.py`
- Modify: `autolink/python/autolink/__init__.py`
- Create: `autolink/python/tests/test_action.py`

- [ ] **Step 1: 写失败的 action 单测（可用 RawMessage 路径：bytes goal）**

```python
# tests/test_action.py
import threading
import time

import autolink


def test_simple_action_bytes_roundtrip():
    autolink.init("py_test_action")
    node = autolink.Node("action_node")

    def execute():
        # sugar/simple API: get goal bytes, succeed with result bytes
        goal = simple.get_current_goal()  # bytes or Raw wrapper
        simple.publish_feedback(b"fb")
        simple.succeeded_current(b"ok:" + goal)

    simple = autolink.SimpleActionServer(
        node, "py/test_action", execute, goal_type=None,  # RawData
        feedback_type=None, result_type=None)
    # Keep refs on node via sugar
    client = autolink.ActionClient(node, "py/test_action")
    assert client.wait_for_server(timeout_sec=5)
    handle = client.send_goal(b"g1", timeout_sec=5)
    assert handle is not None
    result = handle.get_result(timeout_sec=10)
    assert result == b"ok:g1" or getattr(result, "message", None) == b"ok:g1"
    autolink.shutdown()
```

（实现时按最终 sugar 签名微调断言；TDD：先红后绿。）

- [ ] **Step 2: `BindAction` 暴露枚举 + SimpleActionServerHandle + ActionClientHandle（bytes）**

```cpp
py::enum_<action::GoalStatus>(m, "GoalStatus")
  .value("UNKNOWN", action::GoalStatus::UNKNOWN)
  // ... ACCEPTED..ABORTED
  ;
// GoalResponse, CancelResponse, ResultCode 同理
```

回调进 Python 前 `gil_scoped_acquire`；`wait_for_server` / `send_goal` 等待释放 GIL。

- [ ] **Step 3: `_action_sugar.py` 包装 protobuf 类与 RawData；挂 `_autolink_actions`**

```python
def create_simple_action_server(node, name, execute_cb, Goal, Feedback, Result,
                                completion_cb=None):
    # if Goal is protobuf class: wrap execute to parse/serialize
    handle = node._create_simple_action_server(...)  # or module ctor
    _retain(node, handle)
    return handle
```

- [ ] **Step 4: 跑 `test_action.py` — Expected: PASS**

```bash
cmake --build build -j1 --target autolink _core
cp -f autolink/python/autolink/*.py build/python/autolink/
PYTHONPATH=build/python python3 autolink/python/tests/test_action.py
```

- [ ] **Step 5: Commit（仅当用户要求）**

---

### Task 6: ActionClient 异步回调 + ClientGoalHandle 完整 API

**Files:**
- Modify: `action_bridge.*`, `bind_action.cpp`, `_action_sugar.py`
- Extend: `tests/test_action.py`

- [ ] **Step 1: 增加 `send_goal_async` 与 `goal_response_cb` / `feedback_cb` / `result_cb`**

测试：异步发送后用 `threading.Event` 等 result_cb；校验 feedback 至少一次。

- [ ] **Step 2: `cancel` 同步路径单测（server execute 中 `is_cancel_requested` 后 `terminate_current`）**

- [ ] **Step 3: PASS 后 Commit（仅当用户要求）**

---

### Task 7: 低层 ActionServer 三回调 + ServerGoalHandle

**Files:**
- Modify: `action_bridge.*`, `bind_action.cpp`, `_action_sugar.py`
- Extend: `tests/test_action.py`（`test_low_level_action_server`）

- [ ] **Step 1: 绑定 `ActionServer(handle_goal, handle_cancel, handle_accepted)`**

在 `handle_accepted` 里起线程 `execute()` + `publish_feedback` + `succeed(result)`。

- [ ] **Step 2: Client 往返单测 PASS**

- [ ] **Step 3: Commit（仅当用户要求）**

---

### Task 8: Action examples（对齐 C++）+ protobuf Goal

**Files:**
- Rewrite: `examples/python/py_action_server.py`, `py_action_client.py`
- Modify: `examples/python/README.md`

- [ ] **Step 1: server 用 `SimpleActionServer` + `SimpleMessageAction`**

```python
from autolink.examples.examples_pb2 import SimpleMessageAction
Goal = SimpleMessageAction.Goal
Feedback = SimpleMessageAction.Feedback
Result = SimpleMessageAction.Result
ACTION = "examples/simple_message_action"

def execute():
    goal = server.get_current_goal()
    for i in range(3):
        if server.is_cancel_requested():
            server.terminate_current()
            return
        server.publish_feedback(Feedback(index=i))
        time.sleep(0.2)
    server.succeeded_current(Result(success=True))
```

- [ ] **Step 2: client `wait_for_server` + `send_goal` + `get_result`，可选 feedback_cb**

- [ ] **Step 3: 手动双终端跑 Python server/client；可选再对 C++ `action_listener`/`action_talker`**

```bash
# term1
PYTHONPATH=build/python python3 examples/python/py_action_server.py
# term2
PYTHONPATH=build/python python3 examples/python/py_action_client.py
# Expected: client 打印 SUCCEEDED / success=true
```

- [ ] **Step 4: Commit（仅当用户要求）**

---

### Task 9: 文档 + Spec 交叉引用 + 全量验收

**Files:**
- Modify: `docs/source/autolink_python_api_cn.md`
- Modify: `docs/superpowers/specs/2026-08-03-python-pybind11-design.md`（非目标 Action → 指向本 spec）
- Modify: `examples/python/README.md`

- [ ] **Step 1: 文档增加 pb2 构建、`pip install -r …/requirements.txt`、Action API 与 examples 步骤**

- [ ] **Step 2: 全量验收**

```bash
pip3 install -r autolink/python/requirements.txt
cmake --build build -j1 --target autolink _core autolink_python_pb2
cp -f autolink/python/autolink/*.py build/python/autolink/
ctest --test-dir build -R autolink_python --output-on-failure
PYTHONPATH=build/python python3 -c \
  "import autolink; from autolink.proto.unit_test_pb2 import Chatter; \
   print(autolink.ActionClient, Chatter)"
```

Expected: 全部 PASS；Chatter / ActionClient 可打印。

- [ ] **Step 3: 对照 spec §6 验收清单逐条勾选**

- [ ] **Step 4: Commit（仅当用户要求）**

---

## Spec Coverage Check

| Spec 项 | Task |
|---|---|
| CMake 核心 pb2 | 1 |
| requirements + skip | 1–2 |
| protobuf Channel examples/tests | 2 |
| protobuf Service examples | 3 |
| action_bridge + Bytes traits | 4 |
| SimpleActionServer + Client bytes | 5 |
| 异步回调 + cancel | 6 |
| 低层 ActionServer | 7 |
| py_action_* + C++ 对齐名 | 8 |
| 文档 / 验收 | 9 |

## Self-Review

- 无 TBD/「类似 Task N」占位；Bytes traits 与 RawMessage 前后一致  
- GIL / bridge 约束与 v1 踩坑一致  
- Commit 步骤标注「仅当用户要求」，符合仓库约定  

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-08-03-python-protobuf-action.md`.

**两种执行方式：**

1. **Subagent-Driven（推荐）** — 每任务新开子代理，任务间复查  
2. **Inline Execution** — 本会话按 `executing-plans` 连续推进并设检查点  

选哪一种？
