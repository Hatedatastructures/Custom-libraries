# Wan Web 服务运行时实施计划

> 历史状态：当时已在未提交工作树中实现并验证。

> **历史执行说明：** 原计划要求执行者使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans，逐项勾选（`- [ ]`）。

**目标：** 增加面向生产使用的 `Wan::Web::Application` 宿主，明确生命周期、优雅停止、资源上限与回归证据，同时保留直接使用 `Server` 的接口。

**方案：** 在 `Server` 上层加入轻量应用门面，使服务器生命周期可观察、可等待。已接入连接注册在线程安全的注册表中；停止时先关闭接入，再等待活动连接至截止时间，最后取消剩余工作。保留路由、中间件、HTTP wire 值、指标名及直接调用 `Server::RunSync` 的方式。

**技术栈：** C++23、CMake 3.23+、Boost.Asio、Boost.Beast HTTP、已有 Wan Fault/Executor/Metrics/Router/Middleware 组件、CTest 和确定性回环测试。

**设计说明：** `Docs/Superpowers/Specs/2026-09-21-WanWebServiceRuntimeDesign.md`

## 全局约束

- 保留当时全部未提交改动；不执行 reset、clean、checkout、revert、commit 或 push。根目录只保留一个 `build/`，不修改 `build/_deps` 第三方源码。
- 固定发现文件名 `CMakeLists.txt`、`CMakePresets.json`、`.vscode/*` 和 `.claude/settings.local.json` 不变。
- 项目标识符采用 PascalCase/CamelCase，字段采用 `MemberPascalCase`；标准库、Boost、BoringSSL、Crypto++、jsoncpp 标识符不变，不加兼容别名或废弃包装器。
- JSON 键、HTTP 头、User-Agent/Server 字符串、指标名、错误文本、URL 与 wire 格式不变。
- sanitizer 运行时不可用时须报告环境缺口，不宣称覆盖；设计与实现改动保持未提交。

---

### 任务 1：确定生命周期约定与先失败的测试

**文件：** 新增 `Wan/Web/Application/Application.hpp`、`Tests/TestApplication.cpp`；修改 `Tests/CMakeLists.txt`、`Wan/Web/Web.hpp`、`Include/Wan.hpp`。

**接口：** 按设计声明 `Wan::Web::ApplicationState`、`Wan::Web::ApplicationConfig` 和不可复制的 `Wan::Web::Application`。`Application::GetServer()` 用于在 `Start` 前注册路由与中间件；本任务新增的生命周期接口仅有 `Start()`、`Stop()`、`Wait()`、`State()` 和 `GetMetrics()`。

- [x] **步骤 1：加入先失败的公开头文件测试**

增加仅包含 `../Include/Wan.hpp` 的 Boost.Test 用例：构造 `Application`，检查 `Created`，通过 `GetServer()` 注册路由，并用 `static_assert` 验证不可复制或移动。

加入先 `Stop()` 或 `Wait()` 后 `Start()` 的运行时用例，要求不泄漏状态且最终为 `Stopped`。

- [x] **步骤 2：注册新测试目标**

在 `Tests/CMakeLists.txt` 的 `TestSources` 中加入 `TestApplication.cpp`，不重命名已有 CTest 目标。

- [x] **步骤 3：先加入公开声明**

按设计加入枚举值、方法声明和最小状态占位，不增加兼容名称或实现行为。

- [x] **步骤 4：运行聚焦测试，确认约定尚未实现**

运行：

```text
cmake --build build --parallel --target TestApplication
ctest --test-dir build -R "^TestApplication$" --output-on-failure
```

预期：目标因缺少实现而编译失败，或生命周期断言失败；不削弱测试。

### 任务 2：增加线程安全的连接注册表

**文件：** 新增 `Wan/Web/Server/ConnectionRegistry.hpp` 及 `Tests/TestApplication.cpp` 中的注册表用例。

**接口：** `ConnectionRegistry` 是 Web 内部组件，不作为应用公开 API 重新导出。以递增 `std::uint64_t` ID 保存 `std::shared_ptr<Beast::tcp_stream>`，操作为 `Register(Stream) -> Id`、`Remove(Id)`、`Size() const -> std::size_t` 和 `CloseAll()`。

