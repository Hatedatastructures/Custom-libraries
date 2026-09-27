# Wan 生产加固第二阶段设计

## 历史状态

当时已在未提交工作树中实现并验证。本阶段限定于现有 Web 和协议接口的资源边界、超时语义、解析上限和交付门槛。

## 目标

提高现有库组件处理恶意或无界输入时的安全性，同时保留 HTTP wire 值、协议字符串、第三方接口及第一阶段交付的直接 `Server` 生命周期。

## 范围

1. 为 `Wan::Web::HttpClient` 增加超时、响应头与正文大小的强类型配置，保持原构造及 `Get`/`Post` 调用形式兼容。
2. 为 `Wan::Web::SessionStore` 增加容量、每 Session 值数量及大小限制，自动回收过期条目并检查 TTL 运算。
3. 为 `Wan::Web::StaticFileServer` 与协议 `FromString` 增加边界测试，统一错误格式、超大、根路径及路径穿越输入的错误码行为。
4. CI 与本地测试加入重复生命周期及错误输入门槛。

## 不在范围内

- 不增加 TLS、WebSocket、HTTP/2、代理、cookie-jar 或连接池功能，也不增加兼容别名或废弃包装器。
- 不修改 JSON 键、HTTP 头值、User-Agent/Server 字符串、Prometheus 指标名、协议字段、错误文本、URL 或 wire 格式。
- 不修改第三方源码及生成的依赖文件。

## HttpClient 约定

增加 `Wan::Web::HttpClientConfig`，要求超时为正且可表示，响应头/正文上限大于零。`HttpClient` 在发起请求前验证配置；Beast 解析器同时限制头部和正文。DNS、连接、写入与读取仍可通过现有 Asio 执行器和截止时间取消。

负数重定向次数作为无效参数拒绝。相对、协议相对和绝对 `http://` 重定向沿用原行为；不支持或格式错误的地址终止重定向并返回响应。每次请求的重定向次数有上限。

## Session 约定

增加 `Wan::Web::SessionConfig`，`MaxSessions`、`MaxValuesPerSession` 和 `MaxValueSize` 默认值均为正。`Create` 在容量检查前移除过期条目，仍满时抛出 `std::length_error`。`Set` 拒绝超大键/值或超出数量上限的新值。TTL 必须为正且可由 `steady_clock::duration` 表示，时间点运算不得回绕。

Session ID 与 `sess_` 令牌格式保持不变。返回的 `SessionData` 是快照，修改它不影响存储。

## 解析与文件约定

协议文本解析器在提交状态前执行有限的头部与消息上限，失败时保持目标对象不变。静态文件路径规范化后仍须位于根目录内；拒绝路径穿越、仅根路径、缺失、非普通文件及错误格式的目标，普通查找失败不抛异常。

## 验证约定

本阶段须通过原有完整测试、重复 `TestStress`/`TestWeb`、新增加固测试、`git diff --check`、PascalCase/API 审查及项目范围内的 analyzer/bugprone 检查。仅在编译器提供实际运行时时执行 sanitizer，否则明确报告环境缺口。
