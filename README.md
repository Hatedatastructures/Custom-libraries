# Custom Libraries

[![许可证](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
![平台](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)
[![CI](https://github.com/Hatedatastructures/Custom-libraries/actions/workflows/CI.yml/badge.svg)](https://github.com/Hatedatastructures/Custom-libraries/actions/workflows/CI.yml)

## 介绍

本仓库提供名为 `wan` 的 C++23 基础组件库，采用头文件为主的交付方式，面向网络服务、异步任务和通用基础设施场景。公共源码位于 `Wan/`，统一入口位于 `Include/Wan.hpp`。

当前的主要模块包括：

- `Wan/Container/`：STL 风格的容器、算法、哈希、树和智能指针组件，详见[容器手册](Docs/Container/README.md)；
- `Wan/Chronicle/`：协程驱动的日志组件；
- `Wan/Fault/`：错误码、错误类别和错误处理工具；
- `Wan/Json/`：基于 Boost.JSON 的 JSON 解析、序列化和文档封装；
- `Wan/Sched/`：基于 Boost.Asio `thread_pool` 的任务调度封装；
- `Wan/Transport/`：传输层抽象和可靠 TCP 传输实现；
- `Wan/Network/`：协议、HTTP、JSON 转换和密码学组件；
- `Wan/Web/`：基于 Boost.Asio/Boost.Beast 的协程 Web 框架。

## 快速开始

### 依赖

- CMake 3.23 或更高版本；
- 支持 C++23 的编译器：GCC 11+、Clang 14+ 或 MSVC 2022+；
- CMake 配置会通过 `FetchContent` 获取 Boost、BoringSSL、jsoncpp 和 Crypto++。

### 构建和测试

仓库只保留一个构建目录 `build/`：

```bash
cmake --preset default
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### 集成到其他项目

```cmake
add_subdirectory(path/to/Custom-libraries)
target_link_libraries(your_target PRIVATE CustomLibraries::CustomLibraries)
```

统一入口示例：

```cpp
#include <Wan.hpp>

int main()
{
    Wan::Sched::Pool Pool{4};
    Pool.Post([] {});
    Pool.Join();
    return 0;
}
```

## 模块说明

### Container

`Wan/Container/Container.hpp` 聚合容器模块，公开 `StandardCon::Vector`、`List`、`TreeMap`、`HashMap`、`Queue`、`Stack` 等类型；也可通过 `Wan::Scl` 访问。具体语义与复杂度见[容器手册](Docs/Container/README.md)。

### Sched

`Wan/Sched/Pool.hpp` 封装 `boost::asio::thread_pool`，提供执行器访问、任务提交、停止和等待接口，适合与 Asio 协程一起使用。

### Transport

`Wan/Transport/Transmission.hpp` 定义可组合的传输层抽象，`Wan/Transport/Reliable.hpp` 提供基于 TCP socket 的可靠传输实现，并支持协程和回调两种异步读写路径。

### Network

`Wan/Network/Network.hpp` 聚合协议和密码学接口。协议组件位于 `Wan/Network/Agreement/`，加密组件位于 `Wan/Network/Crypt/`。会话、路由和 HTTP 服务能力已经迁移到 `Wan/Web/`。

### Web

`Wan/Web/Web.hpp` 聚合当前 Web 框架，包含：

- `Wan::Web::Application` 生命周期宿主和优雅停止；
- `HttpClientConfig` 响应边界与超时控制；
- `SessionConfig` 会话容量与键值限制；
- 路由和路径参数；
- HTTP Server 和 Client；
- 请求/响应上下文；
- JSON、文本和 HTML 响应；
- Query 参数和 Cookie；
- 中间件链；
- 静态文件服务；
- Session；
- 服务指标和 Prometheus 格式输出。

完整的 Web 生命周期、资源限制和直接 `Server` 使用说明见
[`Docs/Web.md`](Docs/Web.md)。

## 目录约定

- `Include/`：公共聚合头文件；
- `Wan/`：项目源码和公共模块；
- `Tests/`：CTest 单元测试和本地网络回环测试；
- `Docs/`：模块文档；
- `Examples/`：示例目标配置，目前没有独立示例程序；
- `build/`：唯一允许保留的构建目录，内部第三方依赖文件不参与项目命名迁移。

项目源码、测试和文档的目录及文件名采用 PascalCase；CMake 和编辑器使用的固定发现文件名保持工具约定不变。

## 许可证

本项目采用 [MIT 许可证](LICENSE)。
