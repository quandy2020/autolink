# Python pybind11 绑定迁移 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 `thirdparty/pybind11` 将 `autolink/python` 全量替换为单一扩展 `autolink._core`，并更新 examples/文档。

**Architecture:** CMake 引入 vendored pybind11，产出包内 `_core` 模块；**Writer/Reader/Service/Client 模板实例化必须放在 `libautolink` 的 `python/bridge/` 中**（禁止在 `_core.so` 的 `bindings/*.cpp` 中实例化，否则与 `libautolink.so` 重复模板静态态会导致 `init()` 段错误）；pybind 仅包装非模板 bridge；Python 薄层提供 protobuf 序列化糖与包级再导出。旧 CPython C API / ctypes / `autolink_py3` 全部删除。

**Tech Stack:** C++17、pybind11（submodule）、CMake、Python3、pytest/ctest、现有 `libautolink`

**Spec:** `docs/superpowers/specs/2026-08-03-python-pybind11-design.md`

---

## File Structure

| Path | Responsibility |
|---|---|
| `autolink/python/CMakeLists.txt` | pybind11 + `_core` 构建/安装/测试 |
| `autolink/python/bindings/module.cpp` | `PYBIND11_MODULE(_core, m)` 入口，调用各 `Bind*` |
| `autolink/python/bindings/gil_utils.hpp` | 回调异常捕获 + GIL acquire 辅助 |
| `autolink/python/bindings/bind_lifecycle.*` | `init`/`ok`/`shutdown`/`is_shutdown`/`wait_for_shutdown` |
| `autolink/python/bindings/bind_time.*` | `Time`/`Duration`/`Rate` |
| `autolink/python/bindings/bind_timer.*` | `Timer` |
| `autolink/python/bindings/bind_node.*` | `Node`/`Writer`/`Reader`/`Service`/`Client` + descriptor 注册 |
| `autolink/python/bindings/bind_parameter.*` | `Parameter`/`ParameterClient`/`ParameterServer` |
| `autolink/python/bindings/bind_record.*` | `RecordReader`/`RecordWriter`/`BagMessage` |
| `autolink/python/bindings/bind_utils.*` | `ChannelUtils`/`NodeUtils`/`ServiceUtils` |
| `autolink/python/autolink/__init__.py` | 再导出公共 API |
| `autolink/python/autolink/_node_sugar.py` | protobuf write/read 糖（可选，Task 3 起） |
| `autolink/python/tests/*.py` | smoke + 单测 + 轻量集成 |
| `examples/python/*` | 改为 `import autolink` |
| `docs/source/autolink_python_api_cn.md` | 新 API 文档 |

**Delete after cutover:** `autolink/python/src/*`（旧胶水与扁平 py）、旧 `_*_wrapper` targets、`_loader.py`。

**Build output:** `${CMAKE_BINARY_DIR}/python/autolink/_core${EXT_SUFFIX}` + 同步拷贝 `autolink/*.py`，使

```bash
PYTHONPATH=${CMAKE_BINARY_DIR}/python python3 -c "import autolink"
```

可用。

---

### Task 1: CMake + 空 `_core` + smoke import

**Files:**
- Rewrite: `autolink/python/CMakeLists.txt`
- Create: `autolink/python/bindings/module.cpp`
- Create: `autolink/python/autolink/__init__.py`
- Create: `autolink/python/tests/test_smoke_import.py`
- Modify: top-level only if needed to keep `add_subdirectory(python)`（已有则不动）

- [ ] **Step 1: 写失败的 smoke 测试**

```python
# autolink/python/tests/test_smoke_import.py
import autolink


def test_import_core_attr():
    assert hasattr(autolink, "__version__") or hasattr(autolink, "_core")
    # Task1 最低要求：能 import 且暴露 _core
    from autolink import _core  # noqa: F401
```

- [ ] **Step 2: 重写 CMakeLists（pybind11 + 空模块）**

