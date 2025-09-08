#pragma once
#include "Task.hpp"
#include "Cohort.hpp"
#include "Worker.hpp"
#include "Scheduler.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <fstream>

namespace internals
{
  using namespace internals;
  using namespace internals::structure_u;
  using namespace internals::structure_c;
  using namespace internals::structure_w;
  using namespace internals::structure_s;
  /**
   * @enum pool_state
   * @brief 线程池状态枚举
   */
  enum class pool_state
  {
    stopped,  // 已停止
    starting, // 启动中
    running,  // 运行中
    pausing,  // 暂停中
    paused,   // 已暂停
    stopping, // 停止中
    error     // 错误状态
  };

  /**
   * @struct pool_config
   * @brief 线程池配置结构
   *
   * 定义线程池的各种配置参数
   */
  class pool_config
  {
  public:
    // 基础配置
    std::string pool_name = "default_pool"; // 线程池名称

    std::size_t min_threads = 1; // 最小线程数
    std::size_t max_threads = 64; // 最大线程数
    std::size_t core_threads = 4; // 核心线程数
    std::size_t initial_threads = 4; // 初始线程数

    // 队列配置
    std::size_t max_queue_size = 10000; // 最大队列大小
    cohort_strategy queue_policy = cohort_strategy::fifo; // 队列策略

    // 调度配置
    expansion_strategy expansion_strategy = expansion_strategy::hybrid; // 扩缩容策略
    scheduling_tactics scheduling_tactics = scheduling_tactics::adaptive; // 调度策略
    

    // 超时配置
    std::chrono::milliseconds task_timeout{30000}; // 默认任务超时时间
    std::chrono::milliseconds idle_timeout{60000}; // 线程空闲超时时间
    std::chrono::milliseconds shutdown_timeout{10000}; // 关闭超时时间

    // 性能配置
    bool enable_work_stealing = true;        // 启用工作窃取
    bool enable_priority_inheritance = true; // 启用优先级继承

    // 监控配置
    bool enable_monitoring = true; // 启用监控
    bool enable_performance_profiling = false; // 启用性能分析
    std::chrono::milliseconds monitoring_interval{1000}; // 监控间隔

    // 日志配置
    std::string log_file_path; // 日志文件路径
    bool enable_event_logging = false; // 启用事件日志

    /**
     * @brief 验证配置有效性
     * @return true 配置有效，false 配置无效
     */
    bool validate() const
    {
      return min_threads > 0 && max_threads >= min_threads && initial_threads >= min_threads &&
      initial_threads <= max_threads && core_threads >= min_threads && core_threads <= max_threads &&
      max_queue_size > 0;
    }
  };
  /**
   * @struct pool_statistics
   * @brief 线程池统计信息结构
   *
   * 记录线程池运行时的各种统计数据
   */
  struct pool_statistics
  {
    // 基础统计
    std::atomic<std::uint64_t> total_tasks_failed{0}; // 总失败任务数
    std::atomic<std::uint64_t> total_tasks_timeout{0}; // 总超时任务数
    std::atomic<std::uint64_t> total_tasks_cancelled{0}; // 总取消任务数
    std::atomic<std::uint64_t> total_tasks_submitted{0}; // 总提交任务数
    std::atomic<std::uint64_t> total_tasks_completed{0}; // 总完成任务数

    // 性能统计
    std::atomic<double> peak_throughput{0.0}; // 峰值吞吐量(任务/秒)
    std::atomic<double> average_wait_time{0.0}; // 平均等待时间(毫秒)
    std::atomic<double> current_throughput{0.0}; // 当前吞吐量(任务/秒)
    std::atomic<double> average_task_duration{0.0}; // 平均任务执行时间(毫秒)

    // 线程统计
    std::atomic<std::size_t> idle_thread_count{0}; // 空闲线程数
    std::atomic<std::size_t> peak_thread_count{0}; // 峰值线程数
    std::atomic<std::size_t> active_thread_count{0}; // 活跃线程数
    std::atomic<std::size_t> current_thread_count{0}; // 当前线程数

    // 队列统计
    std::atomic<std::size_t> peak_queue_size{0}; // 峰值队列大小
    std::atomic<std::size_t> current_queue_size{0}; // 当前队列大小

    // 扩缩容统计
    std::atomic<std::uint64_t> total_scale_up_operations{0};   // 总扩容操作数
    std::atomic<std::uint64_t> total_scale_down_operations{0}; // 总缩容操作数

    // 时间统计
    std::chrono::steady_clock::time_point start_time; // 启动时间
    std::chrono::steady_clock::time_point last_task_time; // 最后任务时间

    /**
     * @brief 重置统计信息
     */
    void reset()
    {
      total_tasks_failed.store(0, std::memory_order_relaxed);
      total_tasks_timeout.store(0, std::memory_order_relaxed);
      total_tasks_cancelled.store(0, std::memory_order_relaxed);
      total_tasks_submitted.store(0, std::memory_order_relaxed);
      total_tasks_completed.store(0, std::memory_order_relaxed);

      peak_throughput.store(0.0, std::memory_order_relaxed);
      average_wait_time.store(0.0, std::memory_order_relaxed);
      current_throughput.store(0.0, std::memory_order_relaxed);
      average_task_duration.store(0.0, std::memory_order_relaxed);

      idle_thread_count.store(0, std::memory_order_relaxed);
      peak_thread_count.store(0, std::memory_order_relaxed);
      active_thread_count.store(0, std::memory_order_relaxed);
      current_thread_count.store(0, std::memory_order_relaxed);

      peak_queue_size.store(0, std::memory_order_relaxed);
      current_queue_size.store(0, std::memory_order_relaxed);

      total_scale_up_operations.store(0, std::memory_order_relaxed);
      total_scale_down_operations.store(0, std::memory_order_relaxed);

      last_task_time = start_time;
      start_time = std::chrono::steady_clock::now();
    }

    /**
     * @brief 计算成功率
     * @return 成功率(0.0-1.0)
     */
    double calculate_success_rate() const
    {
      auto total = total_tasks_submitted.load(std::memory_order_relaxed);
      if (total == 0)
        return 1.0;

      auto completed = total_tasks_completed.load(std::memory_order_relaxed);
      return static_cast<double>(completed) / total;
    }

