# autolink

Autolink base protocol communication framework.

## 安装方式

### 方式一：通过 Docker 安装（推荐）

Docker 相关脚本位于 `docker/` 目录。

镜像名称：

- `autolink.x86_64`
- `autolink.x86_64.nvidia`
- `autolink.aarch64`

构建镜像示例：

```bash
# x86_64 标准镜像
python3 docker/build_docker_x86_64.py -f dockerfile/autolink.x86_64.dockerfile

# x86_64 nvidia 镜像
python3 docker/build_docker_x86_64.py -n

# aarch64 镜像
python3 docker/build_docker_aarch64.py -f dockerfile/autolink.aarch64.dockerfile
```

运行容器示例：

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

### 方式二：本机安装依赖（Ubuntu）

使用脚本 `scripts/install_dependency.py` 安装构建所需依赖：

```bash
python3 scripts/install_dependency.py
```

常用参数：

```bash
# 仅安装 apt 依赖
python3 scripts/install_dependency.py --apt-only

# 仅安装 thirdparty 依赖
python3 scripts/install_dependency.py --thirdparty-only

# 已安装项自动跳过
python3 scripts/install_dependency.py --skip-installed
```

依赖安装后可执行：

```bash
cmake -S . -B build
cmake --build build -j8
sudo cmake --install build
```

## examples/cpp-cmake 使用方式

`examples/cpp-cmake` 演示了外部 CMake 项目如何使用：

- `find_package(Autolink REQUIRED)`
- `target_link_libraries(your_target PRIVATE Autolink::Autolink)`

示例 `CMakeLists.txt`（核心部分）：

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

构建示例：

```bash
cmake -S examples/cpp-cmake -B build/cpp-cmake
cmake --build build/cpp-cmake -j8
```

如果 `Autolink` 安装在非默认路径，请指定前缀：

```bash
cmake -S examples/cpp-cmake -B build/cpp-cmake \
  -DCMAKE_PREFIX_PATH=/path/to/autolink/install
```
