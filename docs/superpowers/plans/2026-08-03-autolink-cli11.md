# Autolink 统一 CLI（CLI11）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `autolink/autolink/tools` 下 7 个独立 getopt 工具合并为单一 `autolink` 可执行文件，用 CLI11 实现 `autolink <一级> <二级>` 子命令树。

**Architecture:** CMake 目标名 `autolink_cli`（`OUTPUT_NAME=autolink`，避免与共享库目标 `autolink` 冲突）。`main.cpp` 注册一级子命令；各 `cli/cmd_*.cpp` 提供 `SetupXxx(CLI::App&)`；业务逻辑从旧 `main.cpp` 迁入 cmd 文件或继续调用 `autolink_recorder/`、`autolink_monitor/` 实现。

**Tech Stack:** C++17、CMake、CLI11 2.7.1（`thirdparty/CLI11`）、现有 autolink 共享库、ncurses（monitor）

**Spec:** `docs/superpowers/specs/2026-08-03-autolink-cli11-design.md`

**Commits:** 仅在用户明确要求时提交（不要自动 commit）。

---

## File map

| Path | Role |
|------|------|
| `autolink/tools/CMakeLists.txt` | 只构建/安装 `autolink_cli`（OUTPUT_NAME `autolink`） |
| `autolink/tools/main.cpp` | CLI::App 根入口 |
| `autolink/tools/cli/cmd_*.{hpp,cpp}` | 各一级命令注册与业务回调 |
| `autolink/tools/autolink_recorder/*` | 保留业务实现；删除 `main.cpp` 与独立 executable |
| `autolink/tools/autolink_monitor/*` | 保留 TUI；删除 `main.cpp` 与独立 executable |
| `autolink/tools/autolink_{channel,node,service,action,launch}/` | 迁移后整目录删除 |
| `README.md`、`docs/source/autolink_*.md` | 文档命令名更新 |

**目录说明：** 保留 `autolink_recorder/`、`autolink_monitor/` 目录名（现有 `#include "autolink/tools/autolink_recorder/..."` 无需批量改名）；与 spec 中的 `recorder/`、`monitor/` 逻辑等价。

**命令面：**

```text
autolink channel  {list,info,echo,hz,bw,type}
autolink node     {list,info}
autolink service  {list,info}
autolink action   {list,info,send_goal}
autolink recorder {info,play,record,split,recover}
autolink launch   {start,stop}
autolink monitor  [--channel NAME]
```

---

### Task 1: 脚手架 — CMake + main + 空子命令

**Files:**
- Create: `autolink/autolink/tools/main.cpp`
- Create: `autolink/autolink/tools/cli/cmd_channel.hpp`
- Create: `autolink/autolink/tools/cli/cmd_channel.cpp`
- Create: `autolink/autolink/tools/cli/cmd_node.hpp`
- Create: `autolink/autolink/tools/cli/cmd_node.cpp`
- Create: `autolink/autolink/tools/cli/cmd_service.hpp`
- Create: `autolink/autolink/tools/cli/cmd_service.cpp`
- Create: `autolink/autolink/tools/cli/cmd_action.hpp`
- Create: `autolink/autolink/tools/cli/cmd_action.cpp`
- Create: `autolink/autolink/tools/cli/cmd_recorder.hpp`
- Create: `autolink/autolink/tools/cli/cmd_recorder.cpp`
- Create: `autolink/autolink/tools/cli/cmd_launch.hpp`
- Create: `autolink/autolink/tools/cli/cmd_launch.cpp`
- Create: `autolink/autolink/tools/cli/cmd_monitor.hpp`
- Create: `autolink/autolink/tools/cli/cmd_monitor.cpp`
- Modify: `autolink/autolink/tools/CMakeLists.txt`

- [ ] **Step 1: 重写 `tools/CMakeLists.txt`**

先**保留**旧 `add_subdirectory(...)`，追加统一 CLI 目标，便于增量迁移；全部迁完后再删旧目标（Task 9）。

