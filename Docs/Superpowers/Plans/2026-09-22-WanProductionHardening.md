# Wan 生产加固第二阶段实施计划

> 历史状态：当时已在未提交工作树中实现并验证。

**设计说明：** `Docs/Superpowers/Specs/2026-09-22-WanProductionHardeningDesign.md`

## 全局约束

- 沿用 PascalCase 路径和项目命名规则；保留第三方标识符、wire 值及原有公开调用形式。
- 不执行 reset、clean、checkout、revert、commit、push，也不修改 `build/_deps`；根目录只保留一个 `build/`。
- 先在临时状态中完成验证，再提交解析或配置结果；每个新限制都要有确定性的边界与错误路径测试。

## 任务 1：HttpClient 边界 [x]

文件：

- 修改 `Wan/Web/Client/Client.hpp`、`Tests/TestWeb.cpp`、`Tests/TestPublicApi.cpp` 和 `Docs/Web.md`。

工作：

- 增加 `HttpClientConfig`，验证超时和响应头/正文上限，同时保留原构造调用。
- 使用 Beast 的头部/正文限制，拒绝无效重定向次数；测试超大响应、超时/取消、错误及相对重定向，以及多次请求的独立状态。

## 任务 2：Session 资源限制 [x]

文件：

- 修改 `Wan/Web/Session/Session.hpp`、`Tests/TestStress.cpp`、`Tests/TestPublicApi.cpp` 和 `Docs/Web.md`。

工作：

- 增加有安全默认值的 `SessionConfig` 并检查时间运算；容量检查前回收过期条目，在 `Set` 中限制值数量及大小。
- 测试容量耗尽、过期位置复用、TTL 极值、超大值、并发创建、销毁、清理和快照行为。

## 任务 3：静态文件与协议边界审查 [x]

文件：

- 仅在确认边界缺陷后修改 `Wan/Web/Server/Static.hpp`。
- 仅为有限解析上限及失败时的原子性修改 `Wan/Network/Agreement/Http.hpp` 和 `Wan/Network/Agreement/Protocol.hpp`；扩展 `Tests/TestPublicApi.cpp` 与 `Tests/TestNetwork.cpp`。

工作：

- 覆盖错误格式、超大、路径穿越、根路径及截断输入；证明解析失败不会部分覆盖已有对象，并保留原协议输出与错误文本。

## 任务 4：交付门槛 [x]

文件：

- 仅为项目测试门槛修改 `.github/workflows/CI.yml`；更新 `README.md` 与模块文档中的公开配置。

工作：

- 保留第三方依赖配置；运行配置、完整构建、全部测试、重复压力/Web 测试、静态分析/bugprone、路径/API 与工作树检查。
- 记录 sanitizer 可用性，不掩盖运行时缺失。

## 验收条件

- 新公开配置可通过 `Include/Wan.hpp` 编译；原有测试与新增加固用例均通过。
- 不重新引入旧项目 API 或小写项目路径；范围内不残留无限制的响应正文、Session 数量/值、TTL 运算或解析输入。
- 最终报告区分已验证证据、剩余风险与环境限制。
