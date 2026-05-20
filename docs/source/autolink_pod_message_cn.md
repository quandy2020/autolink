# Autolink 发送 POD 消息指南

本文说明如何在 Autolink 中发送 POD（Plain Old Data）类型数据，并对应示例：

- `examples/cpp/pod_talker_listener.cpp`（单进程读写）
- `examples/cpp/pod_talker.cpp` + `examples/cpp/pod_listener.cpp`（双进程通信）

## 什么时候使用 POD 消息

- 你有固定二进制布局的数据结构（如状态字、数值数组、时间戳）。
- 你希望避免引入 `.proto` 定义，快速做本地高频通信验证。

## 发送步骤

### 1) 定义 POD 结构体

要求：
- 字段布局固定、无虚函数、无复杂所有权对象。
- 推荐使用 `static_assert(std::is_trivially_copyable<T>::value)` 做编译期约束。

示例：

```cpp
struct SimplePod {
    uint64_t seq;
    double value;
    uint64_t timestamp_ns;
};
```

### 2) 定义消息包装类型

Autolink 传输模板需要消息类型提供序列化接口。包装类型建议实现：

- `TypeName()`
- `descriptor()`
- `ByteSizeLong()`
- `SerializeToArray(void*, int)`
- `ParseFromArray(const void*, int)`
- `SerializeToString(std::string*)`
- `ParseFromString(const std::string&)`

示例见 `PodPacket` 实现（文件：`examples/cpp/pod_packet.hpp`）。

### 3) 创建 writer / reader

```cpp
auto writer = node->CreateWriter<PodPacket>("channel/pod_demo");
auto reader = node->CreateReader<PodPacket>("channel/pod_demo", OnPodMessage);
```

### 4) 发送与接收

发送侧填充 POD 后写出：

```cpp
auto msg = std::make_shared<PodPacket>();
msg->pod.seq = seq;
msg->pod.value = static_cast<double>(seq) * 0.5;
msg->pod.timestamp_ns = autolink::Time::Now().ToNanosecond();
writer->Write(msg);
```

接收侧在回调中读取：

```cpp
void OnPodMessage(const std::shared_ptr<PodPacket>& msg) {
    AINFO << "RX pod seq=" << msg->pod.seq;
}
```

## 构建与运行

### 单进程（talker + listener 在同一进程）

```bash
cd build
cmake .. 
cmake --build . -j8 --target autolink_example_pod_talker_listener
./bin/examples/autolink_example_pod_talker_listener
```

### 双进程（推荐用于验证进程间 SHM）

终端 1：

```bash
cd build
cmake ..
cmake --build . -j8 --target autolink_example_pod_listener autolink_example_pod_talker
./bin/examples/autolink_example_pod_listener
```

终端 2：

```bash
cd build
./bin/examples/autolink_example_pod_talker
```

成功后，listener 终端会持续打印 `RX pod seq=...`。

## 注意事项

- `ByteSizeLong()` 必须与实际序列化字节数一致。
- `SerializeToArray/ParseFromArray` 要做长度检查，避免越界。
- 若后续需要跨语言、可演进 schema，建议使用 protobuf 而非 POD 二进制布局。
