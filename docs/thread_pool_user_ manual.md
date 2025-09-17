# 线程池使用指南

本文档介绍了线程池的使用方法、配置选项和最佳实践。



### 基本使用

```cpp
#include "model/module/Thread_pool.hpp"
using namespace pool;

int main() 
{
    // 创建线程池 (4个线程，队列大小1000)
    auto pool = make_thread_pool(4, 1000);
    
    // 启动线程池
    pool->start();
    
    // 提交任务
    auto future = pool->submit([]() 
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 42;
    });
    
    // 获取结果
    int result = future.get();
    std::cout << "Result: " << result << std::endl;
    
    // 关闭线程池
    pool->shutdown();
    
    return 0;
}
```

### 配置化创建

```cpp
pool_config config;
config._pool_name = "thread_pool";
config._min_threads = 2;
config._max_threads = 8;
config._core_threads = 4;
config._initial_threads = 4;
config._max_queue_size = 10000;
config._enable_monitoring = true;

auto pool = make_thread_pool(config);
pool->start();
```

## ⚙️ 配置选项

### 基础配置

```cpp
pool_config config;

// 线程配置
config._min_threads = 1;        // 最小线程数
config._max_threads = 16;       // 最大线程数
config._core_threads = 4;       // 核心线程数
config._initial_threads = 4;    // 初始线程数

// 队列配置
config._max_queue_size = 10000; // 最大队列大小
config._queue_policy = rank_strategy::fifo; // 队列策略

// 调度配置
config._scheduling_tactics = scheduling_tactics::adaptive;
config._expansion_strategy = expansion_strategy::hybrid;
```

### 超时配置

```cpp
config._task_timeout = std::chrono::milliseconds(30000);     // 任务超时
config._idle_timeout = std::chrono::milliseconds(60000);     // 线程空闲超时
config._shutdown_timeout = std::chrono::milliseconds(10000); // 关闭超时
```

### 监控配置

```cpp
config._enable_monitoring = true;                           // 启用监控
config._enable_performance_profiling = true;                // 启用性能分析
config._monitoring_interval = std::chrono::milliseconds(1000); // 监控间隔
```

## 📋 任务类型

### 1. 普通任务

```cpp
// 有返回值的任务
auto future = pool->submit([]() 
{
    return 42;
});
int result = future.get();

// 无返回值的任务
std::size_t task_id = pool->submit_invalid([]() 
{
    std::cout << "Task executed" << std::endl;
});
```

### 2. 优先级任务

```cpp
// 高优先级任务
auto future = pool->submit_priority(weight::high, []() 
{
    return "High priority task";
});

// 关键优先级任务
auto critical_future = pool->submit_priority(weight::critical, []() 
{
    return "Critical task";
});
```

### 3. 超时任务

```cpp
// 设置任务超时时间
auto future = pool->submit_timeout(
    std::chrono::seconds(5),  // 5秒超时
    []() 
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return "Completed within timeout";
    }
);
```

### 4. 延迟任务

```cpp
// 延迟执行的任务
auto future = pool->submit_delayed(
    std::chrono::seconds(2),  // 延迟2秒执行
    []() 
    {
        return "Delayed task executed";
    }
);
```

## 📊 性能监控

### 设置监控回调

```cpp
// 设置事件处理器
pool->set_event_handler([](const std::string& category, const std::string& message) 
{
    std::cout << "[" << category << "] " << message << std::endl;
});

// 设置统计处理器
pool->set_statistics_handler([](const pool_statistics& stats) 
{
    std::cout << "Completed tasks: " << stats._total_tasks_completed.load() << std::endl;
    std::cout << "Current throughput: " << stats._current_throughput.load() << std::endl;
});

// 设置性能回调
pool->set_performance_callback([](const thread_pool::performance_metrics& metrics) 
{
    std::cout << "Throughput: " << metrics.throughput << " tasks/sec" << std::endl;
    std::cout << "Queue utilization: " << metrics.queue_utilization << std::endl;
});
```

### 获取统计信息

```cpp
const auto& stats = pool->get_statistics();

std::cout << "Total submitted: " << stats._total_tasks_submitted.load() << std::endl;
std::cout << "Total completed: " << stats._total_tasks_completed.load() << std::endl;
std::cout << "Success rate: " << stats.calculate_success_rate() << std::endl;
std::cout << "Uptime: " << stats.calculate_uptime() << " seconds" << std::endl;
```

### 健康检查

```cpp
// 检查线程池健康状态
if (pool->health_check()) 
{
    std::cout << "Thread pool is healthy" << std::endl;
} 
else 
{
    std::cout << "Thread pool has issues" << std::endl;
    
    // 尝试自动修复
    if (pool->auto_repair()) 
    {
        std::cout << "Auto repair successful" << std::endl;
    } 
    else 
    {
        std::cout << "Auto repair failed" << std::endl;
    }
}
```

## 🎯 最佳实践

### 1. 线程数量配置

```cpp
std::size_t cpu_cores = std::thread::hardware_concurrency();

pool_config config;
config._min_threads = 1;
config._core_threads = cpu_cores;
config._max_threads = cpu_cores * 2;  // CPU密集型任务
// config._max_threads = cpu_cores * 4;  // I/O密集型任务
```

### 2. 任务设计原则

```cpp
// ✅ 好的任务设计
auto good_task = pool->submit([]() 
{
    // 任务逻辑简洁明确
    auto result = expensive_computation();
    return result;
});

// ❌ 避免的任务设计
auto bad_task = pool->submit([&pool]() 
{
    // 避免在任务中访问线程池本身
    // 避免长时间阻塞操作
    // 避免抛出未处理的异常
});
```

### 3. 异常处理

```cpp
try 
{
    auto future = pool->submit([]() 
    {
        // 可能抛出异常的代码
        if (some_condition) 
            throw std::runtime_error("Task failed");
        return 42;
    });
    
    int result = future.get();  // 异常会在这里重新抛出
} 
catch (const std::exception& e) 
{
    std::cerr << "Task failed: " << e.what() << std::endl;
}
```

## 🏭 工厂函数

### 标准线程池
```cpp
auto pool = make_thread_pool(4, 1000);  // 4线程，队列大小1000
```

### 高性能线程池
```cpp
auto pool = make_performance_pool(8);   // 启用监控和性能分析
```

### 轻量级线程池
```cpp
auto pool = make_lightweight_pool(2);   // 最小功能集
```

---
