# Python 绑定迁移至 pybind11 设计

日期：2026-08-03  
状态：待审阅  
范围：`autolink/autolink/python`、`examples/python`、`docs/source/autolink_python_api_cn.md`

## 1. 背景与目标

现有 Python 绑定为手写 CPython C API（`Python.h` + `PyArg_ParseTuple`），经 `_loader` 加载 5 个 `_*_wrapper` 扩展，Python 侧再用 ctypes 注册回调。可维护性差，且与仓库已有 `thirdparty/pybind11` 未打通。

**目标**：用 `thirdparty/pybind11` 实现 **Python 调用 C++** 的单一扩展模块；允许对外 API 较大重设计；全量替换旧绑定并更新 examples/文档。

**非目标**：

- （已由后续迭代覆盖）Action 绑定 → 见 `docs/superpowers/specs/2026-08-03-python-protobuf-action-design.md`
- 与 ROS 2 `rclpy` / rmw 互操作
- 保留 `autolink_py3` / `_loader` / 多 wrapper 兼容层
- 将全部 protobuf C++ 类型绑到 Python

## 2. 已确认决策

| 项 | 选择 |
|---|---|
| API 兼容 | **C**：可较大重设计，examples 一并改 |
| 扩展切分 | **A**：单模块 `autolink._core` |
| 消息策略 | **C**：Channel/Service 以 bytes（+ protobuf 糖）为主；Parameter/Time 用原生类型 |
| 范围 | **A**：全量替换（lifecycle、Node、Pub/Sub、Service/Client、Parameter、Record、Time/Timer、Utils） |
| 实现路径 | **方案 2**：直接绑定 C++ 公共 API + 少量 Python 糖 |

## 3. 包与目录结构

### 3.1 导入形态

```python
import autolink

autolink.init("talker")
node = autolink.Node("talker")
writer = node.create_writer("channel/chatter", Chatter)
```

包名统一为 **`autolink`**（废弃安装名 `autolink_py3`）。

### 3.2 布局

```
autolink/python/
  CMakeLists.txt
  autolink/                    # 可安装 Python 包
    __init__.py                # 再导出公共 API
    node.py                    # protobuf 序列化糖（可选薄层）
    ...
  bindings/                    # pybind11 C++ 源码
    module.cpp                 # PYBIND11_MODULE(_core, m)
    bind_lifecycle.cpp/.hpp
    bind_node.cpp/.hpp
    bind_parameter.cpp/.hpp
    bind_record.cpp/.hpp
    bind_time.cpp/.hpp
    bind_timer.cpp/.hpp
    gil_utils.hpp
  tests/
    test_smoke_import.py
    test_time.py
    test_parameter.py
    ...
```

构建产物：`autolink/_core${EXT_SUFFIX}`（唯一扩展）。

### 3.3 删除清单

- 手写 `py_autolink.cpp` / `py_parameter.cpp` / `py_record.cpp` / `py_time.cpp` / `py_timer.cpp` 及对应旧 C API 胶水与 `py_*_test.cpp`（若仅覆盖旧 C API）
- `_loader.py`、ctypes `CFUNCTYPE` 回调路径
- 五个 `_*_wrapper` CMake target
- 安装前缀 `python/autolink_py3`
- 依赖旧布局的 bootstrap/`autolink_py3` import（examples 改为 `import autolink`）
- 旧扁平 `src/*.py` 包布局（迁移到 `python/autolink/` 包目录后删除）

旧 `PyWriter`/`PyReader`/… 若暂时复用，仅作为 `_core` **内部**实现，不作为公开 Python API；优先在 binders 内直接使用 `Node` + `PyMessageWrap`/`RawMessage`。

## 4. 绑定边界

| 层级 | 职责 |
|---|---|
| `autolink._core` | Init/OK/Shutdown、Node、Writer/Reader、Service/Client、Parameter*、Record*、Time/Duration/Rate、Timer、Channel/Node/Service Utils |
| Python 薄层 | protobuf `SerializeToString`/`ParseFromString`、友好构造、包级再导出 |

底层通道继续使用现有 `message::PyMessageWrap` / `RawMessage`，不引入全量 protobuf C++ ↔ Python 类型映射。

### 4.1 消息约定

