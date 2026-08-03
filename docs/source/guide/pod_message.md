# POD 消息

用固定布局的 POD（Plain Old Data）做本地高频通信，无需 `.proto`。

示例：

| 方式 | 源码 / 二进制 |
|---|---|
| 单进程 | `examples/cpp/pod_talker_listener.cpp` → `autolink_example_pod_talker_listener` |
| 双进程 | `pod_talker.cpp` + `pod_listener.cpp` |
| 包装类型 | `examples/cpp/pod_packet.hpp`（`PodPacket`） |

## 适用场景

- 状态字、数值数组、时间戳等布局固定的结构  
- 快速验证本机 / SHM 通路  
- 跨语言或可演进 schema → 用 protobuf  

跨机请优先 protobuf + [AMW](../amw/overview.md)；POD 示例面向本机。

## 步骤

**1. 定义 POD**（平凡可拷贝）：

```cpp
struct SimplePod {
    uint64_t seq;
    double value;
    uint64_t timestamp_ns;
};
static_assert(std::is_trivially_copyable_v<SimplePod>);
```

**2. 包装类型**须提供：`TypeName`、`descriptor`、`ByteSizeLong`、`SerializeToArray` / `ParseFromArray`、`SerializeToString` / `ParseFromString`（见 `PodPacket`）。

**3. 读写：**

```cpp
#include "autolink/autolink.hpp"
#include "examples/cpp/pod_packet.hpp"  // 按工程 include 路径调整

auto writer = node->CreateWriter<autolink::examples::PodPacket>("channel/pod_demo");
auto reader = node->CreateReader<autolink::examples::PodPacket>(
    "channel/pod_demo", OnPodMessage);

auto msg = std::make_shared<autolink::examples::PodPacket>();
msg->pod.seq = seq;
writer->Write(msg);
```

## 运行

```bash
export AUTOLINK_PATH=$PWD/autolink
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH

cmake --build build -j8 --target autolink_example_pod_talker_listener
./build/bin/examples/autolink_example_pod_talker_listener
```

双进程：先 `autolink_example_pod_listener`，再 `autolink_example_pod_talker`。  
注意：`ByteSizeLong` 须与实际序列化长度一致，并做缓冲区长度检查。
