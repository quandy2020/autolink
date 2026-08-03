# autolink

Autolink is a local-first communication framework focused on controllable deployment and low-latency runtime messaging.

## Project Status

- Local: **INTRA + SHM**；跨机：**Fast DDS** / **Cyclone DDS**
  （`-DAUTOLINK_ENABLE_FASTDDS=ON` / `-DAUTOLINK_ENABLE_CYCLONEDDS=ON`）。
- Writer/Reader/**Service/Client**/Action/Parameter 均默认 **HYBRID**，选型对齐 ROS 2
 （`RMW_IMPLEMENTATION` / `AUTOLINK_AMW_IMPLEMENTATION`、`ROS_DOMAIN_ID` /
  `AUTOLINK_DOMAIN_ID`、`AUTOLINK_IP`）。
- 支持外部 `libamw_*.so` 插件 ABI（`AUTOLINK_AMW_PLUGIN_PATH`）；OpenDDS/Connext 可外挂。
- 同机 DIFF_HOST 模拟：`scripts/amw_sim_{dual_host,service,action,param}.sh`。
- 详见 `docs/source/amw/overview.md`。

## Core Features

- Publish/subscribe
- Service/client
- Action (goal/feedback/result)
- Data recording/playback
- CLI tools: channel/node/service/action/monitor/launch/recorder
- C++ and Python support
- Plugin/component support

## Repository Layout

- `autolink/`: core framework source
- `examples/`: runnable demos
- `docs/`: MkDocs documentation source
- `scripts/`: setup and maintenance scripts
- `docker/`: image build/run scripts

## Build Options

The top-level CMake options:

- `AUTOLINK_BUILD_TOOLS` (default `ON`)
- `AUTOLINK_BUILD_TEST` (default `ON`)
- `AUTOLINK_BUILD_EXAMPLES` (default `ON`)
- `AUTOLINK_BUILD_PYTHON` (default `ON`)
- `AUTOLINK_BUILD_DOCS` (default `ON`)

## Installation

### Option A: Docker (recommended)

Use scripts under `docker/`:

```bash
# x86_64 image
python3 docker/build_docker_x86_64.py -f dockerfile/autolink.x86_64.dockerfile

# run container
python3 docker/run.py
```

### Option B: Native (Ubuntu)

Install dependencies:

```bash
python3 scripts/install_dependency.py
```

Build and install:

```bash
cmake -S . -B build \
  -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
  -DAUTOLINK_ENABLE_FASTDDS=ON
cmake --build build -j8
sudo cmake --install build
```

跨机 / 同机 DIFF_HOST 验收（需真实 DDS，非 stub）：

```bash
./scripts/amw_preflight.sh build
./scripts/amw_sim_dual_host.sh amw_cyclonedds 0 10
```

更多环境变量与双机步骤见 `docs/source/amw/overview.md`。

## Quick Start

### 1) Build examples

```bash
cmake -S . -B build -DAUTOLINK_BUILD_EXAMPLES=ON
cmake --build build -j8
```

Example binaries are under `build/bin/examples/`.

### 2) Run talker/listener

Terminal 1:

```bash
./build/bin/examples/autolink_example_listener
```

Terminal 2:

```bash
./build/bin/examples/autolink_example_talker
```

### 3) Run POD examples

Single process:

```bash
./build/bin/examples/autolink_example_pod_talker_listener
```

Two processes:

```bash
# terminal 1
./build/bin/examples/autolink_example_pod_listener

# terminal 2
./build/bin/examples/autolink_example_pod_talker
```

For detailed POD steps, see:

- `examples/cpp/README.md`
- `docs/source/guide/pod_message.md`

## CMake Package Integration

This project installs CMake package files for external consumers:

- `find_package(Autolink REQUIRED)`
- `target_link_libraries(your_target PRIVATE Autolink::Autolink)`

In-tree reference demo:

- `examples/cpp-cmake/`

## Tools

When `AUTOLINK_BUILD_TOOLS=ON`, a unified CLI is built from `autolink/tools/`:

- `autolink channel` / `node` / `service` / `action` / `param` / `recorder` / `launch` / `monitor` / `doctor` / `completion`

Examples:

```bash
autolink --wait 3 channel list -v
autolink channel pub /chatter '{"content":"hi"}' --type autolink.proto.Chatter
autolink channel echo /chatter --once
autolink service call /add_two_ints '{"a":1,"b":2}' --type ...
autolink param list <node>
autolink launch list
autolink doctor
eval "$(autolink completion bash)"   # or: source scripts/completion/autolink.bash
autolink recorder play -f demo.record
```

CLI e2e smoke（需已编译 examples）:

```bash
./scripts/cli_e2e_smoke.sh build
```

## Documentation

Docs are built with MkDocs:

```bash
pip install -r docs/requirements.txt
cmake --build build --target docs
```

Or local preview:

```bash
cd docs
mkdocs serve
```

## Troubleshooting

- Ensure `AUTOLINK_PATH` points to a directory containing `conf/autolink.pb.conf`.
- If running multiple examples, avoid duplicated node names and stale processes.
- If external CMake cannot find package, set `CMAKE_PREFIX_PATH` to your install prefix.
