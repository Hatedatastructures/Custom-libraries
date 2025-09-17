#include "Unit.hpp"
#include "Integration.hpp"
#include "Rank.hpp"
#include "Worker.hpp"
#include "Scheduling.hpp"
#include <iostream>

namespace internals
{
  using namespace structure_t;
}
namespace internals::structure_t
{
  using namespace internals::structure_s;
  using namespace internals::structure_r;
  using namespace internals::structure_u;
  using namespace internals::structure_w;
  using safety_scheduler_pointer = std::unique_ptr<scheduler_ordinary>;
  class thread_pool
  {
  private:
    safety_rank_pointer _unit_rank; //单元队列
    safety_scheduler_pointer _scheduler; //调度器

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
    std::unordered_map<std::string, std::shared_ptr<unit_ordinary>> _active_tasks; // 活跃任务映射

    // 扩展和插件
    mutable std::mutex _plugins_mutex; // 插件互斥锁
    std::unordered_map<std::string, std::function<void()>> _plugins; // 插件映射

    // 性能分析
    std::atomic<bool> _profiling_enabled{false};   // 性能分析启用标志
    std::unique_ptr<std::jthread> _profiler_thread; // 性能分析线程
  public:
    explicit thread_pool(const pool_config &config = pool_config()) : _config(config)
    {
      if(!_config.validate())
        throw std::invalid_argument("Invalid thread pool configuration");
      initialize();
    }
    ~thread_pool()
    {
      shutdown(std::chrono::milliseconds{500});
    }
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
        return false;

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
          start_monitoring();

        // 启动性能分析
        if (_config.enable_performance_profiling)
          start_profiling();

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
        _unit_rank->close();

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
     * @note `false` 表示线程池当前状态不是 `running`，无法暂停
     */
    bool pause()
    {
      std::unique_lock<std::shared_mutex> lock(_state_mutex);

      if (_state.load() != pool_state::running)
        return false;

      _state.store(pool_state::pausing);

      // 暂停任务队列
      _unit_rank->close();

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

      // 重新建立任务队列
      // 注意：这里需要重新创建队列，因为close()可能是不可逆的

      while(!_unit_rank->empty())
      {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
      if(_unit_rank->size() == 0)
      {
        _unit_rank.reset();
        _unit_rank = make_rank(_config.queue_policy,_config.max_queue_size)
      }
      else  return false;
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
        return false;

      initialize();
      return start();
    }
    /**
     * @brief 优雅关闭线程池
     * @param timeout 超时时间
     * @return `true` 关闭成功，`false` 关闭超时
     */
    bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds{1000})
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
     * @return 异步阻塞式容器`future`
     */
    template <typename function, typename... Args>
    auto submit(function &&func, Args &&...args) -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
        throw std::runtime_error("Thread pool is not running");
      auto task = make_unit_standard(std::bind(std::forward<function>(func), std::forward<Args>(args)...));

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
        throw std::runtime_error("Failed to submit task");
      return future;
    }
    /**
     * @brief 提交普通任务(无返回值)
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务ID
     */
    template <typename function, typename... Args>
    std::size_t submit_v(function &&func, Args &&...args)
    {
      if (!is_running())
        throw std::runtime_error("Thread pool is not running");

      auto task = make_unit_standard(std::bind(std::forward<function>(func), std::forward<Args>(args)...));

      auto task_id = task->get_identifier();

      if (!submit_task_internal(task))
        throw std::runtime_error("Failed to submit task");

      return task_id;
    }
    /**
     * @brief 提交优先级任务
     * @param priority 任务优先级
     * @param func 任务函数
     * @param args 函数参数
     * @return future
     */
    template <typename function, typename... Args>
    auto submit_priority(weight priority, function &&func, Args &&...args) -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
        throw std::runtime_error("Thread pool is not running");