```cmake
# Copyright 2025 The Openbot Authors (duyongquan)
# ... license header ...

# --- Unified CLI (CLI11) ---
set(AUTOLINK_CLI_SOURCES
  main.cpp
  cli/cmd_channel.cpp
  cli/cmd_node.cpp
  cli/cmd_service.cpp
  cli/cmd_action.cpp
  cli/cmd_recorder.cpp
  cli/cmd_launch.cpp
  cli/cmd_monitor.cpp
)

# recorder / monitor 业务源（不含各自 main.cpp）—— Task 7/8 再链入；
# 脚手架阶段先不链 recorder/monitor 业务，仅空回调。

add_executable(autolink_cli ${AUTOLINK_CLI_SOURCES})
set_target_properties(autolink_cli PROPERTIES OUTPUT_NAME autolink)

target_include_directories(autolink_cli PRIVATE BEFORE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/thirdparty/CLI11/include
)

target_link_libraries(autolink_cli PRIVATE autolink)

find_package(Curses REQUIRED)
if(TARGET Curses::Curses)
  target_link_libraries(autolink_cli PRIVATE Curses::Curses)
else()
  target_include_directories(autolink_cli PRIVATE ${CURSES_INCLUDE_DIR})
  target_link_libraries(autolink_cli PRIVATE ${CURSES_LIBRARIES})
endif()

install(TARGETS autolink_cli
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Legacy tools (remove in Task 9)
add_subdirectory(autolink_action)
add_subdirectory(autolink_recorder)
add_subdirectory(autolink_monitor)
add_subdirectory(autolink_channel)
add_subdirectory(autolink_launch)
add_subdirectory(autolink_node)
add_subdirectory(autolink_service)
```

- [ ] **Step 2: 写 `main.cpp`**

```cpp
/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include <CLI/CLI.hpp>

#include "autolink/tools/cli/cmd_action.hpp"
#include "autolink/tools/cli/cmd_channel.hpp"
#include "autolink/tools/cli/cmd_launch.hpp"
#include "autolink/tools/cli/cmd_monitor.hpp"
#include "autolink/tools/cli/cmd_node.hpp"
#include "autolink/tools/cli/cmd_recorder.hpp"
#include "autolink/tools/cli/cmd_service.hpp"

int main(int argc, char** argv) {
    CLI::App app{"Autolink command line"};
    app.require_subcommand(1);

    autolink::tools::SetupChannel(app);
    autolink::tools::SetupNode(app);
    autolink::tools::SetupService(app);
    autolink::tools::SetupAction(app);
    autolink::tools::SetupRecorder(app);
    autolink::tools::SetupLaunch(app);
    autolink::tools::SetupMonitor(app);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
```

- [ ] **Step 3: 每个 `cmd_*.hpp` / `cmd_*.cpp` 空壳（以 node 为例，其余同构）**

`cli/cmd_node.hpp`:

```cpp
#pragma once

namespace CLI {
class App;
}

namespace autolink {
namespace tools {

void SetupNode(CLI::App& app);

}  // namespace tools
}  // namespace autolink
```

`cli/cmd_node.cpp`（脚手架：二级子命令存在但 callback 打印 stub）:

```cpp
#include "autolink/tools/cli/cmd_node.hpp"

#include <iostream>

#include <CLI/CLI.hpp>

namespace autolink {
namespace tools {

void SetupNode(CLI::App& app) {
    auto* node = app.add_subcommand("node", "Introspect Autolink nodes");
    node->require_subcommand(1);

    node->add_subcommand("list", "List active nodes")
        ->callback([]() { std::cerr << "TODO: node list\n"; });

    auto* info = node->add_subcommand("info", "Print node info");
    info->callback([]() { std::cerr << "TODO: node info\n"; });
}

}  // namespace tools
}  // namespace autolink
```

对其余六个文件照此模式：

| Setup 函数 | 一级名 | 二级（脚手架） |
|------------|--------|----------------|
| `SetupChannel` | `channel` | list, info, echo, hz, bw, type |
| `SetupService` | `service` | list, info |
| `SetupAction` | `action` | list, info, send_goal |
| `SetupRecorder` | `recorder` | info, play, record, split, recover |
| `SetupLaunch` | `launch` | start, stop |
| `SetupMonitor` | `monitor` | 无二级；仅空 callback |

`SetupMonitor` 特例（无 `require_subcommand`）:

```cpp
void SetupMonitor(CLI::App& app) {
    auto* mon = app.add_subcommand("monitor", "Interactive topology monitor");
    mon->callback([]() { std::cerr << "TODO: monitor\n"; });
}
```

- [ ] **Step 4: 配置并编译 `autolink_cli`**

在既有 build 目录（或重新 cmake）中：

```bash
cmake --build <build-dir> --target autolink_cli -j$(nproc)
```

Expected: 成功；产物名为 `autolink`（不是 `autolink_cli`）。

- [ ] **Step 5: 冒烟 help**

```bash
<path-to>/autolink -h
<path-to>/autolink node -h
<path-to>/autolink channel -h
```

Expected: 列出一级/二级子命令；无 crash。

---

### Task 2: 迁移 `node`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_node.cpp`
- Reference: `autolink/autolink/tools/autolink_node/main.cpp`（迁完后 Task 9 删除）

