# 常见问题

## Autolink 是什么？

面向自动驾驶与机器人场景的本地优先通信运行时：Pub/Sub、Service、Action、Parameter、录制回放。

## 同机通信走哪条路径？

默认 **HYBRID**：同进程 INTRA，同机异进程 SHM。未匹配到 peer 时不发送，等待 discovery。

## 运行示例收不到消息？

1. 两边同一 `AUTOLINK_PATH`（含 `conf/autolink.pb.conf`）  
2. `LD_LIBRARY_PATH` 含 `build/lib`  
3. 无残留同名 Node 进程：`pkill -f autolink_example` 后重试  
4. 稍等 discovery，或 `autolink --wait 3 channel list`  

## Python `import autolink` 报 iceoryx 符号错误？

把 ROS Humble 库路径放在 `LD_LIBRARY_PATH` **最前**：

```bash
export LD_LIBRARY_PATH=/opt/ros/humble/lib/x86_64-linux-gnu:$PWD/build/lib:$LD_LIBRARY_PATH
export PYTHONPATH=$PWD/build/python
```

## Service / Action 一直 not ready？

- 对端进程已起且通道名 / action 名一致  
- Client 侧加长等待；CLI：`autolink --wait 3 service list` / `action list`  
- Action 先起 server（`action_listener` / `py_action_server`）再 client  

## Parameter CLI 找不到参数？

`autolink param *` 的 Node 名必须是 **ParameterServer 所在 Node**，不是任意业务 Node。  
示例：`paramserver` 用 Node `parameter`。

## Component / mainboard 加载失败？

- `.so` 与 `.dag` 路径可解析（绝对路径，或 `AUTOLINK_LIB_PATH` / `AUTOLINK_DAG_PATH`）  
- `class_name` 与 `AUTOLINK_REGISTER_COMPONENT` 一致  
- 使用 `./build/bin/autolink_mainboard -d <dag>`  

见 [快速开始 §9](guide/quickstart.md)。

## 找不到配置 / CMake 包？

- `AUTOLINK_PATH` → 含 `conf/autolink.pb.conf`  
- 外部工程：`CMAKE_PREFIX_PATH` 指向 install 前缀；`find_package(Autolink)`  
- 文档预览：`cd docs && mkdocs serve`  

## CLI 相关？

完整命令见 [CLI](tools/cli.md)；冒烟：`./scripts/cli_e2e_smoke.sh build`。
