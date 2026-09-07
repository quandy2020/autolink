Changelog
=========

2026-09-07
----------

- M7：硬切 Fast DDS **3.x**（FetchContent ``GIT_TAG v3.6.2``；``find_package(fastdds 3)`` / 目标 ``fastdds``）；系统仅 2.x 时 CMake FATAL；不再支持 2.14。
- M7：RTPS API 迁至 ``eprosima::fastdds::*``（Participant / Underlay / Dispatcher / Transmitter / Topology）；M6 Security PropertyPolicy 语义跟迁回归。
- M7：文档 §14 以 3.x 为唯一基线；去掉「默认 2.14 / 3.x 未测」叙事。
- M6：opt-in DDS Security（``AUTOLINK_RTPS_SECURITY=1`` + ``AUTOLINK_RTPS_SECURITY_DIR``）；Auth PKI-DH + Crypto AES-GCM-GMAC；Access 仅 allow-all。
- M6：``SecurityConfig`` 校验证书目录六文件；缺证时 Hub Init fail-loud（不回退明文）；FetchContent Fast DDS ``SECURITY=ON`` + OpenSSL。
- M6：文档 §14 Security 节（目录约定、签名步骤、双机加密清单）；Permissions 本里程碑非 ACL。
- M5：Discovery Server CLIENT（``AUTOLINK_DISCOVERY_SERVER``）；``RtpsStats`` 进程内计数；payload 软限。
- M4：``RtpsParticipantHub`` 双 Participant（topology / transport）。
- M4：``RtpsTopologyBackend`` 跨机 ``ChangeMsg`` 发现（``AUTOLINK_TOPOLOGY_BACKEND=rtps``）；Start 失败回退 local。
- M4：冒烟测 ``rtps_topology_backend_test``；文档 §14 双机拓扑说明。

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
