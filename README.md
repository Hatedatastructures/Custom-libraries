# Custom Libraries - 高性能C++基础库集合

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17%2B-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

## 📖 项目简介

Custom Libraries 是一个高性能、模块化的C++基础库集合，专为现代C++应用程序和高并发服务器开发而设计。项目提供了从基础数据结构到高级并发组件的完整解决方案。

## 🏗️ 项目架构

```
Custom-libraries/
├── foundation/         # 基础容器和数据结构库
├── model/              # 高级并发和线程池模块
├── framework/          # 框架级组件
├── chronicle/          # 日志系统
├── thread/             # 线程同步工具
├── sever/              # 网络服务器组件
└── include/            # 公共头文件
```

## 🚀 核心特性

### 🔧 基础库 (Foundation)
- **智能指针系统**: `shared_ptr`, `unique_ptr`, `weak_ptr`
- **STL兼容容器**: `vector`, `list`, `map`, `set`, `hash_map`, `hash_set`
- **高级数据结构**: 红黑树、AVL树、哈希表、布隆过滤器
- **算法工具**: 排序、查找、哈希算法等

### ⚡ 并发模块 (Model)
- **高性能线程池**: 动态扩缩容、多种调度策略
- **任务系统**: 普通任务、优先级任务、延迟任务、依赖任务
- **并发容器**: 原子操作容器和标准并发容器
- **工作线程**: 标准、优先级、自适应、协程工作线程

## 🛠️ 快速开始

### 基本使用示例

#### 线程池使用
```cpp
#include "model/module/Thread_pool.hpp"
using namespace pool;

// 创建线程池
auto pool = make_thread_pool(4, 1000);
pool->start();

// 提交任务
auto future = pool->submit([]() 
{
    return 42;
});

int result = future.get();
```

#### 基础容器使用
```cpp
#include "foundation/Foundation.hpp"
using namespace con;

// 使用自定义vector
vector<int> vec;
vec.push_back(1);

// 使用智能指针
auto ptr = make_shared<int>(42);
```

## 📚 详细文档

### 核心模块文档
- [基础库详细文档](docs/foundation.md) - 完整的API文档和使用示例
- [线程池模块文档](docs/thread_pool_reference_document.md) - 线程池详细技术文档
- [线程池使用指南](docs/thread_pool_user_%20manual.md) - 线程池配置和优化指南
- [高性能服务器开发指南](high_performance_servers.md) - 服务器开发最佳实践


## 🔧 编译和集成

### 头文件包含
```cpp
// 基础库
#include "foundation/Foundation.hpp"

// 线程池
#include "model/module/Thread_pool.hpp"

// 并发容器
#include "model/atomic_concurrent/Atomic_vector.hpp"

// 日志系统
#include "chronicle/Logbook.hpp"
```

### 命名空间
```cpp
using namespace con;        // 基础容器
using namespace pool;       // 线程池
using namespace ptr;        // 智能指针
using namespace exc;        // 异常处理
```

## 🎯 适用场景

- Web服务器、API网关
- 游戏服务器、实时通信
- 数据处理服务、消息队列
- 数据库系统、中间件开发

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)，允许自由使用、修改和分发。

---

**Custom Libraries** - 为现代C++应用程序提供高性能、可靠的基础组件支持。


## 🔍 项目结构详解

| 文件夹 | 主要职责 | 核心特性 |
|--------|----------|----------|
| `foundation` | 基础数据结构和容器库 | 智能指针、STL兼容容器、算法工具、哈希表、布隆过滤器 |
| `model/module` | 高级线程池和任务系统 | 动态线程池、多种调度策略、任务优先级、性能监控 |
| `model/atomic_concurrent` | 原子操作并发容器 | 无锁数据结构、原子操作、高并发性能 |
| `model/standard_concurrent` | 标准并发容器 | 线程安全容器、互斥锁保护、标准接口 |
| `framework` | 框架级同步和线程工具 | 高级同步原语、线程池框架 |
| `chronicle` | 高性能日志系统 | 异步日志、多级输出、结构化记录 |
| `thread` | 线程同步工具 | 互斥锁、条件变量、线程管理 |
| `sever` | 网络服务器组件 | TCP/UDP服务器、客户端连接管理 |


## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)，允许自由使用、修改和分发。



