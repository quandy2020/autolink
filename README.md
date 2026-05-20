# autolink

Autolink is a local-first communication framework focused on controllable deployment and low-latency runtime messaging.

## Project Status

- Transport mode is **INTRA + SHM only**.
- Topology discovery/registration is handled by the local backend.

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
cmake -S . -B build
cmake --build build -j8
sudo cmake --install build
```

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
- `docs/source/autolink_pod_message_cn.md`

## CMake Package Integration

This project installs CMake package files for external consumers:

- `find_package(Autolink REQUIRED)`
- `target_link_libraries(your_target PRIVATE Autolink::Autolink)`

In-tree reference demo:

- `examples/cpp-cmake/`

## Tools

When `AUTOLINK_BUILD_TOOLS=ON`, the following tool groups are built from `autolink/tools/`:

- `autolink_channel`
- `autolink_node`
- `autolink_service`
- `autolink_action`
- `autolink_monitor`
- `autolink_recorder`
- `autolink_launch`

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
