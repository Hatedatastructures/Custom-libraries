# Custom Libraries 总览

<div align="center">

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

</div>

## 介绍

`model/` 是本仓库的核心子系统，采用 `C++20` 标准与“以头文件为主”的交付方式，聚合四类高性能基础组件：
- `container/`：模拟实现 `STL` 容器，接口一致、低侵入替换；
- `concurrent/`：标准容器的线程安全封装（读共享/写独占）；
- `sched/`：可扩展的调度与线程池框架，支持动态扩缩容与任务编排；
- `network/`：协议/会话/加密/转发组件，面向高并发网络应用。

整体设计强调：模块解耦、统一接口风格、可插拔策略、易集成落地；适用于服务端开发、网络代理、数据处理与并发调度等场景。

## 技术栈与外部依赖

- 标准库（C++20）：`std::shared_mutex`、`std::mutex`、`std::condition_variable`、`std::jthread`、`std::future` 等并发/异步基元。
- Boost 生态：
  - `Boost.Asio`：异步 `IO` 与 `TCP/UDP/SSL` 网络基础；
  - `Boost.Beast`：`HTTP` 协议与流处理；
  - `Boost.JSON`：高性能 `JSON` 解析与序列化；
- 安全库：
  - `OpenSSL`：`TLS/SSL` 握手与证书验证（`X509_check_host` 等）；
  - `Crypto++`：对称/非对称加密、`HMAC`、`AES-GCM/CBC/CTR`、`RSA-PSS/OAEP`、`ElGamal`、`SHA256/MD5` 等。

依赖建议：优先使用 `vcpkg` 或系统包管理安装上述库；链接时启用 `SSL` 与 `Crypto++`，并确保编译选项使用 `-std=c++20`。

## 模块说明（基于 `model/`）

### `container`/（模拟实现）
- 提供 `vector/list/map/set/queue/stack/string/tree` 等容器的模拟实现。
- 保持 `STL` 风格接口与一致语义，统一聚合头 `model/container/container.hpp`。
- 配套仿真与算法工具（`simulate_*`：算法、哈希、布隆过滤器、异常、指针与迭代器模拟等）。

### `concurrent`/（线程安全封装）
- 覆盖主流标准容器：`deque/queue/stack/vector/list/map/set/unordered_map/unordered_set`、`priority_queue`、`forward_list`、`bitset`、`string`。
- 采用 `std::shared_mutex` 实现读共享、写独占；队列与条件等待使用 `std::mutex + std::condition_variable`。
- 提供一致接口与只读快照理念（迭代与统计优先使用 `snapshot()`）。

### `sched`/（调度与线程池）
- 核心组件：`thread_pool.hpp`（线程池）、`worker.hpp`（工作线程）、`rank.hpp`（队列/排名策略）、`scheduling.hpp`（调度策略）、`integration.hpp`（统一配置与统计）、`unit.hpp`（任务单元）。
- 能力特性：
  - 动态扩缩容（`set_thread_count / scale_up / scale_down`）；
  - 多策略队列（`fifo/priority/adaptive` 等）；
  - 任务编排（优先级/延迟/超时/依赖/批量提交）；
  - 健康监控与性能采样（吞吐、队列利用率、活跃线程等）。

### `network`/（协议与会话框架）
- 协议：`agreement/*`（`http.hpp/json.hpp/protocol.hpp/conversion.hpp/auxiliary.hpp`），统一的头/体/编解码与转换工具。
- 会话：`session/*`（`fundamental.hpp/conversation.hpp`），`TCP/SSL` 客户端/服务端、连接池与读写收发回调。
- 业务：`business/forwarder.hpp` 提供 `HTTP/HTTPS` 代理转发和数据劫持与上游名单解析（域名解析与回退策略）。
- 加密：`crypt/encryption.hpp` 提供对称/非对称/摘要算法与封装，支持密文封包格式与完整性校验。


## 编译与集成建议

- 编译：`C++20`，启用优化（如 `-O2`，不启用优化可能会出现标准库静态链接问题，在链接阶段加入 `OpenSSL` 与 `Crypto++`。
- 头文件路径：将 `model/` 目录加入编译器 `include` 搜索路径；模块按需包含对应 `.hpp`。
- 依赖管理：推荐 `vcpkg`（`boost-asio`, `boost-beast`, `boost-json`, `openssl`, `cryptopp`）。
- 迭代器与并发：在写入并发下避免长期持有迭代器/引用；统计遍历优先走只读接口或快照。
- 网络安全：启用 `TLS` 时配置 `SNI` 与证书验证，必要时加载 `CA` 文件并开启主机名检查。

## 许可

本项目采用 [MIT 许可证](LICENSE)，可自由使用、修改与分发。