- [ ] **Step 1: 把旧 `main.cpp` 中的业务函数拷入 `cmd_node.cpp`**

保留（可放匿名 namespace）：`GetNodes`、`GetNodeAttr`、`CmdList`、`CmdInfo`（原逻辑）。删除 `PrintUsage` 与 `getopt` 循环。

- [ ] **Step 2: 用 CLI11 绑定**

```cpp
void SetupNode(CLI::App& app) {
    auto* node = app.add_subcommand("node", "Introspect Autolink nodes");
    node->require_subcommand(1);

    node->add_subcommand("list", "List active nodes")->callback([]() {
        autolink::Init("autolink");
        CmdList();
    });

    auto* info = node->add_subcommand("info", "Print node info");
    auto all = std::make_shared<bool>(false);
    auto names = std::make_shared<std::vector<std::string>>();
    info->add_flag("-a,--all", *all, "Show all nodes");
    info->add_option("nodes", *names, "Node name(s)");
    info->callback([all, names]() {
        autolink::Init("autolink");
        if (*all) {
            for (const auto& n : GetNodes()) {
                CmdInfo(n);
            }
            return;
        }
        if (names->empty()) {
            throw CLI::ValidationError("info", "need node name(s) or --all");
        }
        for (const auto& n : *names) {
            CmdInfo(n);
        }
    });
}
```

注意：从旧实现核对 `CmdInfo` 签名与 `--all` 行为，保持功能等价；`Init` 仅在 callback 内调用。

- [ ] **Step 3: 编译并对比**

```bash
cmake --build <build-dir> --target autolink_cli -j$(nproc)
<path-to>/autolink node -h
# 有运行中的 Autolink 图时:
<path-to>/autolink node list
# 对比旧工具（尚未删除时）:
<path-to>/autolink_node list
```

Expected: 输出一致（节点列表内容）。

---

### Task 3: 迁移 `service`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_service.cpp`
- Reference: `autolink/autolink/tools/autolink_service/main.cpp`

- [ ] **Step 1:** 拷贝 `GetServices` / `CmdList` / `CmdInfo` 等业务函数到 `cmd_service.cpp`。
- [ ] **Step 2:** `SetupService` 按 Task 2 同构绑定 `list` 与 `info`（含 `-a,--all` 与位置参数 `services`）。
- [ ] **Step 3:** 编译；`autolink service -h` / `autolink service list` 冒烟。

---

### Task 4: 迁移 `action`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_action.cpp`
- Reference: `autolink/autolink/tools/autolink_action/main.cpp`

- [ ] **Step 1:** 迁入 `GetActionNames`、`CmdList`、`CmdInfo`、`CmdSendGoal` 等。
- [ ] **Step 2:** CLI11 绑定：

```cpp
auto* sg = action->add_subcommand("send_goal", "Send an action goal");
auto name = std::make_shared<std::string>();
auto goal = std::make_shared<std::string>();
sg->add_option("action_name", *name, "Action name")->required();
sg->add_option("goal", *goal, "Goal payload (optional)");
sg->callback([name, goal]() {
    autolink::Init("autolink");
    CmdSendGoal(*name, *goal);
});
```

`list` / `info` 同 node/service 模式。

- [ ] **Step 3:** 编译；`autolink action -h` 冒烟。

---

### Task 5: 迁移 `channel`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_channel.cpp`（体量大：原 `main.cpp` ~827 行）
- Reference: `autolink/autolink/tools/autolink_channel/main.cpp`

- [ ] **Step 1:** 将全部 `Cmd*` 业务函数与辅助类型迁入 `cmd_channel.cpp`（或若单文件过大，可拆 `cli/channel_ops.cpp`，但默认单文件即可）。删除 getopt / `PrintUsage`。
- [ ] **Step 2:** 绑定二级命令（功能等价，flags 可整理）：

| 子命令 | 关键参数 / 选项 |
|--------|----------------|
| `list` | 无 |
| `type` | `channel` required |
| `info` | `channel`；`-a,--all` |
| `echo` | `channel` required |
| `bw` | `channel` required；`-w,--window`（原 `-w`） |
| `hz` | `channel` required；`-w,--window` |

示例 `hz`：

```cpp
auto* hz = channel->add_subcommand("hz", "Display publishing rate");
auto name = std::make_shared<std::string>();
auto window = std::make_shared<int>(/* 旧默认值，从原代码读取 */);
hz->add_option("channel", *name, "Channel name")->required();
hz->add_option("-w,--window", *window, "Window size");
hz->callback([name, window]() {
    autolink::Init("autolink");
    CmdHz(*name, *window);
});
```