```cmake
# autolink/python/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(
  "${AUTOLINK_ROOT_DIR}/thirdparty/pybind11"
  "${CMAKE_BINARY_DIR}/thirdparty/pybind11"
  EXCLUDE_FROM_ALL)

set(AUTOLINK_PY_PKG_DIR "${CMAKE_BINARY_DIR}/python/autolink")
file(MAKE_DIRECTORY "${AUTOLINK_PY_PKG_DIR}")

set(AUTOLINK_PY_BIND_SOURCES
  bindings/module.cpp
  # later tasks append: bind_lifecycle.cpp ...
)

pybind11_add_module(_core ${AUTOLINK_PY_BIND_SOURCES})
target_include_directories(_core PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/bindings"
  "${AUTOLINK_ROOT_DIR}")
target_link_libraries(_core PRIVATE autolink)
set_target_properties(_core PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${AUTOLINK_PY_PKG_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${AUTOLINK_PY_PKG_DIR}")

# Copy package py files into build tree on configure/build
file(GLOB AUTOLINK_PY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/autolink/*.py")
foreach(PY ${AUTOLINK_PY_FILES})
  get_filename_component(NAME "${PY}" NAME)
  configure_file("${PY}" "${AUTOLINK_PY_PKG_DIR}/${NAME}" COPYONLY)
endforeach()

install(TARGETS _core
  LIBRARY DESTINATION "python/autolink"
  RUNTIME DESTINATION "python/autolink")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/autolink/"
  DESTINATION "python/autolink"
  FILES_MATCHING PATTERN "*.py")

if(AUTOLINK_BUILD_TEST)
  add_test(NAME autolink_python_smoke_import
    COMMAND ${Python3_EXECUTABLE} -m pytest
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_smoke_import.py" -q)
  set_tests_properties(autolink_python_smoke_import PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python")
endif()
```

- [ ] **Step 3: 空模块 + `__init__.py`**

```cpp
// bindings/module.cpp
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(_core, m) {
  m.doc() = "autolink Python bindings (pybind11)";
  m.attr("__version__") = "0.1.0-pybind11";
}
```

```python
# autolink/python/autolink/__init__.py
from . import _core as _core  # noqa: F401

__all__ = ["_core"]
```

- [ ] **Step 4: 配置并编译，确认 smoke 通过**

```bash
cmake -S . -B build -DAUTOLINK_BUILD_PYTHON=ON -DAUTOLINK_BUILD_TEST=ON
cmake --build build -j$(nproc) --target _core
PYTHONPATH=build/python python3 -c "import autolink; import autolink._core; print(autolink._core.__version__)"
# Expected: 0.1.0-pybind11
ctest --test-dir build -R autolink_python_smoke_import --output-on-failure
# Expected: PASS（若无 pytest，可临时改为直接跑 test 文件）
```

若环境无 pytest：把 ctest COMMAND 改为  
`${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_smoke_import.py"`  
并在测试文件底部加 `if __name__ == "__main__": test_import_core_attr()`。

- [ ] **Step 5: Commit（若用户要求提交）**

```bash
git add autolink/python/CMakeLists.txt autolink/python/bindings autolink/python/autolink autolink/python/tests/test_smoke_import.py
git commit -m "$(cat <<'EOF'
feat(python): scaffold pybind11 _core package layout

EOF
)"
```

---

### Task 2: Lifecycle + Time/Duration/Rate + Timer

**Files:**
- Create: `autolink/python/bindings/gil_utils.hpp`
- Create: `autolink/python/bindings/bind_lifecycle.hpp`, `bind_lifecycle.cpp`
- Create: `autolink/python/bindings/bind_time.hpp`, `bind_time.cpp`
- Create: `autolink/python/bindings/bind_timer.hpp`, `bind_timer.cpp`
- Modify: `bindings/module.cpp`、`CMakeLists.txt`（加入源文件）
- Modify: `autolink/__init__.py`
- Create: `autolink/python/tests/test_time.py`

- [ ] **Step 1: 写 Time 单测（先失败）**

```python
# tests/test_time.py
import autolink


def test_time_now_and_rate():
    autolink.init("py_test_time")
    t = autolink.Time.now()
    assert t.to_nsec() > 0
    d = autolink.Duration(1_000_000)  # 1ms in ns
    r = autolink.Rate(100.0)
    r.sleep()
    autolink.shutdown()
```

