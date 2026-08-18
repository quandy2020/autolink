# C++ API

先 `autolink::Init`，再 `CreateNode`，在 Node 上创建通信对象。可运行样例：`examples/cpp/`（构建见 [快速开始](../guide/quickstart.md)）。

头文件入口：`#include "autolink/autolink.hpp"`（Action / Parameter / Record 另含专用头）。

## Lifecycle / Node

```cpp
#include "autolink/autolink.hpp"

if (!autolink::Init(argv[0])) return 1;
auto node = autolink::CreateNode("my_node");           // 可选第二参 namespace
// auto node = autolink::CreateNode("my_node", "/examples");
if (!node) return 1;

while (autolink::OK()) { /* ... */ }
autolink::WaitForShutdown();  // 或 Clear() 后退出
```

| API | 说明 |
|---|---|
| `Init(binary_name)` | 加载 `AUTOLINK_PATH/conf`、调度等；失败返回 `false` |
| `OK()` | 进程未收到 shutdown |
| `Clear()` / `WaitForShutdown()` | 收尾 |

运行前：`export AUTOLINK_PATH=<含 conf/autolink.pb.conf 的根>`。

## Pub / Sub

```cpp
#include "examples.pb.h"
using autolink::examples::Chatter;

auto writer = node->CreateWriter<Chatter>("channel/chatter");
auto msg = std::make_shared<Chatter>();
msg->set_seq(1);
msg->set_content("hi");
writer->Write(msg);

auto reader = node->CreateReader<Chatter>(
    "channel/chatter",
    [](const std::shared_ptr<Chatter>& m) {
        AINFO << "seq=" << m->seq();
    });
```

也可用 `proto::RoleAttributes` 指定 channel、QoS 等。  
无 protobuf 的固定布局消息见 [POD](../guide/pod_message.md)。  
二进制：`autolink_example_talker` / `listener`。

## Service / Client

```cpp
using autolink::examples::Driver;

auto service = node->CreateService<Driver, Driver>(
    "test_server",
    [](const std::shared_ptr<Driver>& req, std::shared_ptr<Driver>& res) {
        res->set_msg_id(req->msg_id());
        res->set_timestamp(autolink::Time::Now().ToNanosecond());
    });

auto client = node->CreateClient<Driver, Driver>("test_server");
// client->WaitForService(timeout);  // 若需要显式等待
auto req = std::make_shared<Driver>();
req->set_msg_id(7);
auto res = client->SendRequest(req);  // 未就绪时可能为空
```

同进程演示：`autolink_example_service`。

## Action

Goal / Feedback / Result 经 **traits** 绑定（与 ROS 2 Action 类似）：

```cpp
#include "autolink/action/simple_action_server.hpp"
#include "autolink/action/action.hpp"
#include "examples.pb.h"

struct SimpleMessageActionTraits {
    using Goal = autolink::examples::SimpleMessageAction_Goal;
    using Feedback = autolink::examples::SimpleMessageAction_Feedback;
    using Result = autolink::examples::SimpleMessageAction_Result;
};

constexpr char kActionName[] = "examples/simple_message_action";

// Server（execute 回调里 PublishFeedback / SucceededCurrent / TerminateCurrent）
using ActionServer =
    autolink::action::SimpleActionServer<SimpleMessageActionTraits>;
auto server = std::make_shared<ActionServer>(
    node, kActionName, [&server]() { /* RunAcceptedGoal(server); */ });

// Client
auto client =
    autolink::action::CreateClient<SimpleMessageActionTraits>(node, kActionName);
while (autolink::OK() && !client->ActionServerIsReady()) { /* sleep */ }

SimpleMessageActionTraits::Goal goal;
goal.set_text("hello");
autolink::action::Client<SimpleMessageActionTraits>::SendGoalOptions opts;
// opts.feedback_callback = ...;
auto accepted = client->AsyncSendGoal(goal, opts);
// 再 AsyncGetResult / wait；可 AsyncCancelGoal
```

完整逻辑：`action_listener.cpp`（server）、`action_talker.cpp`（client）。  
底层：`CreateServer` 三回调；糖层优先 `SimpleActionServer`。

## Parameter

参数挂在 **ParameterServer 所在 Node 名** 上（Client 第二参填该名）。

```cpp
#include "autolink/parameter/parameter_server.hpp"
#include "autolink/parameter/parameter_client.hpp"

auto server = std::make_shared<autolink::ParameterServer>(node);
auto client = std::make_shared<autolink::ParameterClient>(node, "parameter");

server->SetParameter(autolink::Parameter("int", 1));
client->SetParameter(autolink::Parameter("speed", 1.5));

autolink::Parameter p;
client->GetParameter("speed", &p);
AINFO << p.AsDouble();
```

示例：`autolink_example_paramserver`。

## Time / Rate / Timer

```cpp
auto t = autolink::Time::Now();
t.ToNanosecond();
t.ToSecond();

autolink::Rate rate(10.0);  // 10 Hz
rate.Sleep();

autolink::Timer timer(100 /*ms*/, []() { /* tick */ }, false /*oneshot*/);
timer.Start();
```

## Record

```cpp
#include "autolink/record/record_writer.hpp"
#include "autolink/record/record_reader.hpp"
#include "autolink/message/raw_message.hpp"

autolink::record::RecordWriter w;
w.Open("demo.record");
w.WriteChannel("/ch", "autolink.proto.Test", /*desc*/"");
auto raw = std::make_shared<autolink::message::RawMessage>("payload");
w.WriteMessage("/ch", raw, /*time_ns*/ 123);
w.Close();

autolink::record::RecordReader r("demo.record");
autolink::record::RecordMessage msg;
while (r.ReadMessage(&msg)) {
    // msg.channel_name / content / time
}
```

示例：`autolink_example_record`；CLI 见 [recorder](../tools/cli.md#recorder)。

## Component / Launch

1. 继承 `Component<...>`，实现 `Init` / `Proc`  
2. `AUTOLINK_REGISTER_COMPONENT`  
3. DAG 声明 `module_library`、readers 通道与 class_name  
4. `autolink_mainboard -d <dag>` 或 `autolink launch start <launch>`

样例：`examples/cpp/common_component_example/`、`timer_component_example/`。路径解析见 [快速开始 §9](../guide/quickstart.md)。

## 日志

`AINFO` / `AWARN` / `AERROR`（glog）。`GLOG_logtostderr=1` 打到终端。