      auto task = make_unit_standard(std::bind(std::forward<function>(func), std::forward<Args>(args)...),priority);

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
        throw std::runtime_error("Failed to submit priority task");
      return future;
    }
    /**
     * @brief 提交超时任务
     * @param timeout 超时时间
     * @param func 任务函数
     * @param args 函数参数
     * @return future
     */
    template <typename function,typename rep, typename period, typename... Args>
    auto submit_timeout(const std::chrono::duration<rep, period> timeout, function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
        throw std::runtime_error("Thread pool is not running");

      auto task = make_task_time(std::bind(std::forward<function>(func), std::forward<Args>(args)...),timeout);

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
        throw std::runtime_error("Failed to submit timeout task");

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

      auto task = make_unit_overtime(std::bind(std::forward<function>(func), std::forward<Args>(args)...), delay);

      auto future = std::move(task->get_future());

      // 使用延迟队列
      if (_unit_rank)
      {
        _unit_rank->push(task);
      }
      else
      {
        // 回退到普通提交
        if (!submit_task_internal(task))
          throw std::runtime_error("Failed to submit delayed task");
      }

      return future;
    }
  private:
    void initialize()
    {
      // 创建任务队列
      _unit_rank = make_rank(_config.queue_policy, _config.max_queue_size);
      
      // 创建调度器
      _scheduler = make_scheduler_ordinary("adaptive", _unit_rank, 
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
    bool submit_task_internal(safety_unit_pointer task)
    {
      if (!task)
        return false;

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
      auto queue_size = _unit_rank->size();
      _statistics.current_queue_size.store(queue_size, std::memory_order_relaxed);

      auto peak_queue = _statistics.peak_queue_size.load(std::memory_order_relaxed);
      if (queue_size > peak_queue)
      {
        _statistics.peak_queue_size.store(queue_size, std::memory_order_relaxed);
      }

      // 计算吞吐量
      calculate_throughput();
    }
    //计算吞吐量
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
          if (_active_tasks.empty() && _unit_rank->empty())
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
     * @param dependencies 依赖的任务列表
     * @param func 任务函数
     * @param args 函数参数
     * @return 任务future
     */
    template <typename function, typename... Args>
    auto submit_reliance(const std::vector<safety_unit_pointer> &reliance, function &&func, Args &&...args)
      -> std::future<std::invoke_result_t<function, Args...>>
    {

      if (!is_running())
        throw std::runtime_error("Thread pool is not running");

      auto task = make_unit_reliance(std::bind(std::forward<function>(func), std::forward<Args>(args)...),reliance);

      auto future = std::move(task->get_future());

      if (!submit_task_internal(task))
        throw std::runtime_error("Failed to submit dependency task");
      return future;
    }
     /**
     * @brief 批量提交任务
     * @param tasks 任务列表容器
     * @return 成功提交的任务数量
     */
    template <typename task_container>
    std::size_t submit_batch(const task_container &tasks)
    {
      if (!is_running())
        throw std::runtime_error("Thread pool is not running");

      std::size_t submitted_count = 0;

      for(const auto &task : tasks)
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
     * @param funcs 函数列表容器
     * @return `future` `vector`数组
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
      return _unit_rank->size();
    }

    /**
     * @brief 检查队列是否为空
     * @return true 队列为空，false 队列非空
     */
    bool is_queue_empty() const
    {
      return _unit_rank->empty();
    }

    /**
     * @brief 清空任务队列
     * @return 清除的任务数量
     */
    std::size_t clear_queue()
    {
      auto size = _unit_rank->size();
      _unit_rank->clear();

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
      auto transition_state = std::dynamic_pointer_cast<rank_ordinary>(_unit_rank);
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
     * @brief 重置统计信息
     */
    void reset_statistics()
    {
      _statistics.reset();
      emit_event("monitoring", "Statistics reset");
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

          _scheduler = make_scheduler("adaptive", _unit_rank,_config.scheduling_tactics, _config.expansion_strategy);
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
  }
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
    config.queue_policy       = rank_strategy::fifo;
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
    config.queue_policy         = rank_strategy::priority;
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
    config.min_threads        = thread_count;
    config.max_threads        = thread_count;
    config.core_threads       = thread_count;
    config.queue_policy       = rank_strategy::fifo;
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
namespace pool
{
 using namespace internals::structure_t;
}