- [ ] **Step 2: `gil_utils.hpp`**

```cpp
#pragma once
#include <pybind11/pybind11.h>
#include "autolink/common/log.hpp"

namespace autolink::pybind {

inline void CallPyVoid(const pybind11::function& fn) {
  if (!fn) return;
  pybind11::gil_scoped_acquire gil;
  try {
    fn();
  } catch (pybind11::error_already_set& e) {
    AERROR << "Python callback error: " << e.what();
    e.restore();
    PyErr_Clear();
  }
}

}  // namespace autolink::pybind
```

- [ ] **Step 3: 绑定 lifecycle**

```cpp
// bind_lifecycle.cpp
#include "bind_lifecycle.hpp"
#include <pybind11/pybind11.h>
#include "autolink/init.hpp"
#include "autolink/autolink.hpp"

namespace py = pybind11;

void BindLifecycle(py::module_& m) {
  m.def("init", [](const std::string& name) {
    if (!autolink::Init(name.c_str())) {
      throw py::runtime_error("autolink.init failed: " + name);
    }
  }, py::arg("module_name") = "autolink");

  m.def("ok", &autolink::OK);
  m.def("shutdown", &autolink::Clear);
  m.def("is_shutdown", &autolink::IsShutdown);
  m.def("wait_for_shutdown", &autolink::WaitForShutdown);
}
```

（`bind_lifecycle.hpp` 仅声明 `void BindLifecycle(pybind11::module_&);`）

- [ ] **Step 4: 绑定 Time/Duration/Rate**

直接绑 `autolink::Time` / `Duration` / `Rate`，方法名用 snake_case：

```cpp
py::class_<autolink::Time>(m, "Time")
  .def(py::init<>())
  .def(py::init<uint64_t>())
  .def(py::init<double>())
  .def_static("now", &autolink::Time::Now)
  .def_static("mono_time", &autolink::Time::MonoTime)
  .def_static("sleep_until", &autolink::Time::SleepUntil)
  .def("to_sec", &autolink::Time::ToSecond)
  .def("to_nsec", &autolink::Time::ToNanosecond);

py::class_<autolink::Duration>(m, "Duration")
  .def(py::init<int64_t>())
  .def("sleep", &autolink::Duration::Sleep);

py::class_<autolink::Rate>(m, "Rate")
  .def(py::init<double>())
  .def("sleep", &autolink::Rate::Sleep);
```

（确认 `Duration`/`Rate` 构造签名与头文件一致；不一致则以头文件为准微调。）

- [ ] **Step 5: 绑定 Timer**

```cpp
py::class_<autolink::Timer>(m, "Timer")
  .def(py::init([](uint32_t period_ms, py::function cb, bool oneshot) {
      autolink::TimerOption opt(
          period_ms,
          [cb]() { autolink::pybind::CallPyVoid(cb); },
          oneshot);
      return std::make_unique<autolink::Timer>(opt);
    }),
    py::arg("period_ms"), py::arg("callback"), py::arg("oneshot") = false)
  .def("start", &autolink::Timer::Start)
  .def("stop", &autolink::Timer::Stop);
```

（若 `Timer` 不可 `unique_ptr` 移动，改用 `py::class_<Timer, std::shared_ptr<Timer>>` 或值类型；以 `timer.hpp` 为准。）

- [ ] **Step 6: 在 `module.cpp` 调用 Bind\*；更新 `__init__.py` 再导出**

```python
from ._core import (
    init, ok, shutdown, is_shutdown, wait_for_shutdown,
    Time, Duration, Rate, Timer,
)
__all__ = [ ... ]
```

- [ ] **Step 7: 编译并跑 `test_time.py` — Expected: PASS**

- [ ] **Step 8: Commit（若用户要求）**

---

### Task 3: Node + Writer/Reader（含 protobuf 糖）

