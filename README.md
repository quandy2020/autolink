# Autolink

本地优先的通信框架：同进程 INTRA、同机 SHM；拓扑发现默认本机文件总线。
跨机 Channel 为可选能力（`AUTOLINK_ENABLE_FASTDDS` + `diff_host: RTPS`）。
提供 Pub/Sub、Service、Action、Parameter、录回放，以及统一 CLI 与 C++/Python API。

## 构建

**Docker（推荐）**

```bash
python3 docker/build_docker_x86_64.py -f dockerfile/autolink.x86_64.dockerfile
python3 docker/run.py
```

**本机**

```bash
python3 scripts/install_dependencies.py
cmake -S . -B build
cmake --build build -j8
```

常用开关（默认均为 `ON`）：`AUTOLINK_BUILD_{TOOLS,EXAMPLES,PYTHON,TEST,DOCS}`。

## 运行前

```bash
export AUTOLINK_PATH=$PWD/autolink
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH
export PATH=$PWD/build/bin:$PATH
```

## 快速试用

**C++ Pub/Sub**

```bash
./build/bin/examples/autolink_example_listener   # 终端 1
./build/bin/examples/autolink_example_talker     # 终端 2
```

**Python Pub/Sub**

```bash
export PYTHONPATH=$PWD/build/python
python3 examples/python/py_listener.py          # 终端 1
python3 examples/python/py_talker.py            # 终端 2
```

更多示例：[`examples/cpp`](examples/cpp/)、[`examples/python`](examples/python/)。

## CLI

产物：`build/bin/autolink`。

```bash
autolink doctor
autolink channel list -v
autolink channel echo channel/chatter --once
autolink param list <node>
eval "$(autolink completion bash)"
```

完整命令与 `pub` / `call` / `send_goal`：[`docs/source/tools/cli.md`](docs/source/tools/cli.md)  
冒烟：`./scripts/cli_e2e_smoke.sh build`

## 文档

- [快速开始](docs/source/guide/quickstart.md)
- [C++ API](docs/source/api/cpp.md) · [Python API](docs/source/api/python.md)
- [CLI](docs/source/tools/cli.md) · [FAQ](docs/source/faq.md)

```bash
cd docs && pip install -r requirements.txt && mkdocs serve
```

## 接入你的工程

```cmake
find_package(Autolink REQUIRED)
target_link_libraries(your_target PRIVATE Autolink::Autolink)
```

参考：`examples/cpp-cmake/`。

## 排障

| 现象 | 处理 |
|---|---|
| 找不到配置 | 检查 `AUTOLINK_PATH`（需含 `conf/autolink.pb.conf`） |
| 动态库找不到 | 设置 `LD_LIBRARY_PATH=$PWD/build/lib` |
| CMake 找不到包 | 设置 `CMAKE_PREFIX_PATH` 到安装前缀 |
