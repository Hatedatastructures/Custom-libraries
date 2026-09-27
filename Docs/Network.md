# Network 网络模块

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](../LICENSE)

## 概览

网络模块的统一入口是 `Wan/Network/Network.hpp`，当前负责协议定义、HTTP/JSON 转换和密码学能力。连接生命周期、HTTP Server、HTTP Client 和 Session 已经由 `Wan/Web/` 承担；TCP 传输抽象位于 `Wan/Transport/`。

## 模块索引

| 模块 | 位置 | 作用 |
|---|---|---|
| Agreement | `Wan/Network/Agreement/` | 协议头、请求/响应、HTTP、JSON 和协议转换 |
| Crypt | `Wan/Network/Crypt/` | 对称加密、非对称加密、哈希和完整性校验 |
| Transport | `Wan/Transport/` | TCP 传输层抽象和可靠传输实现 |
| Web | `Wan/Web/` | 路由、上下文、中间件、HTTP 服务、客户端和 Session |

## 公共入口

```cpp
#include "Wan/Network/Network.hpp"

using Wan::Network::Agreement::Request;
using Wan::Network::Agreement::Response;
using Wan::Network::Agreement::ProtocolConverter;
```

`Wan/Network/Network.hpp` 通过 `Wan::Network::Agreement` 导出常用协议类型：

- `Request`、`Response`；
- `RequestHeader`、`ResponseHeader`；
- `ProtocolHeader`、`ProtocolType`、`ChecksumType`；
- `ProtocolConverter`；
- `Wan::Network::Http` 中的 HTTP 类型；
- `Wan::Network::Ciphertext` 中的加密接口。

## 文档

- [Agreement 协议文档](../Wan/Network/Agreement/Appendix.md)
- [Crypt 加密文档](../Wan/Network/Crypt/Appendix.md)
- [Web 模块说明](Web.md)

## 注意事项

- 协议和加密组件是库级基础能力，不会自动创建网络连接；连接和异步生命周期应交给 `Wan/Transport/` 或 `Wan/Web/`。
- `CRC32` 只能用于非密码学完整性检查；敏感数据应使用认证加密或带认证的组合方案。
- 具体实现依赖 Boost.Beast、Boost.JSON 和 Crypto++，请通过 CMake 配置统一获取依赖版本。