- [x] **步骤 1：编写注册表行为测试**

测试注册、移除、重复移除、空表关闭及两个本地流的 `CloseAll()`；确认先取快照、释放注册表锁后再操作 socket。

- [x] **步骤 2：实现注册与移除**

互斥锁只保护映射和 ID 分配。`Register` 对空流抛出 `std::invalid_argument`；`Remove` 可重复调用且不返回错误。

- [x] **步骤 3：实现有界的全部关闭**

持锁复制共享指针，解锁后对每个流调用 `cancel` 和 `close`，分别记录错误码；单个关闭失败不得阻止后续处理。

- [x] **步骤 4：运行聚焦测试**

运行：

```text
cmake --build build --parallel --target TestApplication
ctest --test-dir build -R "^TestApplication$" --output-on-failure
```

预期：注册表测试全部通过，测试后不残留 socket 或进程。

### 任务 3：提取可控制的 Server 生命周期

**文件：** 修改 `Wan/Web/Server/Server.hpp`、`Tests/TestWeb.cpp` 与 `Tests/TestApplication.cpp`；仅在生命周期协调确需时修改 `Wan/Web/Core/Executor.hpp`。

**接口：** 保留 `Server::Run()`、`RunSync()`、`Stop()`、路由方法与配置字段；加入 `Application` 所需的 `Start()`、`Wait()`、`BeginGracefulStop(std::chrono::seconds)` 和 `ForceStop()`。原 `Stop()` 仍立即停止，`Application` 使用优雅停止。

- [x] **步骤 1：为直接使用 Server 加入回归测试**

保留 `RunSync` 回环测试的协议行为；增加断言，确认 `Stop()` 关闭接入循环且运行线程退出，不泄漏进程或端口。

- [x] **步骤 2：由 Server 持有 acceptor**

将局部 acceptor 改为由应用持有的 `std::unique_ptr<Net::ip::tcp::acceptor>` 或受生命周期保护的等价成员；先使用错误码重载完成打开、`reuse_address`、绑定和监听，再发布已启动状态。

启动失败时关闭已部分打开的 acceptor，明确重启状态；监听成功前不启动连接工作。

- [x] **步骤 3：实现 Start 与 Wait**

`Start()` 只可从 `Created` 调用一次，用于启动执行器及接入协程。`Wait()` 等待工作线程并重新抛出已保存的启动异常；无效状态转换抛出 `std::logic_error`，不改变状态。

`RunSync()` 继续包装 `Start()` 和 `Wait()`，保留原调用行为。

- [x] **步骤 4：注册并注销每个已接入连接**

通过连接数限制后，先将 `tcp_stream` 加入 `ConnectionRegistry`，再启动协程。`HandleConnection` 统一收尾，使正常 EOF、解析失败、处理器异常、写失败、超时及取消均只注销一次并使活动指标减一。

- [x] **步骤 5：实现优雅停止与到期强制停止**

`BeginGracefulStop(DrainTimeout)` 关闭 acceptor、拒绝新连接并允许现有请求完成。只启动一个截止计时器；到期调用 `ConnectionRegistry::CloseAll()`，再停止执行器。重复调用沿用首次截止时间，不新增计时器。

- [x] **步骤 6：测试生命周期竞态与失败路径**

测试端口占用导致的启动失败、并发 `Stop()`、处理器内停止、keep-alive 排空、异步计时器阻塞的处理器、到期强停及指标计数归零；每步后运行 `TestWeb` 与 `TestApplication`。

### 任务 4：实现 Application 门面与状态机

**文件：** 修改 `Wan/Web/Application/Application.hpp`、`Wan/Web/Web.hpp`、`Include/Wan.hpp` 和 `Tests/TestApplication.cpp`。

**接口：** `Application` 持有一个 `std::shared_ptr<Server>` 及生命周期状态；`ApplicationConfig::Server` 原样传给服务器，启动前检查 `DrainTimeout`；`Wait()` 清理完资源后重新抛出已保存的启动或生命周期异常。

- [x] **步骤 1：测试状态转换**

覆盖 `Created -> Starting -> Running -> Stopping -> Stopped`、启动 `Failed`、重复 `Stop`、无效的重复 `Start` 及其他线程读取状态。

