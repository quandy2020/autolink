# Python API

绑定基于仓库内 `thirdparty/pybind11`，扩展模块 `autolink._core`，包名 **`import autolink`**。Channel / Service / Action 底层为 bytes；传入 protobuf 类时由糖层 Serialize/Parse。

可运行样例：`examples/python/`（多数脚本会 `_bootstrap_autolink` 自动加 `build/python`）。

## 构建与导入

```bash
cmake -S . -B build -DAUTOLINK_BUILD_PYTHON=ON
cmake --build build -j1 --target autolink _core autolink_python_pb2
pip3 install -r autolink/python/requirements.txt

export AUTOLINK_PATH=$PWD/autolink
export PYTHONPATH=$PWD/build/python
# Cyclone/iceoryx 来自 Humble 时务必把该路径放最前：
export LD_LIBRARY_PATH=/opt/ros/humble/lib/x86_64-linux-gnu:$PWD/build/lib:$LD_LIBRARY_PATH

python3 -c "import autolink; print(autolink.Node)"
ctest --test-dir build -R autolink_python --output-on-failure
```

安装树：`source <prefix>/bin/setup_autolink_python.sh`。

生成物：`*_pb2.py` 在 `build/python/`（含 `autolink.examples.examples_pb2`、`autolink.proto.*`）。  
模板实例化在 `libautolink` 的 `python/bridge/`（含 Action），`_core` 只包装 bridge。

## Lifecycle / Node

```python
import autolink

autolink.init("my_app")
node = autolink.Node("name")
# ...
# node.spin()          # 阻塞泵回调（按需）
autolink.shutdown()
```

| API | 说明 |
|---|---|
| `init(name)` | 对应 C++ `Init` |
| `is_shutdown()` | 循环退出条件 |
| `shutdown()` | 收尾 |

## Channel（Pub / Sub）

```python
from autolink.proto.unit_test_pb2 import Chatter  # 或 examples 中的 Chatter

writer = node.create_writer("channel/chatter", Chatter, qos_depth=6)
msg = Chatter(seq=1, content=b"hi")
writer.write(msg)

def on_msg(m):
    print(m.seq, m.content)

reader = node.create_reader("channel/chatter", on_msg, data_type=Chatter)
# 请保留 reader / service 等返回值，避免被 GC
```

`data_type`：protobuf 类、类型名字符串、或 `"RawData"`（bytes）。

运行：

```bash
cd examples/python
python3 py_listener.py   # 终端 1
python3 py_talker.py     # 终端 2
```

## Service / Client

```python
def handler(req):
    # 返回 response 实例
    ...

node.create_service("svc", handler, req_type=Req, res_type=Res)
client = node.create_client("svc", req_type=Req, res_type=Res)
resp = client.send_request(req, timeout_sec=5)
```

示例：`py_service.py` / `py_client.py`（`ChatterBenchmark`）。

## Action

Action 名与 C++ 一致：`examples/simple_message_action`。

```python
from autolink.examples.examples_pb2 import SimpleMessageAction

Goal = SimpleMessageAction.Goal
Feedback = SimpleMessageAction.Feedback
Result = SimpleMessageAction.Result
ACTION = "examples/simple_message_action"

def execute():
    goal = server.get_current_goal()
    server.publish_feedback(Feedback(index=0))
    if server.is_cancel_requested():
        server.terminate_current(Result(success=False))
        return
    server.succeeded_current(Result(success=True))

server = autolink.SimpleActionServer(
    node, ACTION, execute,
    goal_type=Goal, feedback_type=Feedback, result_type=Result)

client = autolink.ActionClient(
    node, ACTION,
    goal_type=Goal, feedback_type=Feedback, result_type=Result)
assert client.wait_for_server(timeout_sec=5.0)
handle = client.send_goal(Goal(text="hi"), feedback_cb=on_fb, timeout_sec=30.0)
result = handle.get_result(timeout_sec=60.0)
```

异步：`send_goal_async(..., goal_response_cb=, feedback_cb=, result_cb=)`。  
低层 `ActionServer`：在 `handle_accepted` 中先 `handle.execute()`，再 `succeed` / `abort` / `canceled`。

```bash
python3 py_action_server.py   # 终端 1
python3 py_action_client.py   # 终端 2
```

可与 C++ `action_listener` / `action_talker` 互通。

## Parameter

```python
server = autolink.ParameterServer(node)  # node 名即参数服务身份
client = autolink.ParameterClient(node, server_node_name)
client.set_parameter(autolink.Parameter("speed", 1.5))
client.set_parameter(autolink.Parameter("author_name", "WanderingEarth"))
lst = client.list_parameters()
```

示例：`py_parameter.py`（`global_parameter_service`）。

## Record / Time / Timer

```python
w = autolink.RecordWriter()
w.open("/tmp/demo.record")
# w.write_channel(...) / w.write_message(...) / w.close()

r = autolink.RecordReader("/tmp/demo.record")

autolink.Time.now().to_nsec()
autolink.Rate(100.0).sleep()
t = autolink.Timer(10, callback, oneshot=False)
t.start()
```

示例：`py_record.py`、`py_record_channel_info.py`、`py_record_trans.py`、`py_time.py`、`py_timer.py`。

## 与 C++ / CLI 对齐注意

| 项 | 说明 |
|---|---|
| 通道 / Action 名 | 与 C++ 示例字符串一致才能互通 |
| protobuf 类 | 须与对端同一 `.proto` 生成物 |
| Humble iceoryx | `LD_LIBRARY_PATH` 把 `/opt/ros/humble/lib/...` 放最前 |
| Domain / AMW | 跨机时对齐 [AMW](../amw/overview.md) 环境变量 |

命令行调试见 [CLI](../tools/cli.md)。