**Files:**
- Create: `bindings/bind_node.hpp`, `bind_node.cpp`（可含内部 `ChannelWriter`/`ChannelReader` 辅助类）
- Create: `autolink/_node_sugar.py`（或逻辑放 `__init__.py`）
- Create: `tests/test_pubsub.py`
- Modify: `module.cpp`、`CMakeLists.txt`、`__init__.py`

- [ ] **Step 1: 写 pubsub 集成测试（同进程）**

```python
# tests/test_pubsub.py
import threading
import time
import autolink


def test_bytes_pubsub():
    autolink.init("py_test_pubsub")
    node = autolink.Node("pubsub_node")
    got = []

    def cb(data: bytes):
        got.append(data)

    reader = node.create_reader("/py/test/ch", callback=cb)
    writer = node.create_writer("/py/test/ch", msg_type="RawData", qos_depth=10)
    time.sleep(0.2)
    assert writer.write(b"hello-pybind")
    deadline = time.time() + 2.0
    while time.time() < deadline and not got:
        time.sleep(0.02)
    assert got and got[0] == b"hello-pybind"
    autolink.shutdown()
```

（若 RawData 路径与类型匹配需 `message_type`/`proto_desc`，在 binder 内对齐旧 `PyReader`/`PyWriter` 逻辑。）

- [ ] **Step 2: 实现内部 Writer/Reader 辅助类（bind_node.cpp）**

核心行为（对齐旧 `PyWriter`/`PyReader`）：

- Writer：`CreateWriter<PyMessageWrap>(role_attr)`，`write(py::bytes)` → `PyMessageWrap`
- Reader：C++ 回调收到消息 → `gil_scoped_acquire` → 调用 `py::function(py::bytes(data))`
- `Node`：持有 `std::shared_ptr<autolink::Node>`；`create_writer(channel, msg_type: str, qos_depth=1)`；`create_reader(channel, callback, msg_type="RawData")`
- `register_message(file_desc_bytes)`：移植旧 `Node.register_message` / ProtobufFactory 注册
- `spin()`：Python 侧循环 `while not is_shutdown(): time.sleep(0.002)`（可在 Python 糖实现，不必进 C++）

最小 C++ 绑定骨架：

```cpp
class PyChannelWriter {
 public:
  bool Write(const std::string& bytes);
  // ...
};

class PyChannelReader {
 public:
  explicit PyChannelReader(..., py::function cb);
  // destructor acquires GIL if holding py::function
};

py::class_<autolink::Node, std::shared_ptr<autolink::Node>>(m, "Node")
  .def(py::init([](const std::string& name) {
      auto n = autolink::CreateNode(name);
      if (!n) throw py::runtime_error("CreateNode failed");
      return n;
    }))
  .def("create_writer", ...)
  .def("create_reader", ...)
  .def("register_message", ...);
```

- [ ] **Step 3: Python 糖（protobuf）**

```python
# autolink/_node_sugar.py 或包装 Node.create_writer
def create_writer(self, name, data_type, qos_depth=1):
    """data_type: protobuf class OR type name str."""
    if isinstance(data_type, type) and hasattr(data_type, "DESCRIPTOR"):
        type_name = data_type.DESCRIPTOR.full_name
        # register descriptor if needed (FileDescriptorProto serialize)
        w = self._create_writer(name, type_name, qos_depth)
        return _ProtobufWriter(w, data_type)
    return self._create_writer(name, str(data_type), qos_depth)
```

`_ProtobufWriter.write(msg)`：若有 `SerializeToString` 则序列化再调底层。

- [ ] **Step 4: 编译并跑 `test_pubsub.py` — Expected: PASS**

- [ ] **Step 5: Commit（若用户要求）**

---

### Task 4: Service / Client

**Files:**
- Extend: `bindings/bind_node.cpp`（或 `bind_service.cpp`）
- Create: `tests/test_service.py`
- Update examples later in Task 7

- [ ] **Step 1: 写 service 测试**

