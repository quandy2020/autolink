# **autolink** 🚀

**Autolink base protocol communication framework.**

## 📑 目录

- [✅ 为什么选择 Autolink](#-为什么选择-autolink)
- [🌟 核心能力](#-核心能力)
- [🛠️ 安装方式](#️-安装方式)
- [📘 CMake 集成示例（examples/cpp-cmake）](#-cmake-集成示例examplescpp-cmake)
- [🆚 与 ROS2 对比](#-与-ros2-对比)

## ✅ 为什么选择 Autolink

<span style="color:#2E7D32"><strong>核心理由：可控性</strong></span>  
你可以完全掌控通信框架的行为、依赖和演进节奏，不被外部生态版本节奏绑定。

直接价值：

- **交付更稳**：依赖固定、行为可预测
- **优化更快**：性能/时延问题可直接深入框架层优化
- **总成本更低**：长期维护不为大量非必需能力买单

## 🌟 核心能力

- **通信模型完整**：支持发布订阅、服务客户端、流式服务客户端
- **数据协议标准化**：支持 `proto3`
- **可观测与运维工具**：支持命令行操作（`channel list/info/hz/bw/echo`、`node list/info`）
- **多语言开发**：支持 `C++` 与 `Python`
- **日志体系完善**：支持 LOG 管理
- **插件化架构**：支持动态库管理与热插拔加载
- **多层通信场景**：支持进程内 / 多进程 / 多级通信
- **数据闭环能力**：支持数据录制与回放
- **任务与时序控制**：支持定时器与回调
- **工程化交付**：支持 Docker + 脚本一键部署
- **系统适配**：支持 Linux（C++ 依赖）
- **性能优化能力**：支持协程优化
- **资源调优能力**：支持进程调度策略

## 🛠️ 安装方式

### **方式一：通过 Docker 安装（推荐）** 🐳

Docker 相关脚本位于 **`docker/`** 目录 📁。

#### 1️⃣ 镜像名称 🧩

- `autolink.x86_64`
- `autolink.x86_64.nvidia`
- `autolink.aarch64`

#### 2️⃣ ✅ **推荐命令：构建镜像** 🔧

```bash
# x86_64 标准镜像
python3 docker/build_docker_x86_64.py -f dockerfile/autolink.x86_64.dockerfile

# x86_64 nvidia 镜像
python3 docker/build_docker_x86_64.py -n

# aarch64 镜像
python3 docker/build_docker_aarch64.py -f dockerfile/autolink.aarch64.dockerfile
```

#### 3️⃣ ✅ **推荐命令：运行容器** ▶️

```bash
# 自动检测平台并运行
python3 docker/run.py

# x86_64 标准镜像
python3 docker/run.py -p x86_64 -n no

# x86_64 nvidia 镜像
python3 docker/run.py -p x86_64 -n yes

# aarch64 镜像
python3 docker/run.py -p aarch64
```

### **方式二：本机安装依赖（Ubuntu）** 💻

#### 1️⃣ ✅ **推荐命令：安装依赖** 📦

使用脚本 **`scripts/install_dependency.py`** 安装构建所需依赖：

```bash
python3 scripts/install_dependency.py
```

#### 2️⃣ 🧭 **可选参数：按需安装** ⚙️

```bash
# 仅安装 apt 依赖
python3 scripts/install_dependency.py --apt-only

# 仅安装 thirdparty 依赖
python3 scripts/install_dependency.py --thirdparty-only

# 已安装项自动跳过
python3 scripts/install_dependency.py --skip-installed
```

#### 3️⃣ ✅ **推荐命令：编译并安装** 🧱

```bash
cmake -S . -B build
cmake --build build -j8
sudo cmake --install build
```

## 📘 CMake 集成示例（examples/cpp-cmake）

**`examples/cpp-cmake`** 演示了外部 CMake 项目如何使用 `Autolink` 🔌：

- `find_package(Autolink REQUIRED)`
- `target_link_libraries(your_target PRIVATE Autolink::Autolink)`

#### 🧪 核心 `CMakeLists.txt` 示例

```cmake
cmake_minimum_required(VERSION 3.14)
project(autolink_find_package_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Autolink REQUIRED)

add_executable(autolink_find_package_demo main.cpp)
target_link_libraries(autolink_find_package_demo PRIVATE Autolink::Autolink)
```

#### ⚡ Quick Start：构建示例 🏗️

```bash
cmake -S examples/cpp-cmake -B build/cpp-cmake
cmake --build build/cpp-cmake -j8
```

#### 📌 注意：非默认安装路径

如果 **`Autolink`** 安装在非默认路径，请指定前缀：

<span style="color:#2E7D32"><strong>推荐</strong></span>：优先使用默认安装路径，减少环境变量配置复杂度。  
<span style="color:#C62828"><strong>注意</strong></span>：外部项目找不到包时，通常是 `CMAKE_PREFIX_PATH` 未设置。

```bash
cmake -S examples/cpp-cmake -B build/cpp-cmake \
  -DCMAKE_PREFIX_PATH=/path/to/autolink/install
```

## 🆚 与 ROS2 对比

| 维度 | Autolink | ROS2 |
|---|---|---|
| 定位 | 工程内可控通信框架 | 通用机器人中间件生态 |
| 可控性 | **高**（可深度定制） | 中（受发行版/生态约束） |
| 生态丰富度 | 中 | **高** |
| 部署复杂度 | **低到中** | 中到高 |
| 定制改造成本 | **低** | 中 |
