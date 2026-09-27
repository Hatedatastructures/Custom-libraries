# Wan Web 服务运行时设计

## 历史状态

第一阶段扩展设计当时已实现。本文仅涉及 HTTP 应用生命周期及生产环境边界控制；TLS、WebSocket、HTTP/2、插件加载和外部配置文件属于其他阶段。

## 目标

基于既有 `Wan::Web::Server`、`Executor`、`Router`、`MiddlewareChain`、`ServerMetrics`、`SessionStore` 和 `Fault` 构建轻量应用宿主，明确启动、停止、失败、资源限制和健康状态，同时保留 HTTP wire 行为及直接使用 `Server` 的接口。

## 不在范围内

- 本阶段不实现 TLS、WebSocket、HTTP/2、动态插件或依赖注入容器。
- 配置使用有类型的 C++ 值，不引入 JSON 配置文件。
- 不修改已有 JSON 键、HTTP 头、状态文本、指标名、协议字符串，也不添加旧名称兼容别名。

## 公开接口

新增 `Wan/Web/Application/Application.hpp`，由 `Wan/Web/Web.hpp` 和 `Include/Wan.hpp` 导出。

```cpp
namespace Wan::Web
{
    enum class ApplicationState : std::uint8_t
    {
        Created,
        Starting,
        Running,
        Stopping,
        Stopped,
        Failed
    };

    struct ApplicationConfig
    {
        ServerConfig Server;
        std::chrono::seconds DrainTimeout{10};
    };

    class Application
    {
    public:
        explicit Application(ApplicationConfig Config = {});
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        [[nodiscard]] auto GetServer() noexcept -> Server&;
        [[nodiscard]] auto GetServer() const noexcept -> const Server&;
        [[nodiscard]] auto GetMetrics() const noexcept
            -> std::shared_ptr<ServerMetrics>;
        [[nodiscard]] auto State() const noexcept -> ApplicationState;

        void Start();
        void Stop();
        void Wait();
    };
}
```

上述签名是本阶段的公开接口目标；实施前须先在设计中明确任何变更。下述所有权和状态保证为必要条件。仅在已处于 `Running` 时重复 `Start` 不产生额外效果；其他终态或过渡态下重复启动抛出 `std::logic_error`。`Stopping` 或 `Stopped` 后重复 `Stop` 不产生额外效果。

## 状态机

```text
Created -> Starting -> Running -> Stopping -> Stopped
              |            |          |
              +----------> Failed <---+
```

- `Created`：已接受配置，执行器与监听器尚未启动。
- `Starting`：正在创建监听器与工作资源。
- `Running`：接入新连接并处理请求。
- `Stopping`：监听器已关闭，现有连接可继续完成工作。
- `Stopped`：工作线程、连接、计时器及拥有的资源已释放。
- `Failed`：启动或生命周期出现不可恢复错误；清理部分创建的资源后，`Wait` 重新抛出已保存的异常。
- 状态转换通过原子状态配合生命周期互斥锁，或等价的执行器串行化机制，保证可观察性。
- 处理器可调用 `Stop`，但不得让工作线程等待自身退出。
- 析构时请求停止并等待应用拥有的线程与连接，不得留下可 join 的线程或访问已销毁应用的分离工作。用户处理器须使用可取消的异步操作；无限期同步工作可能拖延析构，不在取消保证内。

## 生命周期与停止

仅在需要可控监听器生命周期时扩展现有 `Server`；实现优雅停止时，acceptor 不能继续是不可观察的局部对象。

1. `Start` 完成打开、配置、绑定与监听后才发布 `Running`。
2. 启动失败时关闭部分创建的资源，保留原错误并发布 `Failed`。
3. `Stop` 先关闭 acceptor，阻止新连接。
4. 现有连接可在 `DrainTimeout` 到期前完成当前请求及 keep-alive 周期。
5. 到期后取消并关闭剩余流。
6. 由拥有者线程等待工作线程；工作线程可请求停止，但不能等待自身。
7. `Wait` 可在 `Start` 前、`Stop` 后或启动失败后调用。
8. 每个已接入连接在停止时只使活动连接指标减一。

## 资源限制