    /**
     * @brief 计算运行时间
     * @return 运行时间(秒)
     */
    double calculate_uptime() const
    {
      auto now = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
      return duration.count();
    }
  };
  /**
   * @class thread_pool
   * @brief 高性能线程池类
   *
   * 这是线程池系统的核心类，提供了完整的线程池功能和丰富的接口
   *
   * 主要特性：
   * 动态扩缩容和负载均衡,
   * 多种任务类型和调度策略,
   * 完善的监控和统计功能,
   * 灵活的配置和扩展机制,
   * 高性能和高可靠性,
   * 
   * 调用关系：
   *  管理`scheduler_base`、`cohort_base`、`worker_base`等组件,被用户代码直接调用,
   *  提供各种任务提交和管理接口
   */
  class thread_pool
  {
  private:
    //封装的容器
    std::shared_ptr<cohort_base> _task_queue; // 任务队列
    std::unique_ptr<scheduler_base> _scheduler; // 任务调度器

    // 配置
    pool_config _config; // 线程池配置
    std::atomic<pool_state> _state{pool_state::stopped}; // 线程池状态

    // 统计
    pool_statistics _statistics; // 统计信息

    // 同步
    std::mutex _config_mutex; // 配置互斥锁
    std::condition_variable _state_cv; // 状态条件变量
    mutable std::shared_mutex _state_mutex; // 状态读写锁

    // 监控
    std::unique_ptr<std::jthread> _monitor_thread; // 监控线程
    std::function<void(const pool_statistics &)> _statistics_handler; // 统计处理器
    std::function<void(const std::string &, const std::string &)> _event_handler; // 事件处理器

    // 任务
    mutable std::shared_mutex _tasks_mutex; // 任务映射读写锁
    std::unordered_map<std::string, std::shared_ptr<uint_ordinary>> _active_tasks; // 活跃任务映射

    // 扩展和插件
    mutable std::mutex _plugins_mutex; // 插件互斥锁
    std::unordered_map<std::string, std::function<void()>> _plugins; // 插件映射

    // 性能分析
    std::atomic<bool> _profiling_enabled{false};   // 性能分析启用标志
    std::unique_ptr<std::jthread> _profiler_thread; // 性能分析线程
  public: 
    explicit thread_pool(const pool_config &config = pool_config())
    :_config(config)
    {
      if(!_config.validate())
      {
        throw std::invalid_argument("Invalid thread pool configuration");
      }
      initialize();
    }
    /**
     * @brief 析构函数
     */
    ~thread_pool()
    {
      shutdown(std::chrono::milliseconds{5000});
    }
    // 禁用拷贝和移动
    thread_pool(const thread_pool &) = delete;
    thread_pool &operator=(const thread_pool &) = delete;
    thread_pool(thread_pool &&) = delete;
    thread_pool &operator=(thread_pool &&) = delete;
    /**
     * @brief 启动线程池
     * @return `true` 启动成功，`false` 启动失败
     */
    bool start()
    {
      std::unique_lock<std::shared_mutex> lock(_state_mutex);

      if (_state.load() != pool_state::stopped)
      {
        return false;
      }

      _state.store(pool_state::starting);

      try
      {
        // 启动调度器
        if (!_scheduler->start(_config.initial_threads))
        {
          _state.store(pool_state::error);
          return false;
        }

        // 启动监控线程
        if (_config.enable_monitoring)
        {
          start_monitoring();
        }

        // 启动性能分析
        if (_config.enable_performance_profiling)
        {
          start_profiling();
        }

        _statistics.reset();
        _state.store(pool_state::running);
        _state_cv.notify_all();

        emit_event("lifecycle", "Thread pool started with " + std::to_string(_config.initial_threads) + " threads");

        return true;
      }
      catch (const std::exception &e)
      {
        _state.store(pool_state::error);
        emit_event("error", "Failed to start thread pool: " + std::string(e.what()));
        return false;
      }
    }
    /**
     * @brief 停止线程池
     * @param wait_for_completion 是否等待任务完成
     * @return `true` 停止成功，`false` 停止失败
     */
    bool stop(bool wait_for_completion = true)
    {
      std::unique_lock<std::shared_mutex> lock(_state_mutex);

      auto current_state = _state.load();
      if (current_state == pool_state::stopped || current_state == pool_state::stopping)
      {
        return true;
      }

      _state.store(pool_state::stopping);

      try
      {
        // 停止接受新任务
        _task_queue->close();

        // 等待任务完成或超时
        if (wait_for_completion)
        {
          wait_for_all_tasks(_config.shutdown_timeout);
        }

        // 停止调度器
        _scheduler->stop(wait_for_completion);

        // 停止监控和分析线程
        stop_monitoring();
        stop_profiling();

        _state.store(pool_state::stopped);
        _state_cv.notify_all();

        emit_event("lifecycle", "Thread pool stopped");

        return true;
      }
      catch (const std::exception &e)
      {
        _state.store(pool_state::error);
        emit_event("error", "Failed to stop thread pool: " + std::string(e.what()));
        return false;
      }
    }
    /**
     * @brief 暂停线程池
     * @return `true` 暂停成功，`false` 暂停失败
     */
    bool pause()
    {
      std::unique_lock<std::shared_mutex> lock(_state_mutex);

      if (_state.load() != pool_state::running)
      {
        return false;
      }

      _state.store(pool_state::pausing);

      // 暂停任务队列
      _task_queue->close();

      _state.store(pool_state::paused);
      _state_cv.notify_all();

      emit_event("lifecycle", "Thread pool paused");

      return true;
    }
    /**
     * @brief 恢复线程池
     * @return `true` 恢复成功，`false` 恢复失败
     */
    bool resume()
    {
      std::unique_lock<std::shared_mutex> lock(_state_mutex);

      if (_state.load() != pool_state::paused)
      {
        return false;
      }

      // 重新打开任务队列
      // 注意：这里需要重新创建队列，因为close()可能是不可逆的
      _task_queue.reset(new cohort_order(_config.max_queue_size));
      ////////////////////////////////////////////

      _state.store(pool_state::running);
      _state_cv.notify_all();

      emit_event("lifecycle", "Thread pool resumed");

      return true;
    }
    /**
     * @brief 重启线程池
     * @param wait_for_completion 是否等待任务完成
     * @return `true` 重启成功，`false` 重启失败
     */
    bool restart(bool wait_for_completion = true)
    {
      if (!stop(wait_for_completion))
      {
        return false;
      }

      // 重新初始化
      initialize();

      return start();
    }
    /**
     * @brief 优雅关闭线程池
     * @param timeout 超时时间
     * @return `true` 关闭成功，`false` 关闭超时
     */
    bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds{10000})
    {
      std::this_thread::sleep_for(timeout);
      return stop(true);
    }
    /**
     * @brief 强制关闭线程池
     * @return true 关闭成功，false 关闭失败
     */
    bool force_shutdown()
    {
      return stop(false);
    }
    /**
     * @brief 提交普通任务
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务`future`
     */
    template <typename function, typename... Args>
    auto submit(function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_rslt(std::bind(std::forward<function>(func), std::forward<Args>(args)...));

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
      {
        throw std::runtime_error("Failed to submit task");
      }

      return future;
    }
    /**
     * @brief 提交普通任务(无返回值)
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务ID
     */
    template <typename function, typename... Args>
    std::size_t submit_void(function &&func, Args &&...args)
    {
      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_norm(std::bind(std::forward<function>(func), std::forward<Args>(args)...));

      auto task_id = task->get_identifier();

      if (!submit_task_internal(task))
      {
        throw std::runtime_error("Failed to submit task");
      }

      return task_id;
    }

