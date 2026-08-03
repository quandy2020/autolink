# Autolink 统一 CLI（CLI11）设计

日期：2026-08-03  
状态：待审阅  
范围：`autolink/autolink/tools`、`autolink/thirdparty/CLI11`、相关 README/脚本引用

## 1. 背景与目标

现有命令行工具为 7 个独立可执行文件（`autolink_channel` / `node` / `service` / `action` / `recorder` / `monitor` / `launch`），各自用 `getopt`/`手写解析`。用法分散，与 ros2 式 `cli <verb> <subcommand>` 不一致，且仓库已有 `thirdparty/CLI11` 未接入。

**目标**：用 CLI11 实现单一入口 `autolink`，形如：

```text
autolink <一级命令> <二级命令> [options] [args]
```

功能与现有工具等价；删除全部旧独立二进制。

**非目标**：

- parameter CLI
- bash/zsh 补全
- 插件式 / dlopen 扩展
- Python 包装入口
- 与旧二进制名的 shell 兼容别名

## 2. 已确认决策

| 项 | 选择 |
|---|---|
| 交付形态 | **A**：只保留单一 `autolink`，删除旧 `autolink_*` 二进制 |
| 一级命名 | **A**：保留 Autolink 术语（`channel` 非 `topic`） |
| 录包一级名 | **A**：`recorder`（避免 `autolink record record`） |
| flags | **A**：功能等价即可，允许用 CLI11 习惯整理短/长选项 |
| 实现路径 | **方案 1**：单二进制 + 按域源文件注册 |

## 3. 命令面

| 一级 | 二级 | 说明 |
|------|------|------|
| `channel` | `list` / `info` / `echo` / `hz` / `bw` / `type` | 通道自省 |
| `node` | `list` / `info` | 节点自省 |
| `service` | `list` / `info` | 服务自省 |
| `action` | `list` / `info` / `send_goal` | Action 自省与发 goal |
| `recorder` | `info` / `play` / `record` / `split` / `recover` | 录回放 |
| `launch` | `start` / `stop` | 启停模块 |
| `monitor` | （无二级；选项挂在一级） | ncurses 拓扑监视 |

示例：

```text
autolink channel list
autolink channel echo /chatter
autolink node info --all
autolink recorder play --file demo.record
autolink launch start my.launch
autolink monitor -c /chatter
```

## 4. 目录与构建

```text
autolink/autolink/tools/
├── CMakeLists.txt          # 只产一个 autolink 目标
├── main.cpp                # CLI::App 根入口
├── cli/
│   ├── cmd_channel.{hpp,cpp}
│   ├── cmd_node.{hpp,cpp}
│   ├── cmd_service.{hpp,cpp}
│   ├── cmd_action.{hpp,cpp}
│   ├── cmd_recorder.{hpp,cpp}
│   ├── cmd_launch.{hpp,cpp}
│   └── cmd_monitor.{hpp,cpp}
├── recorder/               # 保留现有业务实现（player/info/...）
└── monitor/                # 保留现有 TUI 实现
```

要点：

- 删除各子目录独立 `add_executable(autolink_*)`；`install` 只装 `autolink`
- CLI11 header-only：`target_include_directories` 指向 `thirdparty/CLI11/include`
- 每个域暴露 `void SetupXxx(CLI::App& app)`，在 `main.cpp` 中依次注册
- `channel` / `node` / `service` / `action` / `launch`：旧 `main.cpp` 逻辑迁入对应 `cmd_*.cpp` 后删除旧入口
- `recorder/`、`monitor/`：去掉独立 `main`，业务源文件由 `cmd_*` 调用；TUI / player 逻辑不重写

## 5. CLI11 约定

- 根：`CLI::App{"Autolink command line"}`，`require_subcommand(1)`
- 一级（除 `monitor`）与二级：`require_subcommand` 按需设置，使缺省子命令时打印该层 help
- 位置参数：`add_option("name", ...)->required()`
- 选项：优先清晰长名（如 `--window`），短名按需（如 `-w`）
- 统一 `--help` / `-h`（CLI11 内置）；删除手写 `PrintUsage`
- `autolink::Init` 放在子命令 callback / 实际执行路径中，避免仅查询 help 时初始化运行时
- flags 整理原则：功能等价；去掉歧义短选项；布尔用 `--all` 等长名；多值用 CLI11 多参数；不要求与旧 getopt 字母一一对应

## 6. 组件职责

| 单元 | 职责 | 依赖 |
|------|------|------|
| `main.cpp` | 解析 argv、注册一级、返回退出码 | CLI11、各 `SetupXxx` |
| `cli/cmd_*.cpp` | 定义子命令树、绑定参数、调用业务函数 | CLI11、autolink 库、recorder/monitor 实现 |
| `recorder/*` | record/play/info/split/recover 实现 | autolink 库 |
| `monitor/*` | ncurses 拓扑 UI | autolink 库、ncurses |

数据流：`argv` → CLI11 解析 → 子命令 callback → 业务函数 → stdout/stderr → exit code。

## 7. 错误处理

- CLI11 解析失败：非 0 退出（库默认行为）
- 业务失败（资源不存在、文件错误等）：`stderr` 说明 + 非 0；成功为 0
- 仅输入 `autolink` 或 `autolink channel`（缺二级）时：打印该层 help，非 0

## 8. 迁移与文档

- 同步更新 `README`、examples、脚本中对 `autolink_*` 的引用为 `autolink <一级> ...`
- 本阶段不做兼容包装脚本

## 9. 验证

- 构建产出且仅产出 `autolink` 工具二进制（tools 目录下）
- 冒烟：`autolink -h`、各一级 `-h`
- 有运行环境时：`channel list` / `node list` / `recorder info` 等核心路径
- `monitor` 仍依赖 ncurses，构建条件与现网一致

## 10. 风险与缓解

| 风险 | 缓解 |
|------|------|
| recorder/monitor 与 CLI 层耦合紧 | cmd 层只做参数绑定，调用现有类/函数，避免大改业务 |
| flags 整理导致脚本失效 | 文档列出新旧对照；本轮明确不保证旧短选项兼容 |
| 单二进制链接体积变大 | 可接受；后续若需再拆静态库 |
