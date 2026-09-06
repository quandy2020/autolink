Changelog
=========

2026-09-07
----------

- M5：Discovery Server CLIENT（``AUTOLINK_DISCOVERY_SERVER``）；``RtpsStats`` 进程内计数；payload 软限；CMake 对 Fast DDS 3.x 发出 WARNING。
- M4：``RtpsParticipantHub`` 双 Participant（topology / transport）。
- M4：``RtpsTopologyBackend`` 跨机 ``ChangeMsg`` 发现（``AUTOLINK_TOPOLOGY_BACKEND=rtps``）；Start 失败回退 local。
- M4：冒烟测 ``rtps_topology_backend_test``；文档 §14 双机拓扑 / Security 未实现说明。

2026-09-06
----------

- 文档/配置与本机 INTRA+SHM 实现对齐；RTPS 不再静默 fallback SHM。
- 新增 TopologyBackendFactory（``AUTOLINK_TOPOLOGY_BACKEND``）。
- 可选 Fast DDS 2.14 RTPS 数据面（``AUTOLINK_ENABLE_FASTDDS``）。

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