- [ ] **Step 3:** 编译；`autolink channel -h`；有环境时 `list` / `type` 冒烟。

---

### Task 6: 迁移 `launch`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_launch.cpp`
- Reference: `autolink/autolink/tools/autolink_launch/main.cpp`（~533 行，含进程启停逻辑）

- [ ] **Step 1:** 将 `start`/`stop` 相关函数整体迁入 `cmd_launch.cpp`（保持匿名 namespace 辅助函数）。
- [ ] **Step 2:** CLI11：

```cpp
void SetupLaunch(CLI::App& app) {
    auto* launch = app.add_subcommand("launch", "Start/stop modules from launch files");
    launch->require_subcommand(1);

    auto* start = launch->add_subcommand("start", "Start modules");
    auto file = std::make_shared<std::string>();
    start->add_option("file", *file, "Launch file (optional, default autolink.launch)");
    start->callback([file]() { /* 调用原 Start 逻辑；通常不需 Topology Init */ CmdStart(*file); });

    auto* stop = launch->add_subcommand("stop", "Stop modules");
    auto stop_file = std::make_shared<std::string>();
    stop->add_option("file", *stop_file, "If set, stop only matching launch");
    stop->callback([stop_file]() { CmdStop(*stop_file); });
}
```

核对原 `main`：是否调用 `autolink::Init`；保持相同。

- [ ] **Step 3:** 编译；`autolink launch -h` 冒烟。

---

### Task 7: 迁移 `recorder`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_recorder.cpp`
- Modify: `autolink/autolink/tools/CMakeLists.txt`（链入 recorder 业务源，排除 `main.cpp`）
- Reference: `autolink/autolink/tools/autolink_recorder/main.cpp` + `recorder.hpp` / `player/` / `info.hpp` / `spliter.hpp` / `recoverer.hpp`

- [ ] **Step 1: CMake 收集 recorder 业务源**

```cmake
file(GLOB_RECURSE AUTOLINK_RECORDER_LIB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/autolink_recorder/*.cpp")
list(FILTER AUTOLINK_RECORDER_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_executable(autolink_cli
  ${AUTOLINK_CLI_SOURCES}
  ${AUTOLINK_RECORDER_LIB_SOURCES}
  # monitor sources added in Task 8
)
```

- [ ] **Step 2: 在 `cmd_recorder.cpp` 用 CLI11 重写原 getopt 分支**

为每个子命令绑定清晰长选项（功能等价于原 `RECORD_OPTIONS` / `PLAY_OPTIONS` 等）：

| 子命令 | 关键选项（从原 long_opts 映射） |
|--------|-------------------------------|
| `info` | `file` 位置参数（或 `--file`） |
| `record` | `--output`/`-o`，`--all`/`-a`，`--white-channel`/`-c`，`--black-channel`/`-k`，`--segment-interval`/`-i`，`--segment-size`/`-m` |
| `play` | `--files`/`-f`（多值），`--loop`/`-l`，`--rate`/`-r`，`--begin`/`-b`，`--end`/`-e`，`--start`/`-s`，`--delay`/`-d`，`--preload`/`-p`，white/black channel |
| `split` | 输入/输出文件 + 时间/大小切分选项（对照原 SPLIT 分支） |
| `recover` | 输入/输出文件（对照原 RECOVER 分支） |

每个 callback：先 `autolink::Init(...)`（与原各分支一致，含 `Init(argv[0], "autolink_recorder")` 的那支改为 `Init("autolink", "autolink_recorder")` 或等价），再构造 `Recorder`/`Player`/`Info`/`Spliter`/`Recoverer` 并 `Init`/`Proc`。

**不要**重写 `recorder.cpp` / `player/*` 业务；只换参数解析层。

- [ ] **Step 3:** 编译；`autolink recorder -h`；`autolink recorder info -h` 等。

---

### Task 8: 迁移 `monitor`

**Files:**
- Modify: `autolink/autolink/tools/cli/cmd_monitor.cpp`
- Modify: `autolink/autolink/tools/CMakeLists.txt`
- Reference: `autolink/autolink/tools/autolink_monitor/main.cpp`

- [ ] **Step 1: CMake 链入 monitor 源（排除 main）**

```cmake
file(GLOB AUTOLINK_MONITOR_LIB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/autolink_monitor/*.cpp")
list(FILTER AUTOLINK_MONITOR_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

# add to autolink_cli sources + already have Curses link
```

- [ ] **Step 2: `SetupMonitor`**

