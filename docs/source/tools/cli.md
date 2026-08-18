# CLI 与录制工具

统一入口：`autolink`（`AUTOLINK_BUILD_TOOLS=ON`，产物通常为 `build/bin/autolink`）。构建与示例联调见 [快速开始](../guide/quickstart.md)。

```bash
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH
export PATH=$PWD/build/bin:$PATH
export AUTOLINK_PATH=$PWD/autolink     # 含 conf/autolink.pb.conf 的树根
```

全局选项（写在子命令前）：

| 选项 | 默认 | 说明 |
|---|---|---|
| `--wait N` | `2` | 拓扑发现等待秒数；`0` 表示不等待 |

```bash
autolink -h
autolink <subcommand> -h
autolink --wait 3 channel list -v
```

JSON 发布 / Service / Action 常需 protobuf 类型与 descriptor set：

```bash
protoc -I examples/cpp/proto --include_imports \
  --descriptor_set_out=/tmp/examples.pb examples.proto
```

端到端冒烟：`./scripts/cli_e2e_smoke.sh build`。

---

## channel

```text
autolink [--wait N] channel <list|type|info|echo|pub|bw|hz> ...
```

| 子命令 | 用法 |
|---|---|
| `list` | `autolink channel list [-v\|--verbose]` |
| `type` | `autolink channel type <channel>` |
| `info` | `autolink channel info [-a\|--all] [channel]` |
| `echo` | `autolink channel echo <channel> [--once] [-n\|--times N]`（均省略则持续打印） |
| `pub` | `autolink channel pub <channel> <json> [--type T] [--descriptor-set FILE] [--rate HZ] [--times N]` |
| `bw` | `autolink channel bw <channel> [-w\|--window SIZE]`（默认 window=100） |
| `hz` | `autolink channel hz <channel> [-w\|--window SIZE]`（默认 window=50000） |

示例：

```bash
autolink --wait 3 channel list -v
autolink channel type channel/chatter
autolink channel info channel/chatter
autolink channel info -a
autolink channel echo channel/chatter --once
autolink channel echo channel/chatter -n 5
# content 为 bytes 时 JSON 用 base64，如 "hi" → aGk=
autolink channel pub channel/chatter \
  '{"seq":1,"content":"aGk="}' \
  --type autolink.examples.Chatter \
  --descriptor-set /tmp/examples.pb \
  --rate 1 --times 3
autolink channel bw channel/chatter -w 50
autolink channel hz channel/chatter
```

---

## node

```text
autolink [--wait N] node <list|info> ...
```

| 子命令 | 用法 |
|---|---|
| `list` | `autolink node list` |
| `info` | `autolink node info [-a\|--all] [node ...]` |

```bash
autolink node list
autolink node info talker
autolink node info -a
```

---

## service

```text
autolink [--wait N] service <list|info|call> ...
```

| 子命令 | 用法 |
|---|---|
| `list` | `autolink service list` |
| `info` | `autolink service info [-a\|--all] [service ...]` |
| `call` | `autolink service call <service> [json] [--type T] [--descriptor-set FILE] [--timeout SEC]` |

`json` 默认可省略（`{}`）。`--timeout` 默认 `5`（秒，内部至少按 1 秒计）。

```bash
autolink service list
autolink service info test_server
autolink service call test_server '{"msg_id":7}' \
  --type autolink.examples.Driver \
  --descriptor-set /tmp/examples.pb \
  --timeout 8
```

---

## action

```text
autolink [--wait N] action <list|info|send_goal> ...
```

| 子命令 | 用法 |
|---|---|
| `list` | `autolink action list` |
| `info` | `autolink action info <action_name>` |
| `send_goal` | `autolink action send_goal <action_name> [json] [--type T] [--descriptor-set FILE]` |

Action 名不含后缀；底层会走 `…/send_goal` 等服务。Goal 类型示例：`autolink.examples.SimpleMessageAction.Goal`。

```bash
autolink action list
autolink action info examples/simple_message_action
autolink action send_goal examples/simple_message_action \
  '{"text":"hello"}' \
  --type autolink.examples.SimpleMessageAction.Goal \
  --descriptor-set /tmp/examples.pb
```

