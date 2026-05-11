# Business 业务转发器模块附录

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

## 📚 目录

- [📖 文件说明](#-文件说明)
- [🏗️ 命名空间与整体结构](#️-命名空间与整体结构)
- [⚙️ 核心类型与配置](#️-核心类型与配置)
- [🚚 转发器类与路由](#-转发器类与路由)
- [🏦 连接池协作](#-连接池协作)
- [🧪 使用示例](#-使用示例)
- [⚠️ 注意事项](#️-注意事项)
- [📊 复杂度与性能](#-复杂度与性能)
- [🧠 写作思路与实现细节](#-写作思路与实现细节)

---

## 📖 文件说明

本文档基于以下头文件的公开接口与实现逻辑进行整理：

- `model/network/business/forwarder.hpp` — `http / https` 请求转发器（代理），路由白名单与上游访问。
- `model/network/agreement/http.hpp` — `HTTP` 请求/响应轻量封装（基于 `boost::beast`）。
- `model/network/session/conversation.hpp` — 会话管理与连接池，提供 `endpoint_config` 与 `connection_pool`。
- `model/network/network.hpp` — 网络模块聚合头（命名空间导出）。

### 📋 文档结构

| 章节 | 内容 | 说明 |
|---|---|---|
| 文件说明 | 关联文件与来源 | 明确依赖与聚合导出 |
| 命名空间与整体结构 | `wan::network::business` 的组成 | 导出方式与内部类关系 |
| 核心类型与配置 | `transponder_config` 与 `connection_pool_defaults` | 关键参数与默认值 |
| 转发器类与路由 | `transponder` 的接口清单与行为 | 同/异步转发、白名单、`SSL` 配置等 |
| 连接池协作 | 与 `conversation::connection_pool` 的协同 | 借用/归还、失效处理、会话配置 |
| 使用示例 | 最小可用示例 | 同步/异步/配置文件/过滤器 |
| 注意事项 | 健壮性与安全建议 | 证书校验、主机头、超时管理 |
| 复杂度与性能 | 工程级预估 | 时间/空间开销评估 |
| 写作思路与实现细节 | 设计与对齐说明 | 维护性与调用关系交代 |

---

## 🏗️ 命名空间与整体结构

- 聚合导出：`model/network/network.hpp` 中将 `represents` 以 `using namespace represents;` 暴露到 `wan::network::business`。
  - 使用者在业务侧应通过 `#include "model/network/network.hpp"` 引入，再使用 `wan::network::business::transponder`。
- 内部依赖：
  - 协议封装：`wan::network::http`（`protocol::http` 导出）。
  - 会话与连接池：`wan::network::session`（来自 `conversation`）。
  - 线程池：`model/sched/thread_pool.hpp`（可选外部执行器）。

---

## ⚙️ 核心类型与配置

- `represents::connection_pool_defaults`
  - `min_connections`：默认最小连接数（`8`）。
  - `max_connections`：默认最大连接数（`16`）。
  - `borrow_timeout`：借用会话超时（`2000ms`）。
  - `connect_timeout`：连接建立超时（`1500ms`）。
  - `health_check_interval`：健康检查间隔（`10s`）。

- `represents::transponder_config`
  - `ssl_ca_file`：`CA` 证书路径。
  - `ssl_cert_file`：客户端证书链路径（双向 `TLS` 可选）。
  - `ssl_key_file`：客户端私钥路径（双向 `TLS` 可选）。
  - `ssl_insecure_skip_verify`：是否跳过证书校验（默认 `false`）。

- `represents::transponder<body, fields>` — 模板参数通常为 `boost::beast::http::string_body` 与 `boost::beast::http::fields`。
  - 类型别名：
    - `request`：`protocol::http::request<body, fields>`。
    - `response`：`protocol::http::response<body, fields>`。
    - `request_func`：`std::function<void(request&)>` 请求过滤器。
    - `response_func`：`std::function<void(response&)>` 响应过滤器。
    - `session_ptr`：`std::shared_ptr<conversation::fundamental::session<request, response>>`。
  - 内部路由项：`struct upstream { domain, host, port, use_https }`。

---

## 🚚 转发器类与路由

- 构造与生命周期：
  - `explicit transponder(boost::asio::io_context& io_context, const transponder_config& config = {})`
    - 构造时创建 `_http_pool` 并 `start()`；可覆盖 `SSL` 与连接池默认配置。
  - `bool stop()`
    - 拒绝新异步任务，停止连接池；幂等。
  - `bool shutdown(std::chrono::milliseconds timeout = 5000ms)`
    - 停止连接池并等待在途任务完成，超时返回是否清空。

- `SSL` 配置：
  - `void set_ssl_ca_file(const std::string& path)`
  - `void set_ssl_cert_file(const std::string& path)`
  - `void set_ssl_key_file(const std::string& path)`
  - `void set_ssl_insecure_skip_verify(bool v)`

- 上游白名单与配置：
  - `void add_upstream(const std::string& domain, const std::string& ip_or_host, std::uint16_t port, bool use_https)`
    - 解析域名到 `ip`（若 `ip_or_host` 为空），将端点加入连接池；应用 `connection_pool_defaults` 到 `endpoint_config`。
  - `void remove_upstream(const std::string& domain)`
    - 从白名单移除并取消连接池中对应端点。
  - `bool json_config_file(const std::string& path)`
    - 加载 `JSON` 文件，内部调用 `load_config_json` 与 `load_config_value`，最终由 `parse_upstreams` 写入白名单与连接池。
  - `bool load_config_json(std::string_view json_text)` / `bool load_config_value(const boost::json::value& jv)` / `void parse_upstreams(const boost::json::array &arr)`
    - 支持数组格式：`[{"domain": "...", "ip": "...", "port": 80, "https": true}, ...]`。

- 同步/异步转发：
  - `response forward_sync(request req, request_func request_filter = {}, response_func response_filter = {})`
    - 基于 `Host` 匹配上游；请求/响应过滤器在发送前/返回后应用；出错返回错误响应（`make_error_response`）。
  - `std::future<response> forward_async(request req, request_func request_filter = {}, response_func response_filter = {})`
    - 并发门限 `_max_async_tasks` 控制任务爆炸；支持外部执行器 `wan::pool::thread_pool` 或回退到 `std::async`。

- 辅助与内部逻辑：
  - `std::pair<std::string, std::optional<std::uint16_t>> parse_host_header(std::string_view host_hdr)` — 解析主机头。
  - `const upstream* match_upstream(std::string_view host_name, std::optional<std::uint16_t> req_port)` — 白名单匹配。
  - `void apply_host_header_if_missing(request &req, const upstream &up)` — 兜底设置 `Host`。
  - `response perform_upstream_plain(const request &req, const upstream &up)` — 借用连接池会话、发送请求、接收并解析响应，错误时 `invalidate` 并返回错误。
  - `response perform_upstream_ssl(const request &req, const upstream &up)` — 根据端点 `session_cfg` 决定 `SSL`，逻辑复用 `plain`。
  - `response perform_upstream(const request &req,const upstream &up)` — 根据 `use_https` 选择具体实现。

---

## 🏦 连接池协作

- 借用/归还：
  - `auto borrowed = _http_pool.borrow(host, port)`；成功返回 `session_ptr`，失败返回错误响应。
  - 成功发送后等待解析完成，最后 `give_back(session_ptr)`；若发生错误或超时，调用 `invalidate(session_ptr)`。

- 端点会话配置：
  - 由 `add_upstream` 与 `parse_upstreams` 统一写入 `endpoint_config`：
    - `session_cfg._enable_ssl` / `_tls_server_name` / `_ssl_ca_file` / `_ssl_cert_file` / `_ssl_key_file` / `_ssl_insecure_skip_verify`。
    - 连接池默认参数由 `connection_pool_defaults` 应用。

- 解析与保活：
  - 若请求未显式设置 `connection: keep-alive`，转发器会默认设置；响应接收采用缓冲 `string` 拼接并在完整解析后返回。

---

## 🧪 使用示例


### 示例一：同步转发基本用法（`AES` 无关，本模块仅路由与转发）

```cpp
/**
 * @brief 同步转发示例
 */
void 示例_同步转发()
{
    boost::asio::io_context io_context;

    // 构造转发器，并设置 SSL 配置（如需 HTTPS）
    represents::transponder_config transponder_config_obj{};
    transponder_config_obj.ssl_ca_file = "./ca.pem";
    represents::transponder<> http_transponder(io_context, transponder_config_obj);

    // 添加上游映射：根据 Host 白名单路由
    http_transponder.add_upstream("example.com", "93.184.216.34", 80, false);

    // 构造简单请求
    using request  = wan::network::http::request<boost::beast::http::string_body, boost::beast::http::fields>;
    using response = wan::network::http::response<boost::beast::http::string_body, boost::beast::http::fields>;

    request client_request;
    client_request.base().method(boost::beast::http::verb::get);
    client_request.base().target("/");
    client_request.base().set(boost::beast::http::field::host, "example.com");

    // 直接同步转发
    response upstream_response = http_transponder.forward_sync(std::move(client_request));
    // 此处可检查 status 与 body
}
```

### 示例二：异步转发与外部执行器

```cpp
/**
 * @brief 异步转发示例，结合外部线程池
 */
void 示例_异步转发()
{
    boost::asio::io_context io_context;
    represents::transponder<> http_transponder(io_context);
    http_transponder.add_upstream("api.example.com", "203.0.113.10", 443, true);

    // 配置外部线程池作为执行器
    auto executor_pool = std::make_shared<wan::pool::thread_pool>(4);
    http_transponder.set_async_executor(executor_pool);

    using request  = wan::network::http::request<boost::beast::http::string_body, boost::beast::http::fields>;
    using response = wan::network::http::response<boost::beast::http::string_body, boost::beast::http::fields>;
    request client_request;
    client_request.base().method(boost::beast::http::verb::post);
    client_request.base().target("/v1/items");
    client_request.base().set(boost::beast::http::field::host, "api.example.com");
    client_request.body() = "{}";
    client_request.base().set(boost::beast::http::field::content_type, "application/json");

    // 请求/响应过滤器（可选）
    auto request_filter = [](request &req){ req.base().set(boost::beast::http::field::user_agent, "agent/1.0"); };
    auto response_filter = [](response &resp){ /* 可做审计或改写 */ };

    std::future<response> async_task = http_transponder.forward_async(std::move(client_request), request_filter, response_filter);
    response upstream_response = async_task.get();

    // 关闭与关停（可在进程退出前调用）
    http_transponder.stop();
    http_transponder.shutdown(std::chrono::milliseconds{3000});
}
```

### 示例三：从 `JSON` 文件加载上游配置

```cpp
/**
 * @brief 从 JSON 文件批量加载白名单与端点配置
 */
void 示例_JSON加载()
{
    boost::asio::io_context io_context;
    represents::transponder<> http_transponder(io_context);

    // JSON 文件结构示例：参见 model/network/business/example.json
    // 建议 `port` 使用数值类型：{"domain":"www.example.com","ip":"","port":80,"https":false}
    bool load_ok = http_transponder.json_config_file("./example.json");
    if (!load_ok)
    {
        // 处理加载失败（路径或格式错误）
    }
}
```

### 示例四：移除上游与错误处理

```cpp
/**
 * @brief 移除上游并进行错误响应演示
 */
void 示例_移除上游()
{
    boost::asio::io_context io_context;
    represents::transponder<> http_transponder(io_context);
    http_transponder.add_upstream("remove.me", "192.0.2.1", 80, false);

    // 随后移除上游
    http_transponder.remove_upstream("remove.me");

    using request  = wan::network::http::request<boost::beast::http::string_body, boost::beast::http::fields>;
    using response = wan::network::http::response<boost::beast::http::string_body, boost::beast::http::fields>;
    request client_request;
    client_request.base().method(boost::beast::http::verb::get);
    client_request.base().target("/");
    client_request.base().set(boost::beast::http::field::host, "remove.me");

    // 不在白名单中，将返回错误响应（403/502）
    response upstream_response = http_transponder.forward_sync(std::move(client_request));
}
```

---

## ⚠️ 注意事项

- 证书校验：
  - 生产环境必须开启证书校验；仅在测试环境考虑 `ssl_insecure_skip_verify=true`。务必正确配置 `ssl_ca_file`。
- 主机头与白名单：
  - 路由严格基于 `Host`，若缺失则按上游配置兜底；建议客户端始终设置正确 `Host`。
- 超时与并发门限：
  - 连接池 `borrow_timeout`/`connect_timeout` 与转发器 `_max_async_tasks` 控制负载飙升，避免任务爆炸。
- 错误与关闭：
  - 发送失败与超时将 `invalidate` 会话；关闭流程应先 `stop()` 后 `shutdown()`，等待在途任务结束。
- 配置文件：
  - 推荐 `port` 使用数值类型；当 `ip` 为空时将尝试 `domain` 解析，失败回退为域名。

---

## 📊 复杂度与性能

- 路由匹配：
  - 白名单采用 `unordered_multimap`，按 `domain` 查找为均摊 `O(1)`；端口匹配为遍历 `O(k)`（`k` 为同域名条目数，通常较小）。
- 会话借还：
  - 借用/归还为均摊 `O(1)`；健康检查与维护在后台周期执行，开销与端点数量线性相关。
- 网络往返：
  - 端到端时延主导在网络；转发器侧的 `解析/拷贝/同步等待` 附加成本通常相对较小。
- 并发控制：
  - 外部线程池或 `std::async` 承担调度；门限 `_max_async_tasks` 防止过载。

---

## 🧠 写作思路与实现细节

- 思路说明：
  - 结构与封面、目录、章节严格对齐 `agreement/appendix.md` 风格，确保跨模块文档一致性。
  - 接口信息完全来源于 `forwarder.hpp` 与聚合导出 `network.hpp`，避免引入实现细节的耦合风险。

- 怎么实现的：
  - 解析 `forwarder.hpp`，抽取 `transponder_config`、`connection_pool_defaults`、`transponder` 的公开接口与行为（路由、转发、`SSL` 配置、关闭等）。
  - 结合连接池接口语义，补充端点配置传播与错误场景下的 `invalidate` 处理。

- 怎么调用：
  - 业务通过 `#include "model/network/network.hpp"` 引入 `wan::network::business`，即可直接使用 `transponder`。
  - 同步/异步转发、加载 `JSON` 配置与外部线程池接入，详见示例代码。

- 被谁调用：
  - 服务端代理、网关与中间层业务代码；测试侧用于验证路由与上游连通性。

- 健壮性与可维护性：
  - 强调白名单路由与证书校验配置；示例以最小可用为目标，避免冗余逻辑，便于扩展与调试。