- [x] **步骤 2：实现配置验证**

在创建监听器或工作线程前拒绝负数或不可表示的超时、零 `MaxHeaderSize`、零 `MaxBodySize`、零 `MaxConnections`，以及启用 keep-alive 却将 `MaxKeepaliveRequests` 设为零的配置。

- [x] **步骤 3：委托 Start/Stop/Wait**

用生命周期互斥锁串行化状态转换并原子发布状态。`Start` 记录 `Starting`，启动服务器后发布 `Running`；异常时保存 `std::exception_ptr`、发布 `Failed` 并清理。`Stop` 发布 `Stopping`，只触发一次优雅停止，可从处理器调用。

- [x] **步骤 4：完成公开头文件集成**

从 `Wan/Web/Web.hpp` 和 `Include/Wan.hpp` 导出 `Application.hpp`，加入仅使用 `<Wan.hpp>` 的公开头编译用例。

### 任务 5：集成文档与 CMake 回归门槛

**文件：** 仅在需要新编译单元时修改 `CMakeLists.txt`；修改 `Tests/CMakeLists.txt`、`README.md`；仅在跨模块生命周期引用错误时修改 `Docs/Network.md`；若尚无 Web 专用手册，则新增或修改 `Docs/Web.md`。

**文档要求：** 说明 `ApplicationConfig`、生命周期方法、状态语义、停止行为、限制与非目标；准确记录支持的流程，运行时缺失时不声称 sanitizer 覆盖。

- [x] **步骤 1：记录公开接口与生命周期约定**

加入启动应用、在 `Start` 前注册路由、从其他线程停止及调用 `Wait` 的示例；说明处理器须使用可取消的异步操作，直接使用 `Server` 仍受支持。

- [x] **步骤 2：同步 CMake 与测试注册**

确认 `TestApplication.cpp` 位于 `TestSources` 中，统一入口包含新应用头，且第三方目标与依赖路径未改动。

- [x] **步骤 3：搜索过时名称与行为变化**

搜索源码、测试、文档与 CMake 中的旧命名空间、小写项目 API、兼容别名、变化的 wire 字符串及多余的根构建/缓存目录。

### 任务 6：完整验证与静态审查

**文件：** 不计划修改源码，仅修复前述门槛发现的问题。

**输入与产出：** 汇总应用、注册表、服务器、测试、CMake 和文档工作，形成最终证据与剩余风险报告。

- [x] **步骤 1：重新配置**

运行：

```text
cmake --preset default
```

预期：退出码为 0，仅有已知第三方配置警告。

- [x] **步骤 2：构建所有目标**

运行：

```text
cmake --build build --parallel
```

预期：退出码为 0，没有项目编译错误。

- [x] **步骤 3：运行完整及重复测试**

运行：

```text
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure --repeat until-fail:20 -R "^TestStress$"
ctest --test-dir build --output-on-failure --repeat until-fail:10 -R "^TestWeb$"
```

预期：全部已注册测试和重复运行均通过，不残留测试进程。

- [x] **步骤 4：运行项目范围内的静态检查**

运行 PascalCase 命名、`clang-analyzer-*` 与 `bugprone-*` 检查，头文件过滤器限定 `Wan`、`Include` 和 `Tests`；仅按路径排除第三方诊断，不全局禁用检查。

- [x] **步骤 5：完成仓库审查**

运行：

```text
git diff --check
git status --short --branch
```

确认根目录仅有一个 `build/`，无 `clangd` 缓存、重复构建目录、旧项目 API、第三方源码变化，也未执行 commit/push/reset/clean。

## 历史自审结果

- 设计覆盖：状态、生命周期、资源、错误、所有权、并发、测试、文档与验证均映射到任务 1-6。
- 完整性：当时的计划未留未解决标记或未定义的实施步骤。
- 类型一致性：`ApplicationConfig`、`ApplicationState`、`Application`、`ConnectionRegistry`、`Server::Start`、`Server::Wait`、`Server::BeginGracefulStop` 和 `Server::ForceStop` 在各任务中一致。
- 用户约束：原任务要求保留未提交工作树，因此未包含提交步骤。

## 历史执行状态

当时在同一会话中逐项完成实现并检查，工作树按计划保持未提交。