    /**
     * @brief 提交优先级任务
     * @param priority 任务优先级
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务future
     */
    template <typename function, typename... Args>
    auto submit_priority(urgency_level priority, function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_prio(std::bind(std::forward<function>(func), std::forward<Args>(args)...),priority);

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
      {
        throw std::runtime_error("Failed to submit priority task");
      }
      return future;
    }

    /**
     * @brief 提交超时任务
     * @param timeout 超时时间
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务future
     */
    template <typename function,typename rep, typename period, typename... Args>
    auto submit_timeout(const std::chrono::duration<rep, period> timeout, function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_time(std::bind(std::forward<function>(func), std::forward<Args>(args)...),timeout);

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
      {
        throw std::runtime_error("Failed to submit timeout task");
      }

      return future;
    }

    /**
     * @brief 提交延迟任务
     * @param delay 延迟时间
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务future
     */
    template <typename function,typename rep, typename period, typename... Args>
    auto submit_delayed(const std::chrono::duration<rep, period> delay, function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_time(std::bind(std::forward<function>(func), std::forward<Args>(args)...), delay);

      auto future = std::move(task->get_future());

      // 使用延迟队列
      if (_task_queue)
      {
        _task_queue->push(task);
      }
      else
      {
        // 回退到普通提交
        if (!submit_task_internal(task))
        {
          throw std::runtime_error("Failed to submit delayed task");
        }
      }

      return future;
    }
  private:
    /**
     * @brief 初始化线程池
     */
    void initialize()
    {
      // 创建任务队列
      _task_queue = make_cohort(_config.queue_policy, _config.max_queue_size);
      
      // 创建调度器
      _scheduler = make_scheduler("adaptive", _task_queue, 
      _config.scheduling_tactics, _config.expansion_strategy);
      
      // 设置调度器配置
      scaling_config scaling_cfg;
      scaling_cfg.min_threads = _config.min_threads;
      scaling_cfg.max_threads = _config.max_threads;
      scaling_cfg.core_threads = _config.core_threads;
      _scheduler->set_scaling_config(scaling_cfg);
      
      auto event_callback = [this](const std::string& event) 
      {
        emit_event("scheduler", event);
      };
      _scheduler->set_event_callback(event_callback);
      
      _statistics.reset();
    }
    /**
     * @brief 内部任务提交
     * @param task 任务
     * @return 提交结果
     */
    bool submit_task_internal(_interior_task_ptr task)
    {
      if (!task)
      {
        return false;
      }

      // 添加到活跃任务映射
      {
        std::unique_lock<std::shared_mutex> lock(_tasks_mutex);
        _active_tasks[std::to_string(task->get_identifier())] = task;
      }

      // 提交到调度器
      bool result = _scheduler->submit_task(task);

      if (result)
      {
        _statistics.total_tasks_submitted.fetch_add(1, std::memory_order_relaxed);
        _statistics.last_task_time = std::chrono::steady_clock::now();
      }
      else
      {
        // 提交失败，从活跃任务中移除
        std::unique_lock<std::shared_mutex> lock(_tasks_mutex);
        _active_tasks.erase(std::to_string(task->get_identifier()));
      }
      return result;
    }
    /**
     * @brief 启动监控
     */
    void start_monitoring()
    {
      auto monitoring_functions = [this]()
      {
        while (_state.load() == pool_state::running)
        {
          update_statistics();
          if (_statistics_handler)
          {
            _statistics_handler(_statistics);
          }
          std::this_thread::sleep_for(_config.monitoring_interval);
        }
      };
      _monitor_thread = std::make_unique<std::jthread>(std::move(monitoring_functions));
    }
    /**
     * @brief 停止监控
     */
    void stop_monitoring()
    {
      if (_monitor_thread && _monitor_thread->joinable())
      {
        _monitor_thread->join();
      }
    }
    /**
     * @brief 启动性能分析
     */
    void start_profiling()
    {
      _profiling_enabled.store(true);
      auto performance_analysis = [this]()
      {
        while (_profiling_enabled.load())
        {
          // 收集性能数据,回传到网络
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      };
      _profiler_thread = std::make_unique<std::jthread>(std::move(performance_analysis));
    }
    /**
     * @brief 停止性能分析
     */
    void stop_profiling()
    {
      _profiling_enabled.store(false);
      if (_profiler_thread && _profiler_thread->joinable())
      {
        _profiler_thread->join();
      }
    }
    /**
     * @brief 更新统计信息
     */
    void update_statistics()
    {
      // 更新线程统计
      _statistics.current_thread_count.store(_scheduler->get_thread_count(), std::memory_order_relaxed);
      _statistics.active_thread_count.store(_scheduler->get_active_thread_count(), std::memory_order_relaxed);

      // 更新队列统计
      auto queue_size = _task_queue->size();
      _statistics.current_queue_size.store(queue_size, std::memory_order_relaxed);

      auto peak_queue = _statistics.peak_queue_size.load(std::memory_order_relaxed);
      if (queue_size > peak_queue)
      {
        _statistics.peak_queue_size.store(queue_size, std::memory_order_relaxed);
      }

      // 计算吞吐量
      calculate_throughput();
    }
    /**
     * @brief 计算吞吐量
     */
    void calculate_throughput()
    {
      static auto last_time = std::chrono::steady_clock::now();
      static std::uint64_t last_completed = 0;

      auto now = std::chrono::steady_clock::now();
      auto current_completed = _statistics.total_tasks_completed.load(std::memory_order_relaxed);

      auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
      if (time_diff >= 1000) // 每秒计算一次
      {
        auto task_diff = current_completed - last_completed;
        auto throughput = static_cast<double>(task_diff) / (time_diff / 1000.0);

        _statistics.current_throughput.store(throughput, std::memory_order_relaxed);

        auto peak = _statistics.peak_throughput.load(std::memory_order_relaxed);
        if (throughput > peak)
        {
          _statistics.peak_throughput.store(throughput, std::memory_order_relaxed);
        }

        last_time = now;
        last_completed = current_completed;
      }
    }
    /**
     * @brief 等待所有任务完成
     * @param timeout 超时时间
     * @return `true` 所有任务完成，`false` 超时
     */
    bool wait_for_all_tasks(std::chrono::milliseconds timeout)
    {
      auto start_time = std::chrono::steady_clock::now();

      while (std::chrono::steady_clock::now() - start_time < timeout)
      {
        {
          std::shared_lock<std::shared_mutex> lock(_tasks_mutex);
          if (_active_tasks.empty() && _task_queue->empty())
          {
            return true;
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      return false;
    }
    /**
     * @brief 发送事件
     * @param category 事件类别
     * @param message 事件消息
     */
    void emit_event(const std::string &category, const std::string &message)
    {
      if (_event_handler)
      {
        _event_handler(category, message);
      }
    }
  public: 
    /**
     * @brief 检查线程池是否正在运行
     * @return true 正在运行，false 未运行
     */
    bool is_running() const
    {
      return _state.load() == pool_state::running;
    }

    /**
     * @brief 获取线程池状态
     * @return 当前状态
     */
    pool_state get_state() const
    {
      return _state.load();
    }

    /**
     * @brief 获取线程池配置
     * @return 配置的常量引用
     */
    const pool_config & get_config() const
    {
      return _config;
    }

    /**
     * @brief 获取统计信息
     * @return 统计信息的常量引用
     */
    const pool_statistics & get_statistics() const
    {
      return _statistics;
    }
    /**
     * @brief 提交依赖任务
     * @param dependencies 依赖的任务ID列表
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务future
     */
    template <typename function, typename... Args>
    auto submit_dependency(const std::vector<_interior_task_ptr> &dependencies, function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_depn(std::bind(std::forward<function>(func), std::forward<Args>(args)...),dependencies);

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
      {
        throw std::runtime_error("Failed to submit dependency task");
      }
      return future;
    }

    /**
     * @brief 提交协程任务
     * @param coro 协程对象
     * @return 任务ID
     */
    template <typename coroutine_type>
    std::string submit_coroutine(coroutine_type &&coro)
    {
      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      auto task = make_task_coro(std::forward<coroutine_type>(coro));
      auto task_id = std::to_string(task->get_identifier());

      if (!submit_task_internal(task))
      {
        throw std::runtime_error("Failed to submit coroutine task");
      }

      return task_id;
    }
    /**
     * @brief 批量提交任务
     * @param tasks 任务列表
     * @return 成功提交的任务数量
     */
    template <typename task_container>
    std::size_t submit_batch(const task_container &tasks)
    {
      if (!is_running())
      {
        throw std::runtime_error("Thread pool is not running");
      }

      std::size_t submitted_count = 0;

      for (const auto &task : tasks)
      {
        if (submit_task_internal(task))
        {
          ++submitted_count;
        }
      }

      return submitted_count;
    }
    /**
     * @brief 并行执行任务集合
     * @param funcs 函数列表
     * @return future列表
     */
    template <typename func_container>
    auto submit_parallel(const func_container &funcs)
      -> std::vector<std::future<std::invoke_result_t<typename func_container::value_type>>>
    {
      using return_type = std::invoke_result_t<typename func_container::value_type>;
      std::vector<std::future<return_type>> futures;
      futures.reserve(funcs.size());

      for (const auto &func : funcs)
      {
        futures.emplace_back(submit(func));
      }

      return futures;
    }
    /**
     * @brief 提交任务管道
     * @param stages 管道阶段函数列表
     * @return 最终结果的future
     */
    template <typename... stages_func>
    auto submit_pipeline(stages_func &&...stages)
      -> std::future<std::invoke_result_t<std::tuple_element_t<sizeof...(stages_func) - 1, std::tuple<stages_func...>>>>
    {
      // 实现任务管道逻辑
      // 这里简化实现，实际应该按顺序执行各个阶段
      auto last_stage = std::get<sizeof...(stages_func) - 1>(std::make_tuple(stages...));
      return submit(last_stage);
    }
    /**
     * @brief 取消任务
     * @param task_id 任务ID
     * @return true 取消成功，false 取消失败
     */
    bool cancel_task(const std::string &task_id)
    {
      std::shared_lock<std::shared_mutex> lock(_tasks_mutex);

      auto it = _active_tasks.find(task_id);
      if (it != _active_tasks.end())
      {
        auto task = it->second;
        if (task->cancel())
        {
          _statistics.total_tasks_cancelled.fetch_add(1, std::memory_order_relaxed);
          return true;
        }
      }
      return false;
    }
    /**
     * @brief 批量取消任务
     * @param task_ids 任务ID列表
     * @return 成功取消的任务数量
     */
    std::size_t cancel_tasks(const std::vector<std::string> &task_ids)
    {
      std::size_t cancelled_count = 0;

      for (const auto &task_id : task_ids)
      {
        if (cancel_task(task_id))
        {
          ++cancelled_count;
        }
      }

      return cancelled_count;
    }
    /**
     * @brief 取消所有待处理任务
     * @return 取消的任务数量
     */
    std::size_t cancel_all_pending_tasks()
    {
      std::size_t cancelled_count = 0;

      std::shared_lock<std::shared_mutex> lock(_tasks_mutex);

      for (const auto &[task_id, task] : _active_tasks)
      {
        if (task->get_state() == current_status::pending && task->cancel())
        {
          ++cancelled_count;
        }
      }

      _statistics.total_tasks_cancelled.fetch_add(cancelled_count, std::memory_order_relaxed);

      return cancelled_count;
    }
    /**
     * @brief 获取任务状态
     * @param task_id 任务ID
     * @return 任务状态
     */
    current_status get_task_state(const std::string &task_id) const
    {
      std::shared_lock<std::shared_mutex> lock(_tasks_mutex);

      auto it = _active_tasks.find(task_id);
      if (it != _active_tasks.end())
      {
        return it->second->get_state();
      }

      return current_status::pending;
    }
    /**
     * @brief 等待任务完成
     * @param task_id 任务ID
     * @param timeout 超时时间
     * @return true 任务完成，false 超时
     */
    bool wait_for_task(const std::string &task_id,
    std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
    {
      std::shared_lock<std::shared_mutex> lock(_tasks_mutex);

      auto it = _active_tasks.find(task_id);
      if (it != _active_tasks.end())
      {
        return it->second->wait_for(timeout);
      }

      return false;
    }
    /**
     * @brief 等待多个任务完成
     * @param task_ids 任务ID列表
     * @param timeout 超时时间
     * @return 完成的任务数量
     */
    std::size_t wait_for_tasks(const std::vector<std::string> &task_ids,
    std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
    {
      std::size_t completed_count = 0;
      auto start_time = std::chrono::steady_clock::now();

      for (const auto &task_id : task_ids)
      {
        auto remaining_time = timeout -
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

        if (remaining_time <= std::chrono::milliseconds::zero())
        {
          break;
        }

        if (wait_for_task(task_id, remaining_time))
        {
          ++completed_count;
        }
      }

      return completed_count;
    }
    /**
     * @brief 获取活跃任务列表
     * @return 活跃任务ID列表
     */
    std::vector<std::string> get_active_task_ids() const
    {
      std::shared_lock<std::shared_mutex> lock(_tasks_mutex);

      std::vector<std::string> task_ids;
      task_ids.reserve(_active_tasks.size());

      for (const auto &[task_id, task] : _active_tasks)
      {
        task_ids.push_back(task_id);
      }

      return task_ids;
    }
    /**
     * @brief 获取任务详细信息
     * @param task_id 任务ID
     * @return 任务信息字符串
     */
    std::string get_task_info(const std::string &task_id) const
    {
      std::shared_lock<std::shared_mutex> lock(_tasks_mutex);

      auto it = _active_tasks.find(task_id);
      if (it != _active_tasks.end())
      {
        auto task = it->second;
        std::ostringstream oss;
        oss << "Task ID: "     << task_id                                            << "\n";
        oss << "State: "       << static_cast<int>(task->get_state())                << "\n";
        oss << "Priority: "    << static_cast<int>(task->get_priority())             << "\n";
        oss << "Submit Time: " << task->get_submit_time().time_since_epoch().count() << "\n";
        return oss.str();
      }
      return "Task not found";
    }
    /**
     * @brief 获取当前线程数
     * @return 线程数量
     */
    std::size_t get_thread_count() const
    {
      return _scheduler->get_thread_count();
    }

    /**
     * @brief 获取活跃线程数
     * @return 活跃线程数量
     */
    std::size_t get_active_thread_count() const
    {
      return _scheduler->get_active_thread_count();
    }

    /**
     * @brief 获取空闲线程数
     * @return 空闲线程数量
     */
    std::size_t get_idle_thread_count() const
    {
      return get_thread_count() - get_active_thread_count();
    }
    /**
     * @brief 手动扩容线程
     * @param count 增加的线程数
     * @return `true` 扩容成功，`false` 扩容失败
     */
    bool scale_up(std::size_t count)
    {
      if (!is_running())
      {
        return false;
      }

      auto current_count = get_thread_count();
      auto new_count = std::min(current_count + count, _config.max_threads);

      if (new_count > current_count)
      {
        _scheduler->manual_scale_downs(count);
        _statistics.total_scale_up_operations.fetch_add(1, std::memory_order_relaxed);

        emit_event("scaling", "Scaled up to " + std::to_string(new_count) + " threads");
        return true;
      }
      return false;
    }
    /**
     * @brief 手动缩容线程
     * @param count 减少的线程数
     * @return `true` 缩容成功，`false` 缩容失败
     */
    bool scale_down(std::size_t count)
    {
      if (!is_running())
      {
        return false;
      }

      auto current_count = get_thread_count();
      auto new_count = std::max(current_count - count, _config.min_threads);

      if (new_count < current_count)
      {
        _scheduler->manual_scale_down();
        _statistics.total_scale_down_operations.fetch_add(1, std::memory_order_relaxed);

        emit_event("scaling", "Scaled down to " + std::to_string(new_count) + " threads");
        return true;
      }

      return false;
    }
    /**
     * @brief 设置线程数
     * @param count 目标线程数
     * @return `true` 设置成功，`false`设置失败
     */
    bool set_thread_count(std::size_t count)
    {
      if (!is_running())
      {
        return false;
      }

      count = std::clamp(count, _config.min_threads, _config.max_threads);
      auto current_count = get_thread_count();

      if (count > current_count)
      {
        return scale_up(count - current_count);
      }
      else if (count < current_count)
      {
        return scale_down(current_count - count);
      }
      return true;
    }
    /**
     * @brief 获取队列大小
     * @return 队列中的任务数量
     */
    std::size_t get_queue_size() const
    {
      return _task_queue->size();
    }

    /**
     * @brief 检查队列是否为空
     * @return true 队列为空，false 队列非空
     */
    bool is_queue_empty() const
    {
      return _task_queue->empty();
    }

    /**
     * @brief 清空任务队列
     * @return 清除的任务数量
     */
    std::size_t clear_queue()
    {
      auto size = _task_queue->size();
      _task_queue->clear();

      emit_event("queue", "Cleared " + std::to_string(size) + " tasks from queue");

      return size;
    }

    /**
     * @brief 获取队列容量
     * @return 队列最大容量
     */
    std::size_t get_queue_capacity() const
    {
      return _config.max_queue_size;
    }

    /**
     * @brief 设置队列最大大小
     * @param max_size 最大大小
     * @return true 设置成功，false 设置失败
     */
    bool set_queue_max_size(std::size_t max_size)
    {
      if (max_size == 0)
      {
        return false;
      }

      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.max_queue_size = max_size;
      // 如果队列支持动态调整大小
      auto transition_state = std::dynamic_pointer_cast<cohort_order>(_task_queue);
      if(transition_state.get() != nullptr)
      {
        transition_state->set_max_size(max_size);
        return true;
      }
      return false;
    }
    /**
     * @brief 获取队列使用率
     * @return 使用率(0.0-1.0)
     */
    double get_queue_utilization() const
    {
      auto current_size = get_queue_size();
      auto max_size = get_queue_capacity();

      if (max_size == 0)
      {
        return 0.0;
      }

      return static_cast<double>(current_size) / max_size;
    }
    /**
     * @brief 更新配置
     * @param new_config 新配置
     * @return `true` 更新成功，`false` 更新失败
     */
    bool update_config(const pool_config &new_config)
    {
      if (!new_config.validate())
      {
        return false;
      }

      std::lock_guard<std::mutex> lock(_config_mutex);

      // 保存旧配置用于回滚
      auto old_config = _config;
      _config = new_config;

      try
      {
        // 应用新配置
        apply_config_changes(old_config);

        emit_event("config", "Configuration updated successfully");
        return true;
      }
      catch (const std::exception &e)
      {
        // 回滚配置
        _config = old_config;
        emit_event("error", "Failed to update configuration: " + std::string(e.what()));
        return false;
      }
    }

    /**
     * @brief 设置调度策略
     * @param policy 调度策略
     * @return `true` 设置成功，`false` 设置失败
     */
    bool set_scheduling_policy(scheduling_tactics policy)
    {
      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.scheduling_tactics = policy;

      if (_scheduler)
      {
        _scheduler->set_scheduling_policy(policy);
      }

      return true;
    }
    /**
     * @brief 设置扩缩容策略
     * @param policy 扩缩容策略
     * @return `true` 设置成功，`false` 设置失败
     */
    bool set_scaling_policy(expansion_strategy policy)
    {
      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.expansion_strategy = policy;

      if (_scheduler)
      {
        _scheduler->set_scaling_policy(policy);
      }

      return true;
    }
    /**
     * @brief 设置任务超时时间
     * @param timeout 超时时间
     */
    void set_task_timeout(std::chrono::milliseconds timeout)
    {
      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.task_timeout = timeout;
    }

    /**
     * @brief 设置线程空闲超时时间
     * @param timeout 超时时间
     */
    void set_idle_timeout(std::chrono::milliseconds timeout)
    {
      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.idle_timeout = timeout;
    }

    /**
     * @brief 启用/禁用工作窃取
     * @param enabled 是否启用
     */
    void set_work_stealing_enabled(bool enabled)
    {
      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.enable_work_stealing = enabled;
    }

    /**
     * @brief 启用/禁用监控
     * @param enabled 是否启用
     */
    void set_monitoring_enabled(bool enabled)
    {
      std::lock_guard<std::mutex> lock(_config_mutex);
      _config.enable_monitoring = enabled;

      if (is_running())
      {
        if (enabled && !_monitor_thread)
        {
          start_monitoring();
        }
        else if (!enabled && _monitor_thread)
        {
          stop_monitoring();
        }
      }
    }
    /**
     * @brief 重置统计信息
     */
    void reset_statistics()
    {
      _statistics.reset();
      emit_event("monitoring", "Statistics reset");
    }
    /**
     * @brief 获取性能报告
     * @return 性能报告字符串
     */
    std::string get_performance_report() const
    {
      std::ostringstream oss;

      oss << "=== 线程池性能报告 ===\n";
      oss << "池名称: "      << _config.pool_name                 << "\n";
      oss << "状态: "        << static_cast<int>(_state.load())   << "\n";
      oss << "正常运行时间 "  << std::fixed                        << std::setprecision(2)
          << _statistics.calculate_uptime()                       << " seconds\n";

      oss << "\n--- 任务统计 ---\n";
      oss << "提交总任务数: "     << _statistics.total_tasks_submitted.load() << "\n";
      oss << "已完成总数: "       << _statistics.total_tasks_completed.load() << "\n";
      oss << "失败总数: "         << _statistics.total_tasks_failed.load()    << "\n";
      oss << "超时总数: "         << _statistics.total_tasks_timeout.load()   << "\n";
      oss << "取消总数: "         << _statistics.total_tasks_cancelled.load() << "\n";
      oss << "成功率: "           << std::fixed << std::setprecision(2)
          << (_statistics.calculate_success_rate() * 100)                    << "%\n";

      oss << "\n--- 性能指标 ---\n";
      oss << "当前吞吐量: "       << std::fixed << std::setprecision(2)
          << _statistics.current_throughput.load()   << " 个任务/秒\n";
      oss << "峰值吞吐量: "       << std::fixed << std::setprecision(2)
          << _statistics.peak_throughput.load()      << " 个任务/秒\n";
      oss << "平均任务持续时间: " << std::fixed << std::setprecision(2)
          << _statistics.average_task_duration.load()<< " 毫秒\n";
      oss << "平均等待时间: "     << std::fixed << std::setprecision(2)
          << _statistics.average_wait_time.load()    << " 毫秒\n";

      oss << "\n--- 线程统计 ---\n";
      oss << "当前线程数: "    << _statistics.current_thread_count.load() << "\n";
      oss << "活动线程： "     << _statistics.active_thread_count.load()  << "\n";
      oss << "空闲线程: "      << _statistics.idle_thread_count.load()    << "\n";
      oss << "峰值线程数: "    << _statistics.peak_thread_count.load()    << "\n";

      oss << "\n--- 队列统计信息 ---\n";
      oss << "当前队列大小: "    << _statistics.current_queue_size.load()  << "\n";
      oss << "峰值队列大小: "    << _statistics.peak_queue_size.load()     << "\n";
      oss << "队列利用率: "      << std::fixed                             << std::setprecision(2)
          << (get_queue_utilization() * 100)                                  << "%\n";

      oss << "\n--- 缩容统计- ---\n";
      oss << "扩容次数: "   << _statistics.total_scale_up_operations.load()   << "\n";
      oss << "缩容次数: "   << _statistics.total_scale_down_operations.load() << "\n";

      return oss.str();
    }
    /**
     * @brief 导出统计数据到文件
     * @param filename 文件名
     * @return `true `导出成功，`false `导出失败
     */
    bool export_statistics(const std::string &filename) const
    {
      try
      {
        std::ofstream file(filename);
        if (!file.is_open())
        {
          return false;
        }

        file << get_performance_report();
        file.close();

        return true;
      }
      catch (const std::exception &)
      {
        return false;
      }
    }
    /**
     * @brief 获取实时负载信息
     * @return 负载信息字符串
     */
    std::string get_load_info() const
    {
      std::ostringstream oss;

      auto queue_size     = get_queue_size();
      auto thread_count   = get_thread_count();
      auto active_threads = get_active_thread_count();

      oss << "Queue: "     << queue_size     << "/" << get_queue_capacity();
      oss << ", Threads: " << active_threads << "/" << thread_count;
      oss << ", Load: "    << std::fixed     << std::setprecision(1)
          << (thread_count > 0 ? (static_cast<double>(active_threads) / thread_count * 100) : 0.0) << "%";

      return oss.str();
    }
    /**
     * @brief 设置事件处理器
     * @param handler 事件处理函数
     */
    void set_event_handler(std::function<void(const std::string &, const std::string &)> handler)
    {
      _event_handler = std::move(handler);
    }
    /**
     * @brief 设置统计处理器
     * @param handler 统计处理函数
     */
    void set_statistics_handler(std::function<void(const pool_statistics &)> handler)
    {
      _statistics_handler  = std::move(handler);
    }
    /**
     * @brief 注册插件
     * @param name 插件名称
     * @param plugin 插件函数
     * @return `true` 注册成功，`false` 注册失败
     */
    bool register_plugin(const std::string &name, std::function<void()> plugin)
    {
      std::lock_guard<std::mutex> lock(_plugins_mutex);

      if (_plugins.find(name) != _plugins.end())
      {
        return false; // 插件已存在
      }

      _plugins[name] = std::move(plugin);
      emit_event("plugin", "Registered plugin: " + name);

      return true;
    }
    /**
     * @brief 卸载插件
     * @param name 插件名称
     * @return `true` 卸载成功，`false` 卸载失败
     */
    bool unregister_plugin(const std::string &name)
    {
      std::lock_guard<std::mutex> lock(_plugins_mutex);

      auto it = _plugins.find(name);
      if (it == _plugins.end())
      {
        return false; // 插件不存在
      }

      _plugins.erase(it);
      emit_event("plugin", "Unregistered plugin: " + name);

      return true;
    }
    /**
     * @brief 执行插件
     * @param name 插件名称
     * @return `true` 执行成功，`false` 执行失败
     */
    bool execute_plugin(const std::string &name)
    {
      std::lock_guard<std::mutex> lock(_plugins_mutex);

      auto it = _plugins.find(name);
      if (it == _plugins.end())
      {
        return false; // 插件不存在
      }

      try
      {
        it->second();
        return true;
      }
      catch (const std::exception &e)
      {
        emit_event("error", "Plugin execution failed: " + std::string(e.what()));
        return false;
      }
    }
    /**
     * @brief 获取已注册插件列表
     * @return 插件名称列表
     */
    std::vector<std::string> get_registered_plugins() const
    {
      std::lock_guard<std::mutex> lock(_plugins_mutex);

      std::vector<std::string> plugin_names;
      plugin_names.reserve(_plugins.size());

      for (const auto &[name, plugin] : _plugins)
      {
        plugin_names.push_back(name);
      }

      return plugin_names;
    }
    /**
     * @brief 执行健康检查
     * @return 健康检查结果
     */
    bool health_check() const
    {
      // 检查线程池状态
      if (_state.load() != pool_state::running)
      {
        return false;
      }

      // 检查调度器状态
      if (!_scheduler || !_scheduler->is_running())
      {
        return false;
      }

      // 检查任务队列状态
      if (!_task_queue || _task_queue->closed())
      {
        return false;
      }

      // 检查线程数量
      auto thread_count = get_thread_count();
      if (thread_count < _config.min_threads || thread_count > _config.max_threads)
      {
        return false;
      }

      return true;
    }
    /**
     * @brief 获取详细健康状态
     * @return 健康状态报告
     */
    std::string get_health_status() const
    {
      std::ostringstream oss;

      oss << "=== 线程池健康检查报告 ===\n";

      // 整体状态
      bool overall_healthy = health_check();
      oss << "总体状态: " << (overall_healthy ? "健康" : "异常") << "\n";

      // 详细检查
      oss << "\n--- 组件状态 ---\n";
      oss << "池状态: "        << static_cast<int>(_state.load())                           << "\n";
      oss << "调度器运行中: "  << (_scheduler && _scheduler->is_running() ? "是" : "否")   << "\n";
      oss << "任务队列启用: "  << (_task_queue && !_task_queue->closed() ? "是" : "否") << "\n";

      oss << "\n--- 资源状况 ---\n";
      auto thread_count = get_thread_count();
      oss << "工作线程数: "      << thread_count        << " (" << _config.min_threads
          << "-"                << _config.max_threads << ")\n";
      oss << "队列长度: "        << get_queue_size()    << "/"  << get_queue_capacity() << "\n";
      oss << "队列利用率: "      << std::fixed << std::setprecision(1)
          << (get_queue_utilization() * 100)            << "%\n";

      oss << "\n--- 性能指标 ---\n";
      oss << "当前吞吐量: "   << std::fixed  << std::setprecision(2)
          << _statistics.current_throughput.load()        << " 任务/秒\n";
      oss << "成功率: "       << std::fixed  << std::setprecision(2)
          << (_statistics.calculate_success_rate() * 100) << "%\n";

      return oss.str();
    }
    /**
     * @brief 自动修复
     * @return true 修复成功，false 修复失败
     */
    bool auto_repair()
    {
      if (health_check())
      {
        return true; // 无需修复
      }

      emit_event("repair", "Starting auto repair");

      try
      {
        // 尝试重启调度器
        if (!_scheduler || !_scheduler->is_running())
        {
          if (_scheduler)
          {
            _scheduler->stop(false);
          }

          _scheduler = make_scheduler("adaptive", _task_queue,_config.scheduling_tactics, _config.expansion_strategy);
          _scheduler->start(_config.initial_threads);
        }

        // 检查线程数量
        auto thread_count = get_thread_count();
        if (thread_count < _config.min_threads)
        {
          scale_up(_config.min_threads - thread_count);
        }
        else if (thread_count > _config.max_threads)
        {
          scale_down(thread_count - _config.max_threads);
        }

        emit_event("repair", "Auto repair completed");
        return health_check();
      }
      catch (const std::exception &e)
      {
        emit_event("error", "Auto repair failed: " + std::string(e.what()));
        return false;
      }
    }
  private:
    /**
     * @brief 应用配置变更
     * @param old_config 旧配置
     */
    void apply_config_changes(const pool_config &old_config)
    {
      // 应用线程数量变更
      if (_config.initial_threads != old_config.initial_threads)
      {
        set_thread_count(_config.initial_threads);
      }

      // 应用队列大小变更
      if (_config.max_queue_size != old_config.max_queue_size)
      {
        set_queue_max_size(_config.max_queue_size);
      }

      // 应用监控设置变更
      if (_config.enable_monitoring != old_config.enable_monitoring)
      {
        set_monitoring_enabled(_config.enable_monitoring);
      }
    }
  };
  /**
   * @brief 创建标准线程池
   * @param thread_count 线程数量
   * @param queue_size 队列大小
   * @return 线程池智能指针
   */
  inline std::unique_ptr<thread_pool> make_thread_pool(std::size_t thread_count,std::size_t queue_size = 10000)
  {
    pool_config config;
    config.max_queue_size     = queue_size;
    config.initial_threads    = thread_count;
    config.min_threads        = 1;
    config.max_threads        = thread_count;
    config.core_threads       = thread_count;
    config.queue_policy       = cohort_strategy::fifo;
    config.scheduling_tactics = scheduling_tactics::round_robin;
    config.expansion_strategy = expansion_strategy::aggressive;
    config.enable_work_stealing = false;
    config.enable_monitoring    = false;
    config.enable_performance_profiling = false;
    if(config.validate())
    {
      return std::make_unique<thread_pool>(config);
    }
    return nullptr;
  }
  /**
   * @brief 创建配置化线程池
   * @param config 线程池配置
   * @return 线程池智能指针
   */
  inline std::unique_ptr<thread_pool> make_thread_pool(const pool_config &config)
  {
    if(config.validate())
    {
      return std::make_unique<thread_pool>(config);
    }
    return nullptr;
  }
  /**
   * @brief 创建高性能线程池
   * @param thread_count 线程数量
   * @return 线程池智能指针
   */
  inline std::unique_ptr<thread_pool> make_high_performance_pool(std::size_t thread_count)
  {
    pool_config config;
    config.initial_threads      = thread_count;
    config.min_threads          = thread_count;
    config.max_threads          = thread_count * 2;
    config.core_threads         = thread_count;
    config.queue_policy         = cohort_strategy::priority;
    config.scheduling_tactics   = scheduling_tactics::adaptive;
    config.expansion_strategy   = expansion_strategy::aggressive;
    config.enable_work_stealing = true;
    config.enable_monitoring    = true;
    config.enable_performance_profiling = true;

    if(config.validate())
    {
      return std::make_unique<thread_pool>(config);
    }
    return nullptr;
  }
    /**
   * @brief 创建轻量级线程池
   * @param thread_count 线程数量
   * @return 线程池智能指针
   * @warning 创建的轻量级线程池不支持动态调整线程数量，监控，性能分析等功能
   */
  inline std::unique_ptr<thread_pool> make_lightweight_pool(std::size_t thread_count)
  {
    pool_config config;
    config.initial_threads    = thread_count;
    config.min_threads        = 1;
    config.max_threads        = thread_count;
    config.core_threads       = thread_count;
    config.queue_policy       = cohort_strategy::fifo;
    config.scheduling_tactics = scheduling_tactics::round_robin;
    config.expansion_strategy = expansion_strategy::conservative;
    config.enable_work_stealing = false;
    config.enable_monitoring    = false;
    config.enable_performance_profiling = false;

    if(config.validate())
    {
      return std::make_unique<thread_pool>(config);
    }
    return nullptr;
  }
}

namespace con
{
  using internals::make_thread_pool;
  using internals::make_lightweight_pool;
  using internals::make_high_performance_pool;

  using internals::thread_pool;
  /**
   * @brief `thread_pool` 依赖类命名空间
   */
  namespace thpool
  {
    using namespace pool;
  }
}