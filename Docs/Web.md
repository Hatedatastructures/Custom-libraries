# Web 运行时

`Wan/Web/` 基于 Boost.Asio 和 Boost.Beast 提供 HTTP 运行时。服务优先使用 `Wan::Web::Application` 管理生命周期；需要自行控制服务器时可直接使用 `Wan::Web::Server`。

## Application 生命周期

`Application` 持有一个 `Server`，状态依次变化：

```text
Created -> Starting -> Running -> Stopping -> Stopped
                 \-> Failed
```

调用 `Start()` 前注册路由和中间件，并设置监听端口与资源限制：

```cpp
#include <Wan.hpp>

#include <chrono>

int main()
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 4;
    Config.Server.MaxBodySize = 1024 * 1024;
    Config.Server.MaxConnections = 1000;
    Config.DrainTimeout = std::chrono::seconds{10};

    Wan::Web::Application Application{Config};
    Application.GetServer().Listen(8080).Get("/ready",
        [](Wan::Web::Context& Context)
            -> boost::asio::awaitable<void>
        {
            Context.Text("ready");
            co_return;
        });

    Application.Start();

    // 由应用所有者决定何时停止服务。
    Application.Stop();
    Application.Wait();
}
```

仅在已处于 `Running` 时重复调用 `Start()` 不产生额外效果；从 `Stopping`、`Stopped` 或 `Failed` 再启动会抛出 `std::logic_error`。开始停止后可重复调用 `Stop()`。`Wait()` 等待工作线程结束，并在清理后重新抛出先前的启动或生命周期异常。启动前调用 `Stop()` 或 `Wait()` 会直接进入 `Stopped`，不会打开监听器。

通过 `Application::GetServer()` 配置路由、中间件和端口。`Start()` 开始后这些配置会被拒绝；发布监听器前路由器已冻结。

## 停止与清理

`Stop()` 先关闭监听 socket，阻止新连接。现有连接可在 `ApplicationConfig::DrainTimeout` 内完成请求及 keep-alive 处理；到期后剩余连接被取消并关闭。`Wait()` 等待工作线程和连接清理完成，并清空活动连接计数。监听器与连接 socket 的操作通过各自的 strand 串行化。

处理器应使用可取消的异步操作。强制停止会取消处理器/中间件的 awaitable；无限期同步工作无法被中断，可能拖延销毁。处理器内可以调用 `Stop()` 请求停止，但须由拥有者线程调用 `Wait()`。

## 限制与指标

`Application` 在启动监听器和工作线程前检查：

- 超时值非负且可由计时器表示；
- `MaxHeaderSize`、`MaxBodySize` 和 `MaxConnections` 大于零；
- 启用 keep-alive 时 `MaxKeepaliveRequests` 大于零。

服务器在连接接入与解析阶段执行头部、正文和连接数限制。`GetMetrics()` 返回共享的 `ServerMetrics`；`ToPrometheus()` 保留 `wan_*` 指标名称。

## 直接使用 Server

直接使用 `Server` 时，可调用 `RunSync()`，或分步调用 `Start()`、`BeginGracefulStop()`、`ForceStop()` 和 `Wait()`。启动前完成配置；直接调用 `Stop()` 会立即停止，而 `Application` 使用优雅停止流程。

## HTTPS / TLS

`TlsServer` 使用与 `Server` 相同的路由、请求限制、指标和生命周期实现。`ApplicationConfig::Tls` 设置后，`Application` 会托管 TLS 服务器；未设置时仍使用普通 HTTP。`Application::GetServer()` 在两种模式下都可用于注册路由与设置监听端口。

服务端证书链和私钥分别可从 PEM 文件或 PEM 内容加载，每项凭据需要且只需要一种来源。加密私钥可通过 `PrivateKeyPasswordCallback` 提供口令。证书或私钥加载失败会在创建监听器前报错。

