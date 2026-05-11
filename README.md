# Custom Libraries

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CI](https://github.com/Hatedatastructures/Custom-libraries/actions/workflows/ci.yml/badge.svg)](https://github.com/Hatedatastructures/Custom-libraries/actions/workflows/ci.yml)

## 介绍

`model/` 是本仓库的核心子系统，采用 `C++20` 标准与"以头文件为主"的交付方式，聚合四类高性能基础组件：

- `container/`：模拟实现 `STL` 容器，接口一致、低侵入替换；
- `concurrent/`：标准容器的线程安全封装（读共享/写独占）；
- `sched/`：可扩展的调度与线程池框架，支持动态扩缩容与任务编排；
- `network/`：协议/会话/加密/转发组件，面向高并发网络应用。

## 快速开始

### 依赖

- C++20 编译器（GCC 11+、Clang 14+、MSVC 2022+）
- CMake 3.20+
- [vcpkg](https://github.com/microsoft/vcpkg)（推荐）

### 安装依赖

```bash
vcpkg install boost-asio boost-beast boost-json boost-log boost-coroutine boost-fiber openssl cryptopp
```

### 构建

```bash
cmake --preset default
cmake --build build
```

### 运行测试

```bash
ctest --test-dir build --output-on-failure
```

### 集成到你的项目

```cmake
# 方式一：作为子目录
add_subdirectory(path/to/Custom-libraries)
target_link_libraries(your_target PRIVATE CustomLibraries::CustomLibraries)

# 方式二：安装后使用
find_package(CustomLibraries REQUIRED)
target_link_libraries(your_target PRIVATE CustomLibraries::CustomLibraries)
```

### 代码示例

```cpp
#include "wan.hpp"
#include <iostream>

int main() {
    // 线程安全的并发 vector
    wan::multi_concurrent::concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    std::cout << "Size: " << vec.size() << "\n";

    // 线程池
    wan::sched::thread_pool pool(4);
    pool.submit([]() { std::cout << "Hello from thread pool!\n"; });

    return 0;
}
```

## 模块说明

### `container/`（模拟实现）

- 提供 `vector/list/map/set/queue/stack/string/tree` 等容器的模拟实现。
- 保持 `STL` 风格接口与一致语义，统一聚合头 `model/container/container.hpp`。
- 配套仿真与算法工具（`simulate_*`：算法、哈希、布隆过滤器、异常、指针与迭代器模拟等）。

### `concurrent/`（线程安全封装）

- 覆盖主流标准容器：`deque/queue/stack/vector/list/map/set/unordered_map/unordered_set`、`priority_queue`、`forward_list`、`bitset`、`string`。
- 采用 `std::shared_mutex` 实现读共享、写独占；队列与条件等待使用 `std::mutex + std::condition_variable`。
- 提供一致接口与只读快照理念（迭代与统计优先使用 `snapshot()`）。

### `sched/`（调度与线程池）

- 核心组件：`thread_pool.hpp`（线程池）、`worker.hpp`（工作线程）、`rank.hpp`（队列/排名策略）、`scheduling.hpp`（调度策略）、`integration.hpp`（统一配置与统计）、`unit.hpp`（任务单元）。
- 能力特性：
  - 动态扩缩容（`set_thread_count / scale_up / scale_down`）；
  - 多策略队列（`fifo/priority/adaptive` 等）；
  - 任务编排（优先级/延迟/超时/依赖/批量提交）；
  - 健康监控与性能采样（吞吐、队列利用率、活跃线程等）。

### `network/`（协议与会话框架）

- 协议：`agreement/*`（`http.hpp/json.hpp/protocol.hpp/conversion.hpp/assist.hpp`），统一的头/体/编解码与转换工具。
- 会话：`session/*`（`fundamental.hpp/conversation.hpp`），`TCP/SSL` 客户端/服务端、连接池与读写收发回调。
- 业务：`business/forwarder.hpp` 提供 `HTTP/HTTPS` 代理转发和数据劫持与上游名单解析（域名解析与回退策略）。
- 加密：`crypt/encryption.hpp` 提供对称/非对称/摘要算法与封装，支持密文封包格式与完整性校验。

## 技术栈

- 标准库（C++20）：`std::shared_mutex`、`std::mutex`、`std::condition_variable`、`std::jthread`、`std::future` 等并发/异步基元。
- Boost 生态：`Boost.Asio`、`Boost.Beast`、`Boost.JSON`。
- 安全库：`OpenSSL`、`Crypto++`。

## 许可证

本项目采用 [MIT 许可证](LICENSE)。