- **Channel / Service**：`write(bytes | protobuf_message)`；回调默认收到 **bytes**；若创建时传入 protobuf 类，可由 Python 糖自动 Parse。
- **Parameter**：`bool` / `int` / `float` / `str` / `bytes` ↔ C++ `Parameter`。
- **Time / Timer**：秒（float）或纳秒（int），去掉 ctypes。

### 4.2 回调与 GIL

- Reader / Service / Timer：`py::function` 包装为 `std::function`；进入 Python 前 `py::gil_scoped_acquire`。
- 持有 `py::function` 的对象析构须在持 GIL 下进行。
- Service 改为**同步回调返回** `bytes`/protobuf，删除旧的 ctypes + request/response 双队列握手。
- Python 回调内未捕获异常：在绑定层捕获、打日志，避免穿过 C++ 回调线程导致进程崩溃；Service 异常时返回空响应并 `AERROR`。

### 4.3 生命周期 API

- 公开 Python API 统一 **snake_case**（如 `is_shutdown`、`wait_for_shutdown`、`create_writer`）。
- `autolink.init(name)`：**失败抛异常**，成功返回 `None`（与旧 `bool` 返回不兼容，符合决策 C）。
- `ok()` / `shutdown()` / `is_shutdown()` / `wait_for_shutdown()` 暴露为模块函数。
- `Node.spin()`：默认阻塞 spin（与现有 examples 一致）；若需后台线程，在实现计划中作为可选 API，本设计不强制。

## 5. 构建集成

- `AUTOLINK_BUILD_PYTHON=ON` 时：
  - `add_subdirectory(${AUTOLINK_ROOT}/thirdparty/pybind11 ...)`（或等价 `FetchContent` 禁用，**只用仓库 submodule**）
  - `pybind11_add_module(_core ...)`，`target_link_libraries(_core PRIVATE autolink)`
  - 输出到可被 `import autolink` 发现的包目录（build tree + install tree 一致）
- 依赖：`Python3::Interpreter`、`Python3::Development`、`pybind11::module`
- 不再 link 手写扩展到 `Python3::Python` 的旧 `MODULE` 五件套（由 pybind11 处理）

## 6. 错误处理

- C++ 可预期失败 → `py::value_error` / `py::runtime_error`，信息简短明确。
- 禁止对关键路径静默吞错（旧 C API 常 `Py_RETURN_FALSE`）。

## 7. 测试与验收

| 层级 | 内容 |
|---|---|
| smoke | `import autolink`；`init` + `Node` |
| 单测 | Time/Duration/Rate；Parameter round-trip；Record 小文件读写 |
| 集成 | 同进程 Writer/Reader（protobuf bytes）；Service/Client 一次往返 |
| examples | talker/listener、service/client、parameter、record、time/timer |

**验收清单**：

1. `-DAUTOLINK_BUILD_PYTHON=ON` 编译通过，仅产出 `_core` 扩展  
2. `ctest -R autolink_python`（或等价）全绿  
3. 上述 examples 可运行  
4. `docs/source/autolink_python_api_cn.md` 按新 API 更新  
5. 仓库无残留 `_autolink_wrapper` / `_loader` / `autolink_py3` 安装路径  

## 8. 实施顺序

1. CMake + 空 `_core` + `import autolink` smoke  
2. lifecycle + Time/Timer  
3. Node + Writer/Reader（含 GIL 回调）  
4. Service/Client  
5. Parameter + Record + Utils  
6. 更新 examples/文档，删除旧代码与测试胶水  

## 9. 风险与缓解

| 风险 | 缓解 |
|---|---|
| GIL / 回调死锁 | 统一 `gil_utils`；析构持 GIL；避免在持锁时回调 Python |
| protobuf 描述符注册 | 保留/移植现有 `register_message` / descriptor 注册路径到 `_core` |
| 包发现路径 | build/install 统一 `autolink/` 布局；examples bootstrap 只设 `PYTHONPATH` |
| 单模块体积/编译时间 | 按 `bind_*.cpp` 拆分翻译单元，并行编译 |

## 10. 参考

- 现有：`autolink/python/src/py_*.cpp`、`autolink.py`、`docs/source/autolink_python_api_cn.md`
- 依赖：`thirdparty/pybind11`（`.gitmodules`）