---

## param

```text
autolink [--wait N] param <list|get|set> ...
```

参数挂在 **ParameterServer 所在 Node 名** 上（不是任意业务 Node）。

| 子命令 | 用法 |
|---|---|
| `list` | `autolink param list <node>` |
| `get` | `autolink param get <node> <name>` |
| `set` | `autolink param set <node> <name> <value>` |

```bash
autolink param list parameter
autolink param get parameter int
autolink param set parameter int 100
```

---

## recorder

```text
autolink recorder <info|record|play|split|recover> ...
```

### info

```bash
autolink recorder info <file>
```

### record

须 `-a` 或 `-c` 之一。

| 选项 | 说明 |
|---|---|
| `-o, --output` | 输出文件；默认 `PWD/<时间戳>.record` |
| `-a, --all` | 录全部通道 |
| `-c, --white-channel` | 白名单（可多次） |
| `-k, --black-channel` | 黑名单 |
| `-i, --segment-interval` | 分段间隔（秒） |
| `-m, --segment-size` | 分段大小（MB） |

```bash
autolink recorder record -a -o /tmp/demo.record
autolink recorder record -c channel/chatter -o /tmp/chatter.record
```

### play

| 选项 | 说明 |
|---|---|
| `-f, --files` | 输入 record（必填，可多个） |
| `-a, --all` | 播放全部通道（未指定 `-c` 时等同全部） |
| `-l, --loop` | 循环 |
| `-r, --rate` | 倍速（默认 1.0） |
| `-b, --begin` / `-e, --end` | 起止时间字符串 |
| `-s, --start` | 起始偏移（秒） |
| `-d, --delay` | 延迟启动（秒） |
| `-p, --preload` | 预加载（秒，默认 3） |
| `-c, --white-channel` / `-k, --black-channel` | 通道过滤 |

```bash
autolink recorder play -f /tmp/demo.record
autolink recorder play -f /tmp/demo.record -l -r 2.0 -c channel/chatter
```

### split / recover

```bash
autolink recorder split -f in.record -o out.record \
  [-c white...] [-k black...] [-b begin] [-e end]
autolink recorder recover -f in.record [-o out.record]
```

---

## launch

```text
autolink launch <start|stop|list> [file]
```

| 子命令 | 用法 |
|---|---|
| `start` | `autolink launch start [file]` — 无 file 时按默认/配置拉起 |
| `stop` | `autolink launch stop [file]` — 有 file 时只停匹配项 |
| `list` | `autolink launch list` — 列出运行中的 launch/mainboard |

```bash
autolink launch start examples/cpp/common_component_example/common.launch
autolink launch list
autolink launch stop
```

---

## monitor

交互式拓扑/通道监视（ncurses）。

```bash
autolink monitor
autolink monitor -c channel/chatter
```

快捷键（`h` 查看帮助）：

| 键 | 作用 |
|---|---|
| `q` / `Esc` | 退出 |
| `Backspace` / `a` | 返回上级 |
| `↑↓←→` / `w s a d` / `Enter` | 浏览与进入 |
| `PgUp`/`PgDn` | 翻页 |
| `f` | 帧率 |
| `t` | 消息类型 |
| `Space` | 开关通道消息 |
| `i` | Reader / Writer |
| `b` | DebugString |
| `n` / `m` | 重复字段下一项 / 上一项 |
| `,` | 开关「显示全部重复项」 |

---

## doctor

检查环境变量、`--wait` 后拓扑规模。

```bash
autolink doctor
autolink --wait 0 doctor
```

确认 Autolink 初始化成功，并查看当前 channels / nodes 数量。

---

## completion

```bash
eval "$(autolink completion bash)"
# 或: source scripts/completion/autolink.bash

eval "$(autolink completion zsh)"
# 或: fpath+=(.../scripts/completion); autoload -U compinit; compinit
```

---

## 命令速查

```text
autolink [--wait N]
  channel  list|type|info|echo|pub|bw|hz
  node     list|info
  service  list|info|call
  action   list|info|send_goal
  param    list|get|set
  recorder info|record|play|split|recover
  launch   start|stop|list
  monitor  [-c channel]
  doctor
  completion bash|zsh
```
