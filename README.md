# `template_container`文件与`custom_log`文件
## 一、源文件定位对比

| 模块名称                 | 主要职责             | 核心特性                                                                          |
| -------------------- | ---------------- | ----------------------------------------------------------------------------- |
| `template_container` | 提供通用数据结构与算法模板库   | 智能指针、容器（`vector`、`list`、`map`、`set` 等）、算法、仿函数、哈希、布隆过滤器等  |
| `custom_log`         | 提供可配置、异步、高性能日志系统 | 多级信息聚合、双缓冲异步写入、时间与前缀格式化、二级缓存推送、文件/控制台输出切换                                     |
## 二、文件架构对比

### 1. [`template_container`](template.hpp)
* ### [**跳转详细文档**](template_container.md)
* ### [**跳转到源代码**](template.hpp)
* **命名空间划分**：

  * `custom_exception`、`smart_pointer`、`practicality`、`imitation_functions`、`algorithm`、各类容器命名空间、`base_class_container`、`bloom_filter_container`、以及简化命名空间 `con`、`ptr`、`exc`
* **主要模块**：

  * **智能指针**：`shared_ptr`、`weak_ptr`、`unique_ptr`、`smart_ptr`
  * **算法**：`swap`、`copy`、`find`、`hash_function`
  * **基础工具**：`pair` 与 `make_pair`
  * **容器**：`string`、动态数组 `vector`、双向链表 `list`、栈/队列适配器、树形、关联（`tree_map`/`tree_set`、`hash_map`/`hash_set`）、位图 `bit_set`、布隆过滤器
* **文档结构**：

  1. 函数签名与声明
  2. 作用描述
  3. 返回值说明
  4. 使用示例
  5. 内部原理剖析
  6. 复杂度分析
  7. 边界条件与错误处理
  8. 注意事项（线程安全、迭代器失效等）

