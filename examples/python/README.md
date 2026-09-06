# Autolink Python Examples

本目录示例基于 **pybind11** 绑定：`import autolink`（包位于 `build/python` 或 install 的 `python/`）。

## Prerequisites

```bash
# 构建（容器 SpaceHero 示例）
cmake -S . -B build -DAUTOLINK_BUILD_PYTHON=ON
cmake --build build -j1 --target autolink _core autolink_python_pb2
pip3 install -r autolink/python/requirements.txt

export PYTHONPATH=$PWD/build/python
# CycloneDDS 若来自 ROS Humble，需优先其 iceoryx：
export LD_LIBRARY_PATH=/opt/ros/humble/lib/x86_64-linux-gnu:$PWD/build/lib:$LD_LIBRARY_PATH
# 若使用 install 前缀：
# source <prefix>/bin/setup_autolink_python.sh
```

`examples/python/_bootstrap_autolink.py` 会尽量自动把 `build/python` 加入 `sys.path`。

## Run Examples

### Pub/Sub（protobuf `Chatter`）

```bash
# terminal 1
python3 py_listener.py

# terminal 2
python3 py_talker.py
```

### Service/Client（protobuf `ChatterBenchmark`）

```bash
# terminal 1
python3 py_service.py

# terminal 2
python3 py_client.py
```

### Action（protobuf `SimpleMessageAction`，对齐 C++ action_listener/talker）

```bash
# terminal 1
python3 py_action_server.py

# terminal 2
python3 py_action_client.py
```

可与 C++ `action_listener` / `action_talker` 互通（同 action 名 `examples/simple_message_action`）。

### Parameter

```bash
python3 py_parameter.py
```

### Time/Timer

```bash
python3 py_time.py
python3 py_timer.py
```

### Record

```bash
python3 py_record.py
python3 py_record_channel_info.py /tmp/test_writer.record
python3 py_record_trans.py /tmp/test_writer.record
```

## Notes

- protobuf 依赖：`pip install -r autolink/python/requirements.txt`；`*_pb2.py` 由 CMake 目标 `autolink_python_pb2` 生成到 `build/python/`。
- 默认同进程 INTRA、同机 SHM；拓扑发现为本机文件总线。跨机 Channel 为可选：编译打开 `AUTOLINK_ENABLE_FASTDDS`，配置 `diff_host: RTPS`，双方设置可达 `AUTOLINK_IP` 与相同 `AUTOLINK_DOMAIN_ID`（默认 80）。
