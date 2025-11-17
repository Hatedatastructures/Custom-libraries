# 目标
- 在 `c:\Users\C1373\Desktop\Custom-libraries\model\sched\test.cpp` 编写可复现的基准测试，输出吞吐、总耗时、平均延迟、p50/p95/p99、队列峰值/当前值、活跃/总线程、队列利用率。

# 接口对齐
- 工厂：`make_thread_pool(std::size_t thread_count, std::size_t queue_size)`
- 提交：`submit(fn)`、`submit_priority(weight::high, fn)`、`submit_delayed(std::chrono::milliseconds, fn)`
- 统计：`set_statistics_handler(void(const pool_statistics&))`、`get_statistics() -> const pool_statistics&`
- 指标：`get_thread_count()`、`get_active_thread_count()`、`get_rank_utilization()`

# 实现内容（test.cpp）
- 负载函数 `do_work(sleep_ms, cpu_iters)`（CPU迭代 + 可选睡眠）
- `Scenario`/`Result` 结构，`run(Scenario)` 执行并统计，`print_result(Result)` 输出
- 命令行：`--csv`、`--repeat`、`--cases=single` 搭配 `--threads/--tasks/--sleep/--cpu/--monitor/--profile`
- 预置场景集：睡眠/CPU/优先级/延迟、8/16 线程；队列容量用 `max(tasks, 10000)`
- 分位数计算采用索引法（无 `llround/clamp` 依赖），统计引用避免拷贝原子

# 交付与验证
- 写入完整代码，无注释；默认输出人类可读，支持 CSV
- 你可直接编译运行；需要时我可再按你的偏好扩展背压策略矩阵或更大规模场景