```python
def test_service_roundtrip():
    autolink.init("py_test_srv")
    node = autolink.Node("srv_node")

    def handler(req: bytes) -> bytes:
        return b"echo:" + req

    node.create_service("/py/echo", handler, req_type="RawData", res_type="RawData")
    client = node.create_client("/py/echo", req_type="RawData", res_type="RawData")
    time.sleep(0.3)
    resp = client.send_request(b"ping")
    assert resp == b"echo:ping"
    autolink.shutdown()
```

- [ ] **Step 2: 实现同步 Service 回调**

```cpp
auto f = [cb](const std::shared_ptr<const PyMessageWrap>& req,
              std::shared_ptr<PyMessageWrap>& res) {
  py::gil_scoped_acquire gil;
  try {
    py::object out = cb(py::bytes(req->data()));
    std::string bytes = py::cast<std::string>(out);
    res = std::make_shared<PyMessageWrap>(bytes, /*type*/);
  } catch (py::error_already_set& e) {
    AERROR << e.what();
    e.restore();
    PyErr_Clear();
    res = std::make_shared<PyMessageWrap>("", /*type*/);
  }
};
node->CreateService<PyMessageWrap, PyMessageWrap>(name, f);
```

Client：`send_request(bytes) -> bytes`，超时失败抛 `runtime_error`。

- [ ] **Step 3: 跑测试 — Expected: PASS；Commit（若要求）**

---

### Task 5: Parameter + Record + Utils

**Files:**
- Create: `bind_parameter.*`, `bind_record.*`, `bind_utils.*`
- Create: `tests/test_parameter.py`, `tests/test_record.py`
- Update `__init__.py`

- [ ] **Step 1: Parameter 测试**

```python
def test_parameter_native_types():
    autolink.init("py_test_param")
    node = autolink.Node("param_node")
    server = autolink.ParameterServer(node)
    client = autolink.ParameterClient(node, "param_node")
    p = autolink.Parameter("speed", 1.5)
    assert client.set_parameter(p)
    g = client.get_parameter("speed")
    assert abs(g.as_double() - 1.5) < 1e-6
    autolink.shutdown()
```

绑定：`Parameter(name, value)` 用 pybind11 overload（bool/int64/double/str）；`ParameterServer(node)` / `ParameterClient(node, server_node_name)`。

- [ ] **Step 2: Record 测试**

```python
def test_record_write_read(tmp_path):
    path = str(tmp_path / "t.record")
    w = autolink.RecordWriter()
    assert w.open(path)
    assert w.write_channel("ch", "type.A", "")
    assert w.write_message("ch", b"abc", 123)
    w.close()
    r = autolink.RecordReader(path)
    msg = r.read_message()
    assert not msg.end
    assert msg.data == b"abc" or msg.data == "abc"
    assert msg.channel_name == "ch"
```

绑定 `BagMessage` 为简单 struct（`timestamp`, `channel_name`, `data`, `data_type`, `end`）；Reader/Writer 方法 snake_case。

- [ ] **Step 3: Utils**

移植旧 `PyChannelUtils` / `PyNodeUtils` / `PyServiceUtils` 到 `bind_utils.cpp`，Python：

```python
autolink.ChannelUtils.get_channels(sleep_s=2)
autolink.NodeUtils.get_nodes(sleep_s=2)
autolink.ServiceUtils.get_services(sleep_s=2)
```

实现可先包一层调用现有 C++ discovery 辅助（从旧 `py_autolink.hpp` 拷贝逻辑进 binders，删对 Python.h 的依赖）。

- [ ] **Step 4: 跑相关测试 — Expected: PASS；Commit（若要求）**

---

### Task 6: 删除旧绑定并固定安装布局

**Files:**
- Delete: `autolink/python/src/` 下旧 `py_*.cpp/hpp`、`_loader.py`、扁平 `autolink.py`/`parameter.py`/…（在新包与测试全绿后）
- Delete: 旧 `py_autolink_test.cpp` / `py_record_test.cpp`（若仅测旧 C API）
- Ensure: `CMakeLists.txt` 无 `_*_wrapper` targets
- Update: 任何仍引用 `autolink_py3` / `python/internal` 的脚本

- [ ] **Step 1: `rg` 清残留**

