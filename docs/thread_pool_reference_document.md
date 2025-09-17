# `Thread Pool` 模块参考文档

本文档详细介绍了 `model/module/Thread_pool.hpp` 中线程池的实现和使用方法。


## 🔧 核心类详解

### thread_pool 类

#### 类定义
```cpp
class thread_pool 
{
private:
    safety_rank_pointer _unit_rank;         // 单元队列
    safety_scheduler_pointer _scheduler;    // 调度器
    
    pool_config _config;                    // 线程池配置
    std::atomic<pool_state> _state;         // 线程池状态
    
    pool_statistics _statistics;            // 统计信息
    
    std::mutex _config_mutex;               // 配置互斥锁
    std::condition_variable _state_cv;      // 状态条件变量
    mutable std::shared_mutex _state_mutex; // 状态读写锁
    
    std::unique_ptr<std::jthread> _monitor_thread;    // 监控线程
    std::unique_ptr<std::jthread> _profiler_thread;   // 性能分析线程
    
    mutable std::shared_mutex _tasks_mutex;           // 任务映射读写锁
    std::unordered_map<std::string, std::shared_ptr<unit_ordinary>> _active_tasks; // 活跃任务映射
    std::atomic<uint64_t> _task_id_counter{0};        // 任务ID计数器
    
    std::function<void(const pool_statistics&)> _statistics_handler;              // 统计处理器
    std::function<void(const std::string&, const std::string&)> _event_handler;  // 事件处理器
    std::function<void(const performance_metrics&)> _performance_callback;        // 性能回调
    std::function<void(const std::string&)> _error_callback;                     // 错误回调
};
```

#### 生命周期管理

##### 构造函数
```cpp
explicit thread_pool(const pool_config& config = pool_config());
```

##### 启动和停止
```cpp
bool start();                                   // 启动线程池
bool stop(bool wait_for_completion = true);     // 停止线程池
bool pause();                                   // 暂停线程池
bool resume();                                  // 恢复线程池
bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds{1000}); // 优雅关闭
```

## ⚙️ 配置系统

### pool_config 结构

```cpp
struct pool_config 
{
    std::string _pool_name = "default_pool";    // 线程池名称
    std::size_t _min_threads = 1;               // 最小线程数
    std::size_t _max_threads = 8;               // 最大线程数
    std::size_t _core_threads = 4;              // 核心线程数
    std::size_t _initial_threads = 4;           // 初始线程数
    
    std::size_t _max_queue_size = 10000;        // 最大队列大小
    rank_strategy _queue_policy = rank_strategy::fifo; // 队列策略
    
    expansion_strategy _expansion_strategy = expansion_strategy::hybrid;     // 扩缩容策略
    scheduling_tactics _scheduling_tactics = scheduling_tactics::adaptive;   // 调度策略
    
    std::chrono::milliseconds _task_timeout{30000};     // 任务超时
    std::chrono::milliseconds _idle_timeout{60000};     // 空闲超时
    std::chrono::milliseconds _shutdown_timeout{10000}; // 关闭超时
    
    bool _enable_monitoring = true;                      // 启用监控
    bool _enable_performance_profiling = false;          // 启用性能分析
    std::chrono::milliseconds _monitoring_interval{1000}; // 监控间隔
    
    bool validate() const;  // 验证配置
};
```

## 📋 任务管理

### 任务提交接口

#### 基础任务提交
```cpp
// 普通任务（有返回值）
template<typename Function, typename... Args>
auto submit(Function&& func, Args&&... args) 
    -> std::future<std::invoke_result_t<Function, Args...>>;

// 普通任务（无返回值）
template<typename Function, typename... Args>
std::size_t submit_invalid(Function&& func, Args&&... args);
```

#### 高级任务提交
```cpp
// 优先级任务
template<typename Function, typename... Args>
auto submit_priority(weight priority, Function&& func, Args&&... args)
    -> std::future<std::invoke_result_t<Function, Args...>>;

// 超时任务
template<typename Function, typename Rep, typename Period, typename... Args>
auto submit_timeout(const std::chrono::duration<Rep, Period> timeout, 
                   Function&& func, Args&&... args)
    -> std::future<std::invoke_result_t<Function, Args...>>;

// 延迟任务
template<typename Function, typename Rep, typename Period, typename... Args>
auto submit_delayed(const std::chrono::duration<Rep, Period> delay,
                   Function&& func, Args&&... args)
    -> std::future<std::invoke_result_t<Function, Args...>>;
```

### 任务控制

```cpp
bool cancel_unit(const std::string& task_id);                    // 取消单个任务
std::size_t cancel_units(const std::vector<std::string>& task_ids); // 批量取消
current_status get_unit_state(const std::string& task_id) const; // 获取任务状态
```

## 📝 使用示例

### 基础使用

```cpp
#include "model/module/Thread_pool.hpp"
using namespace pool;

int main() 
{
    // 创建线程池
    auto pool = make_thread_pool(4, 1000);
    
    // 启动线程池
    if (!pool->start()) 
    {
        std::cerr << "Failed to start thread pool" << std::endl;
        return -1;
    }
    
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
config._pool_name = "my_pool";
config._min_threads = 2;
config._max_threads = 8;
config._core_threads = 4;
config._enable_monitoring = true;

auto pool = make_thread_pool(config);
```

### 监控和回调

```cpp
// 设置事件处理器
pool->set_event_handler([](const std::string& category, const std::string& message) 
{
    std::cout << "[" << category << "] " << message << std::endl;
});

// 设置性能回调
pool->set_performance_callback([](const thread_pool::performance_metrics& metrics) 
{
    std::cout << "Throughput: " << metrics.throughput << " tasks/sec" << std::endl;
});
```

### 多种任务类型

```cpp
// 优先级任务
auto high_priority = pool->submit_priority(weight::high, []() 
{
    return "High priority task";
});

// 超时任务
auto timeout_task = pool->submit_timeout(std::chrono::seconds(5), []() 
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return "Completed within timeout";
});

// 延迟任务
auto delayed_task = pool->submit_delayed(std::chrono::seconds(2), []() 
{
    return "Delayed task executed";
});
```

## 🏭 工厂函数

```cpp
// 标准线程池
std::unique_ptr<thread_pool> make_thread_pool(std::size_t thread_count, std::size_t queue_size = 10000);

// 高性能线程池
std::unique_ptr<thread_pool> make_performance_pool(std::size_t thread_count);

// 轻量级线程池
std::unique_ptr<thread_pool> make_lightweight_pool(std::size_t thread_count);
```

---
