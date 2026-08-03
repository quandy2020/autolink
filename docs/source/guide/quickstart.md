# 快速开始

用 CMake 构建 Autolink，并跑通 Pub/Sub、Service、Action、Parameter 等最小用例。推荐 Docker（`SpaceHero`）或已装依赖的 Ubuntu。

## 1. 环境

在仓库根目录（含顶层 `CMakeLists.txt`）：

```bash
export AUTOLINK_PATH=$PWD/autolink         # 须含 conf/autolink.pb.conf
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH
export PATH=$PWD/build/bin:$PATH
# Cyclone 来自 ROS Humble 时：
# export LD_LIBRARY_PATH=/opt/ros/humble/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
# 跨机/与 AMW 示例联调时钉死实现，例如：
# export AUTOLINK_AMW_IMPLEMENTATION=amw_cyclonedds
```

调试日志可加：`export GLOG_logtostderr=1`。

## 2. 构建

```bash
cmake -S . -B build \
  -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
  -DAUTOLINK_ENABLE_FASTDDS=ON \
  -DAUTOLINK_BUILD_EXAMPLES=ON \
  -DAUTOLINK_BUILD_TOOLS=ON \
  -DAUTOLINK_BUILD_PYTHON=ON
cmake --build build -j8
```

| 开关 | 默认 | 作用 |
|---|---|---|
| `AUTOLINK_BUILD_EXAMPLES` | ON | `build/bin/examples/` |
| `AUTOLINK_BUILD_TOOLS` | ON | `build/bin/autolink` CLI |
| `AUTOLINK_BUILD_PYTHON` | ON | `build/python` |
| `AUTOLINK_BUILD_TEST` | ON | `ctest` |
| `AUTOLINK_BUILD_DOCS` | ON | MkDocs 目标 `docs` |
| `AUTOLINK_ENABLE_CYCLONEDDS` / `FASTDDS` | OFF | 真实 DDS（跨机必需） |

示例二进制：`build/bin/examples/autolink_example_*`。

## 3. Pub / Sub

终端 1：

```bash
./build/bin/examples/autolink_example_listener
```

终端 2：

```bash
./build/bin/examples/autolink_example_talker
```

期望 listener 打印 `Chatter`（`seq` / `content`）。单进程：`autolink_example_talker_listener`。

CLI 旁路：

```bash
autolink --wait 3 channel list -v
autolink channel echo channel/chatter --once
```

## 4. Service / Client

单进程自洽演示（同进程内起 server + client）：

```bash
./build/bin/examples/autolink_example_service
```

跨进程 / 跨机用 AMW 对：`amw_service` + `amw_client`（见 [AMW](../amw/overview.md)）。

## 5. Action

先起 server，再 client（action 名：`examples/simple_message_action`）：

```bash
# 终端 1
./build/bin/examples/autolink_example_action_listener
# 终端 2
./build/bin/examples/autolink_example_action_talker
```

应看到 feedback `index` 与最终 `SUCCEEDED`。可与 Python `py_action_*` 互通。

## 6. Parameter

```bash
./build/bin/examples/autolink_example_paramserver
```

同机 DIFF_HOST：`./scripts/amw_sim_param.sh amw_cyclonedds 0 15`。  
CLI：`autolink param list|get|set <ParameterServer 所在 Node 名> ...`（见 [CLI](../tools/cli.md)）。

## 7. Record

```bash
./build/bin/examples/autolink_example_record   # 写/读 test.record
# 或 CLI
autolink recorder record -c channel/chatter -o /tmp/demo.record
autolink recorder play -f /tmp/demo.record
```

## 8. POD（无 protobuf）

见 [POD 消息](pod_message.md)。单进程：`autolink_example_pod_talker_listener`。

## 9. Component / mainboard

实现 `Init` / `Proc`，打成 `.so`，由 DAG 描述通道，经 `autolink_mainboard` 加载。

```bash
cmake --build build -j8 --target common_component_example
# .so → build/lib/examples/common_component_example/libcommon_component_example.so
```

样例：`examples/cpp/common_component_example/`（`common.dag` / `common.launch`）。  
`module_library` / `dag_conf` 须能被路径解析（绝对路径，或相对当前目录，或 `AUTOLINK_LIB_PATH` / `AUTOLINK_DAG_PATH`）：

```bash
./build/bin/autolink_mainboard -d /abs/path/to/common.dag
# 或（launch 文件解析通过后）
autolink launch start /abs/path/to/common.launch
autolink launch list
autolink launch stop
```

仓库内 `common.dag` 的路径偏安装树布局，构建树联调时请改成实际 `.so` 路径。

## 10. Python

```bash
pip3 install -r autolink/python/requirements.txt
export PYTHONPATH=$PWD/build/python
# 终端 1 / 2
cd examples/python
python3 py_listener.py
python3 py_talker.py
```

更多：`py_service`/`py_client`、`py_action_*`、`py_parameter`、`py_record*`。详见 [Python API](../api/python.md)。

## 11. 示例一览

| 能力 | C++ 二进制（`build/bin/examples/`） | Python |
|---|---|---|
| Pub/Sub | `*_talker` / `*_listener` / `*_talker_listener` | `py_talker` / `py_listener` |
| Service | `*_service`；跨机 `*_amw_service`/`*_amw_client` | `py_service` / `py_client` |
| Action | `*_action_listener` / `*_action_talker` | `py_action_server` / `py_action_client` |
| Parameter | `*_paramserver`；跨机 `*_amw_param_*` | `py_parameter` |
| Record | `*_record` | `py_record*` |
| POD | `*_pod_*` | — |
| AMW Pub/Sub | `*_amw_talker` / `*_amw_listener` | — |

冒烟：`./scripts/cli_e2e_smoke.sh build`；同机 DDS：`./scripts/amw_preflight.sh build`。

## 12. 下一步

| 需求 | 文档 |
|---|---|
| 接口写法 | [C++ API](../api/cpp.md) · [Python API](../api/python.md) |
| 跨机 DDS | [AMW](../amw/overview.md) |
| 命令行 | [CLI](../tools/cli.md) |
| 调度绑核 | [调度器](scheduler.md) |
| 排障 | [FAQ](../faq.md) |