```bash
rg -n "autolink_py3|_autolink_wrapper|_loader|python/internal" --glob '!thirdparty/**' --glob '!docs/superpowers/**'
```

Expected: 仅文档历史或本 plan/spec 提及；代码/examples 无引用。

- [ ] **Step 2: 删除旧文件并再全量编译 + `ctest -R autolink_python`**

- [ ] **Step 3: Commit（若要求）**

---

### Task 7: 更新 examples + 文档

**Files:**
- Modify: `examples/python/_bootstrap_autolink.py`、`py_talker.py`、`py_listener.py`、`py_service.py`、`py_client.py`、`py_parameter.py`、`py_record*.py`、`py_time.py`、`py_timer.py`
- Modify: `examples/python/README.md`
- Rewrite key sections: `docs/source/autolink_python_api_cn.md`
- Skip / note: `py_action_*.py`（本设计非目标；文件头标注 “Action Python binding not in pybind11 v1” 或暂时保留并跳过 CI）

- [ ] **Step 1: bootstrap 只加 `build/python`（或 install `python/`）**

```python
def setup_autolink_pythonpath() -> None:
    # ...
    _append_if_exists(Path(os.environ.get("AUTOLINK_BUILD_DIR", "build")) / "python")
    home = Path(os.environ.get("AUTOLINK_DISTRIBUTION_HOME", "/usr/local"))
    _append_if_exists(home / "python")
```

- [ ] **Step 2: 改 talker 为新 API**

```python
import autolink
from autolink.proto.unit_test_pb2 import Chatter  # 若 proto 路径如此；否则用现有 examples 路径

autolink.init("talker")
node = autolink.Node("node_name1")
writer = node.create_writer("channel/chatter", Chatter, qos_depth=6)
rate = autolink.Rate(1.0)
while not autolink.is_shutdown():
    msg = Chatter()
    msg.timestamp = autolink.Time.now().to_nsec()
    # ...
    writer.write(msg)
    rate.sleep()
```

同步改 listener/service/client/parameter/record/timer。

- [ ] **Step 3: 手动跑 examples（Docker/`SpaceHero` 或本机）**

```bash
export PYTHONPATH=build/python
python3 examples/python/py_talker.py &
python3 examples/python/py_listener.py
# Expected: listener 打印消息
```

- [ ] **Step 4: 更新 `autolink_python_api_cn.md`：背景改为 pybind11；删除 ctypes/`autolink_py3`；贴新 import 与 API**

- [ ] **Step 5: Commit（若要求）**

---

### Task 8: 验收对照 Spec §7

- [ ] **Step 1: 跑清单**

```bash
cmake --build build -j$(nproc) --target _core
ctest --test-dir build -R autolink_python --output-on-failure
PYTHONPATH=build/python python3 -c "import autolink; print(autolink.Node, autolink.Time, autolink.RecordWriter)"
rg -n "_autolink_wrapper|_loader|autolink_py3" autolink/python examples --glob '!**/__pycache__/**'
# Expected: no code hits
```

- [ ] **Step 2: 在 PR/总结中勾选 Spec 验收 5 条全部满足**

---

## Spec Coverage Check

| Spec 项 | Task |
|---|---|
| 单模块 `_core` + 包名 `autolink` | 1 |
| lifecycle 抛异常 init | 2 |
| Time/Timer | 2 |
| Node Pub/Sub + GIL | 3 |
| Service 同步回调 | 4 |
| Parameter 原生类型 | 5 |
| Record | 5 |
| Utils | 5 |
| 删除旧绑定 | 6 |
| examples + 文档 | 7 |
| 验收清单 | 8 |
| 非目标 Action | 7（标注跳过） |

## Notes for executors

- 工作目录默认：`autolink/` submodule 仓库根。
- Docker 构建勿与宿主机污染同一 `build`（沿用既有约定）。
- **不要自动 `git commit`/`push`**，除非用户明确要求；计划中的 Commit step 仅在用户授权时执行。
- 绑定实现时优先对照旧 `py_autolink.hpp` 中 RoleAttributes / proto_desc / RawData 分支，避免 discovery 类型不匹配。