```cpp
#include <Wan.hpp>

#include <utility>

Wan::Web::TlsServerConfig Tls;
Tls.CertificateChainFile = "certs/fullchain.pem";
Tls.PrivateKeyFile = "certs/private-key.pem";

Wan::Web::ApplicationConfig Config;
Config.Tls = std::move(Tls);
Wan::Web::Application Application{std::move(Config)};
Application.GetServer().Listen(8443).Get("/health",
    [](Wan::Web::Context& Context) -> boost::asio::awaitable<void>
    {
        Context.Json(R"({"status":"ok"})");
        co_return;
    });
Application.Start();
```

服务端要求 TLS 1.2 或更新版本。本期 HTTP 传输使用 HTTP/1.1。

## HTTPS 客户端

`HttpClient` 增加接收绝对 URL 的 `Get` 和 `Post` 重载，只接受 `http://` 与 `https://`。HTTPS 默认验证证书链和 URL 主机名，并为 DNS 主机发送 SNI。Windows 使用当前用户和本地计算机的 `ROOT` 证书库；Linux 使用 BoringSSL 默认 CA 路径。配置的 CA 文件和内存 PEM 会追加到系统根证书。

```cpp
#include <Wan.hpp>

#include <utility>

Wan::Web::HttpClientTlsConfig Tls;
Tls.CaCertificateFiles.emplace_back("certs/private-root.pem");

Wan::Web::HttpClientConfig Config;
Config.Tls = std::move(Tls);
Wan::Web::HttpClient Client{Executor, Config};
auto Response = co_await Client.Get("https://api.example.com/health");
```

已有的 `Get(Host, Port, Target)` 和 `Post(Host, Port, Target, Body)` 保持明文 HTTP 行为。URL 重定向按 HTTP 规则解析相对地址；HTTP 到 HTTPS 可跟随，HTTPS 到 HTTP 默认返回当前重定向响应。只有将 `AllowInsecureRedirects` 设为 `true` 才允许降级。

运行 `cmake --build build --target HttpsServer HttpsClient` 构建示例。`HttpsServer <certificate-chain.pem> <private-key.pem> [port]` 默认监听 8443，`/health` 返回 JSON；`HttpsClient <http(s)-url> [ca-certificate.pem]` 接受可选的私有 CA 文件。测试目录内的证书和私钥仅供回归测试，不能用于实际服务。

## HTTP 客户端限制

`HttpClient` 保留现有构造方式及 `Get`/`Post` 参数形式。可传入 `HttpClientConfig` 设置正数超时、`MaxResponseHeaderSize` 和 `MaxResponseBodySize`：

```cpp
Wan::Web::HttpClientConfig ClientConfig;
ClientConfig.Timeout = std::chrono::seconds{15};
ClientConfig.MaxResponseHeaderSize = 64 * 1024;
ClientConfig.MaxResponseBodySize = 16 * 1024 * 1024;
Wan::Web::HttpClient Client{Executor, ClientConfig};
```

Beast 响应解析器执行这些限制。负数重定向次数会被拒绝；每次请求单独计算重定向次数。

## Session 限制

`SessionStore` 使用 `SessionConfig` 限制资源。检查容量前先清理过期条目；仍满时创建 Session 会抛出 `std::length_error`：

```cpp
Wan::Web::SessionConfig SessionConfig;
SessionConfig.MaxSessions = 10'000;
SessionConfig.MaxValuesPerSession = 64;
SessionConfig.MaxKeySize = 256;
SessionConfig.MaxValueSize = 64 * 1024;
Wan::Web::SessionStore Sessions{SessionConfig};
```

Session TTL 必须为正且可由平台的 steady clock 表示。`Get()` 返回快照，修改返回的 `SessionData` 不会修改存储。

## 功能范围

当前运行时提供 HTTP/HTTPS 路由、中间件、Session、指标、请求体限制、连接接入控制和生命周期管理。mTLS、WebSocket/WSS、HTTP/2、插件加载及外部配置文件不属于此接口。

## 本地验证

在仓库根目录配置、构建并运行本地测试：

```bash
cmake --preset default
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

TLS 回环测试目标为 `TestTls`；生命周期测试目标为 `TestApplication`。
