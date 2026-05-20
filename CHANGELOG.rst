Changelog
=========

2026-05-20
----------

- 移除 FastDDS 依赖路径，通信模式统一为进程内 + SHM。
- 完成 action 示例稳定性修复：节点唯一化、反馈重复处理、首条反馈可达性优化。
- 新增 POD 消息示例：
  - ``examples/cpp/pod_packet.hpp``
  - ``examples/cpp/pod_talker_listener.cpp``
  - ``examples/cpp/pod_talker.cpp``
  - ``examples/cpp/pod_listener.cpp``
- 更新文档：
  - ``docs/source/autolink_pod_message_cn.md``
  - ``docs/source/index.md``
  - ``examples/cpp/README.md``
