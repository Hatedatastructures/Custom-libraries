# Custom Libraries - 高性能 C++ 基础库集合

<div align="center">

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) [![C++](https://img.shields.io/badge/C%2B%2B-20%2B-blue.svg)](https://en.cppreference.com/w/cpp/20) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

</div>

## 项目简介

`Custom Libraries` 是一个以头文件为主的高性能 `C++` 基础库集合，提供：
- 模拟实现`stl`的容器；
- 基于标准库的线程安全封装（读写锁/互斥锁/条件变量）；
- 调度与线程池框架；
- 网络协议与会话框架；
- 高性能日志系统。

## 模块总览

| 文件夹 | 角色 | 特性 |
|--------|------|------|
| `model/container` | 模拟实现`stl`的容器 | 保持 STL 风格接口、统一聚合头 `container.hpp` |
| `model/concurrent` | 标准库容器的线程安全封装 | 互斥锁/读写锁/条件变量，接口同名同语义，提供阻塞/非阻塞与快照 |
| `model/sched` | 框架：调度与线程池 | 动态扩缩容、`worker` 框架、调度/排名策略、任务编排 |
| `model/network` | 框架：网络协议与会话 | 协议解析、编解码/转换、加密组件、客户端/服务端示例、会话管理 |
| `chronicle` | 高性能日志系统 | 异步日志、多级输出、结构化记录（`Logbook.hpp`） |

说明：
- `model/sched` 与 `model/network` 为框架模块；
- `model/container` 为模拟实现的容器实现逻辑；
- `model/concurrent` 是对标准库容器的线程安全封装。

## 核心特性

### 容器（模拟实现，`model/container`）
- 提供 `vector/list/map/set/queue/stack/string/tree` 等容器的模拟实现；
- 保持 STL 风格接口与一致语义，低侵入替换；
- 配套仿真与算法工具（`simulate_*`：算法、哈希、布隆过滤器、异常等）；
- 统一聚合头 `container.hpp`。

### 并发封装（标准库，`model/concurrent`）
- 覆盖主流标准容器的线程安全封装（`deque/queue/stack/vector/list/map/set/unordered_*`、`priority_queue`、`forward_list`、`bitset`、`string`）；
- 读共享写独占或互斥锁保护，提供阻塞/非阻塞接口与快照 `snapshot()`；



### 调度框架（`model/sched`）
- 动态线程池与 `worker` 框架（`thread_pool.hpp`、`worker.hpp`），弹性扩缩容；
- 调度与排名策略（`scheduling.hpp`、`rank.hpp`），可扩展自定义策略；
- 任务编排与集成（`integration.hpp`、`unit.hpp`），便捷组合与复用；
- 参考文档：`docs/thread_pool_reference_document.md`、`docs/thread_pool_user_ manual.md`。

### 网络框架（`model/network`）
- 协议与会话框架（`agreement/*`、`session/*`），含客户端/服务端示例；
- 编解码与转换（`json.hpp`、`conversion.hpp`），统一协议处理；
- 加密支持（`crypt/encryption.hpp`），可插拔安全组件；
- 统一入口与封装（`network.hpp`），便于集成。

### 日志系统（`chronicle`）
- 异步日志、多级输出、结构化记录（`Logbook.hpp`），适合高并发场景观测。

## 快速开始

### 头文件包含示例

```cpp
// 容器（自研）
#include "model/container/container.hpp"

// 并发封装（标准库）
#include "model/concurrent/concurrent_vector.hpp"

// 调度框架
#include "model/sched/thread_pool.hpp"

// 网络框架
#include "model/network/network.hpp"

// 日志系统
#include "chronicle/Logbook.hpp"
```

### 编译要求
- C++20 或以上编译器；
- Windows / Linux；
- 将 `model/` 与 `chronicle/` 目录加入头文件搜索路径即可使用（以头文件为主）。

## 编译与集成
- 以头文件为主，按需包含相应模块的 `.hpp`；
- 模块间尽量解耦，便于单独集成；
- 推荐为不同模块创建独立编译单元，并启用优化选项（如 `-O2`）。

## 使用建议
- 并发封装：遍历与统计优先使用 `snapshot()`；避免在写入并发下长期持有迭代器/引用；
- 网络框架：协议/会话在业务入口统一封装，便于扩展与测试；
- 日志系统：按级别区分输出，关键路径使用异步日志以降低延迟。

## 文档
- `docs/foundation.md`：基础容器与算法文档；
- `docs/thread_pool_reference_document.md`：线程池技术文档；
- `docs/thread_pool_user_ manual.md`：线程池使用指南；
- `high_performance_servers.md`：高性能服务器开发指南。

## 许可证

本项目采用 [MIT 许可证](LICENSE)，允许自由使用、修改和分发。

---

`Custom Libraries` — 为现代 `C++` 应用提供高性能、可靠的基础组件支持