HTTP 限制继续来自 `ServerConfig`；应用须在启动前验证：

- 使用默认值处理后，`Threads >= 1`。
- `MaxHeaderSize > 0`.
- `MaxBodySize > 0`.
- `MaxConnections > 0`.
- 启用 keep-alive 时 `MaxKeepaliveRequests > 0`。
- 只有文档规定零表示无截止时间时超时才可为零；拒绝负时长。
- `DrainTimeout` 非负，计时器运算不得溢出。
- Beast 解析器在分派处理器前检查头部与正文限制。
- 原子化连接接入判断，启动工作前关闭被拒绝的连接。
- 不引入无界队列、重试循环、请求缓冲区、Session 集合或重定向链。

## 健康与就绪状态

第一阶段只通过 `ApplicationState` 和指标提供状态，不创建可能与用户路由冲突的隐式 `/health` 路由。后续阶段可在明确路由注册规则后增加健康检查处理器。

## 错误语义

- 配置错误在打开资源前抛出 `std::invalid_argument`；无效生命周期转换抛出 `std::logic_error`，不部分启动应用。
- OS 与 Asio 错误保留原 `boost::system::error_code`，或仅在既有传输边界通过 `Wan::Fault` 映射。
- 处理器异常由既有错误中间件或连接边界拦截，不得传播到分离协程终止路径。
- 有意停止期间的取消不报告为服务器错误；每条异步完成路径最多调用一次处理器。

## 并发与所有权

- `Application` 拥有 `Server` 和生命周期资源；`Server` 拥有已接入连接的工作及执行器。
- 应用状态读取无锁或受互斥锁保护，写入串行化。
- `Start` 前完成路由与中间件注册；`Running` 期间的注册须拒绝或显式同步，第一阶段优先拒绝。
- 指标保持原子化，格式化输出时不持有生命周期锁。
- 析构顺序：停止接入、取消/排空连接、停止执行器、等待工作线程、释放指标和服务器。

## 测试矩阵

新增 `Tests/TestApplication.cpp` 并在 `Tests/CMakeLists.txt` 中注册。

- 默认构造、状态转换、启动、停止、等待、重复停止及无效重复启动。
- 端口占用导致启动失败，且不泄漏工作线程或监听器；处理器异常及未知异常被拦截。
- 活动 keep-alive 连接的排空、到期强制关闭阻塞连接。
- `MaxConnections` 边界内接入与超限拒绝；真实回环 socket 上的正文限制与超时。
- 多线程并发停止；在停止、失败及运行状态下析构。
- 各关闭路径上的活动连接指标归零；仅包含公开头文件即可实例化完整接口。

既有 `TestWeb` 与 `TestStress` 的行为保持不变，继续覆盖路由、Cookie、HTTP 客户端、服务器、错误输入、Session 与容器压力。

## 验证门槛

原计划要求完成实施前通过以下命令：

```text
cmake --preset default
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure --repeat until-fail:20 -R "^TestStress$"
ctest --test-dir build --output-on-failure --repeat until-fail:10 -R "^TestWeb$"
git diff --check
git status --short --branch
```

另运行项目范围内的 `readability-identifier-naming`、`clang-analyzer-*` 和 `bugprone-*` 检查。sanitizer 运行时不可用时须报告，不能声称已覆盖。

## 风险与措施

- `Server::RunSync` 可能需拆出启动/等待原语；继续以包装方式保留直接调用接口。
- 处理器若等待应用停止，优雅排空可能死锁；执行截止时间并在到期后取消。
- 工作线程内停止可能等待自身；等待前识别调用线程。
- 绑定/监听失败可能泄漏部分初始化的监听器；使用错误码重载和 RAII 清理。
- 异常与正常关闭路径可能重复减少指标；统一连接收尾。
- 新生命周期状态可能与直接 `Server::Stop` 竞态；两条路径共用可重复调用的停止原语。

## 验收条件

原阶段仅在公开接口、实现、CMake、文档和测试同步、规定命令通过、项目静态检查无未解释诊断、第三方文件与 wire 值不变、工作树不残留额外根构建/缓存目录或测试进程时才视为完成。
