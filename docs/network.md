# Network 模块文档导航

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)


## 🧭 概览简述

- 聚合入口：统一从 `model/network/network.hpp` 引用，导出 `wan::network::{agreement,http,ciphertext,session,business}`。
- 模块速览：
  - `agreement`：协议枚举与协议头、`request/response` 与 `conversion`，支撑报文编解码与校验。
  - `http`：轻量 `HTTP` 封装（基于 `boost::beast`），提供 `request/response` 与常用头字段。
  - `session`：通用会话与连接池（`session_management`、`connection_pool`、`endpoint_config`），含 `TCP/SSL` 客户端/服务端能力。
  - `ciphertext`：加解密与摘要（`arcane_symmetric`、`penumbra_asymmetric`、`umbrage_hash`、`to_hex/to_base64/crc32`）。
  - `business`：`http/https` 代理转发器（`transponder`），基于 `Host` 白名单路由，支持 `forward_sync/forward_async` 与 `json_config_file`。
- 关键能力：
  - 连接池：端点预热、借用/归还与健康检查，降低首包时延。
  - 安全：`SSL/TLS` 证书校验，与对称/非对称加密和哈希的组合使用。
  - 代理：请求/响应过滤器与并发门限控制，提升吞吐与稳定性。

---
## 🧩 一页速览（关键类名）

- agreement：`protocol::request`、`protocol::response`、`protocol::assist::protocol_header`、`protocol::assist::protocol_type`、`protocol::assist::checksum_type`、`protocol::conversion::protocol_converter`。
- session：`conversation::fundamental::session`、`conversation::connection_pool`、`conversation::endpoint_config`、`conversation::session_management`。
- crypt：`encryption::arcane_symmetric`、`encryption::penumbra_asymmetric`、`encryption::umbrage_hash`、`encryption::to_hex`、`encryption::from_hex_to_raw`、`encryption::to_base64`、`encryption::from_base64`、`encryption::crc32`。
- business：`represents::transponder`、`represents::transponder_config`、`represents::connection_pool_defaults`。


## 📒 模块索引表

| 模块 | 作用 | 关键类型 | 文档路径 | 打开链接 |
|---|---|---|---|---|
| agreement | 协议头/请求响应/转换 | `protocol_header`、`request/response`、`protocol_converter`、`protocol_type`、`checksum_type` | `model/network/agreement/appendix.md` | [📄 进入](../model/network/agreement/appendix.md) |
| session | 会话/管理/连接池 | `session`、`session_management`、`connection_pool`、`endpoint_config` | `model/network/session/appendix.md` | [📄 进入](../model/network/session/appendix.md) |
| crypt | 加密/签名/哈希 | `arcane_symmetric`、`penumbra_asymmetric`、`umbrage_hash`、`to_hex/to_base64/crc32` | `model/network/crypt/appendix.md` | [📄 进入](../model/network/crypt/appendix.md) |
| business | HTTP/HTTPS 代理转发 | `transponder`、`transponder_config`、`connection_pool_defaults` | `model/network/business/appendix.md` | [📄 进入](../model/network/business/appendix.md) |




## 📌 说明

- 统一入口：所有网络模块均从聚合头 `model/network/network.hpp` 导出，可在业务侧按需引用。
- 文档风格：各附录采用统一结构（封面徽章、目录锚点、类型与接口、使用示例、注意事项、复杂度与性能）。

---
