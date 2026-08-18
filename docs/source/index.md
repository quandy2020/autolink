# Autolink 文档

本地优先的通信框架：同进程 INTRA、同机 SHM。

## 阅读路径

1. [快速开始](guide/quickstart.md) — 环境、构建、Pub/Sub / Service / Action / Param / Record / Component  
2. [术语](guide/terms.md) — Node / Channel / HYBRID 等  
3. [C++ API](api/cpp.md) / [Python API](api/python.md) — 可复制接口写法  
4. [CLI](tools/cli.md) — `autolink` 全部子命令  

专题：[POD 消息](guide/pod_message.md) · [调度器](guide/scheduler.md) · [FAQ](faq.md)

## 常用命令速记

```bash
export AUTOLINK_PATH=$PWD/autolink
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH
export PATH=$PWD/build/bin:$PATH

./build/bin/examples/autolink_example_listener
./build/bin/examples/autolink_example_talker
autolink doctor
./scripts/cli_e2e_smoke.sh build
```