```cpp
void SetupMonitor(CLI::App& app) {
    auto* mon = app.add_subcommand("monitor", "Interactive topology monitor");
    auto channel = std::make_shared<std::string>();
    mon->add_option("-c,--channel", *channel, "Monitor only this channel");
    mon->callback([channel]() {
        // 迁入原 main：Init、FLAGS_minloglevel、topology listener、Screen::Run
        RunMonitor(*channel);  // channel 空 = 全部
    });
}
```

将原 `main` 中 Init 之后的逻辑提取为 `RunMonitor(const std::string& channel_filter)`。

- [ ] **Step 3:** 编译；`autolink monitor -h`。交互 UI 有显示时手动点开验证（可选）。

---

### Task 9: 删除旧二进制与子目录入口

**Files:**
- Modify: `autolink/autolink/tools/CMakeLists.txt`（去掉全部 `add_subdirectory`）
- Delete: `autolink/autolink/tools/autolink_channel/`（整个目录）
- Delete: `autolink/autolink/tools/autolink_node/`
- Delete: `autolink/autolink/tools/autolink_service/`
- Delete: `autolink/autolink/tools/autolink_action/`
- Delete: `autolink/autolink/tools/autolink_launch/`
- Delete: `autolink/autolink/tools/autolink_recorder/main.cpp`
- Delete: `autolink/autolink/tools/autolink_recorder/CMakeLists.txt`
- Delete: `autolink/autolink/tools/autolink_monitor/main.cpp`
- Delete: `autolink/autolink/tools/autolink_monitor/CMakeLists.txt`

- [ ] **Step 1:** 最终 `tools/CMakeLists.txt` 仅含 `autolink_cli`（含 recorder/monitor 业务源 + Curses），无 `add_subdirectory`。
- [ ] **Step 2:** 删除上表路径。
- [ ] **Step 3:** 全量重新 cmake + build：

```bash
cmake --build <build-dir> --target autolink_cli -j$(nproc)
```

Expected: 成功；`build` 的 bin 中**不再**有 `autolink_channel` 等旧目标（清 stale 产物可用 `ninja -t clean` / 删对应文件）。

- [ ] **Step 4: 最终冒烟**

```bash
autolink -h
autolink channel -h
autolink node -h
autolink service -h
autolink action -h
autolink recorder -h
autolink launch -h
autolink monitor -h
```

Expected: 全部打印帮助，退出码 0（CLI11 help 通常为 0）。

```bash
autolink
autolink channel
```

Expected: 缺子命令 → 非 0 + help。

---

### Task 10: 更新文档与 README

**Files:**
- Modify: `autolink/README.md`（工具列表）
- Modify: `autolink/docs/source/autolink_developer_tools.md`
- Modify: `autolink/docs/source/autolink_quick_start_cn.md`
- Modify: `autolink/docs/source/autolink_api_for_developers.md`（launch 相关）
- Grep 全库：`autolink_(channel|node|service|action|recorder|monitor|launch)`，凡用户文档/示例命令一律改为新形式

替换对照：

| 旧 | 新 |
|----|----|
| `autolink_channel list` | `autolink channel list` |
| `autolink_node info` | `autolink node info` |
| `autolink_service list` | `autolink service list` |
| `autolink_action list` | `autolink action list` |
| `autolink_recorder play` | `autolink recorder play` |
| `autolink_monitor` | `autolink monitor` |
| `autolink_launch start` | `autolink launch start` |

- [ ] **Step 1:** 按上表改文档。
- [ ] **Step 2:** 再 grep 确认 `docs/`、`README.md`、`examples/` 无残留旧命令（`#include` 路径 `autolink/tools/autolink_recorder/` 可保留）。

---

## Spec coverage（自检）

| Spec 项 | Task |
|---------|------|
| 单一 `autolink` 二进制 / 删旧工具 | 1, 9 |
| 命令面 7 个一级 | 2–8 |
| CLI11 + SetupXxx | 1–8 |
| `OUTPUT_NAME` 避开库名冲突 | 1 |
| flags 可整理、功能等价 | 2–8 |
| Init 仅在执行路径 | 2–8 |
| recorder/monitor 业务不重写 | 7–8 |
| 文档更新 | 10 |
| 非目标（补全/parameter/别名） | 不实现 |

## Placeholder / 一致性自检

- CMake 目标统一为 `autolink_cli`，产物名 `autolink`
- Setup 函数签名统一：`void SetupXxx(CLI::App& app)`，命名空间 `autolink::tools`
- 无 TBD；大文件迁移以「迁函数 + CLI11 绑定」步骤描述，避免在计划中粘贴 800 行业务代码
