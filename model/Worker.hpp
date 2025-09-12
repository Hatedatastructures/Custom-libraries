#pragma once
#include "./Task.hpp"
#include "./Cohort.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
namespace internals
{
  namespace structure_w
  {
    using _interior_task_ptr   = std::shared_ptr<internals::structure_u::unit_ordinary>;
    using _interior_cohort_ptr = std::shared_ptr<internals::structure_c::cohort_base>;
    /**
     * @enum worker_state
     * @brief 工作线程状态枚举
     *
     * 定义工作线程的各种状态，用于状态管理和监控
     */
    enum class worker_state
    {
      idle,     // 空闲状态 - 等待任务
      running,  // 运行状态 - 正在执行任务
      stopping, // 停止中   - 正在停止但未完全停止
      stopped,  // 已停止   - 线程已结束
      error     // 错误状态 - 发生异常
    };

    /**
     * @struct worker_statistics
     * @brief 工作线程统计信息
     *
     * 记录工作线程的性能统计数据，用于监控和优化
     */
    class worker_statistics
    {
    public:
      std::atomic<std::uint64_t> tasks_failed{0};           // 执行失败任务数量
      std::atomic<std::uint64_t> tasks_executed{0};         // 已执行任务数量
      std::atomic<std::uint64_t> total_idle_time{0};        // 总空闲时间(微秒)
      std::atomic<std::uint64_t> total_execution_time{0};   // 总执行时间(微秒)

      std::chrono::steady_clock::time_point start_time;     // 线程启动时间
      std::chrono::steady_clock::time_point last_task_time; // 最后任务执行时间

      worker_statistics()
      {
        reset();
      }

      /**
       * @brief 重置统计信息
       */
      void reset()
      {
        tasks_failed.store(0, std::memory_order_relaxed);
        tasks_executed.store(0, std::memory_order_relaxed);
        total_idle_time.store(0, std::memory_order_relaxed);
        total_execution_time.store(0, std::memory_order_relaxed);
        start_time = std::chrono::steady_clock::now();
        last_task_time = start_time;
      }

      /**
       * @brief 获取平均任务执行时间
       * @return 平均执行时间(微秒)
       */
      double get_average_execution_time() const
      {
        auto executed = tasks_executed.load(std::memory_order_relaxed);
        if (executed == 0)
          return 0.0;

        auto total_time = total_execution_time.load(std::memory_order_relaxed);
        return static_cast<double>(total_time) / executed;
      }

      /**
       * @brief 获取任务成功率
       * @return 成功率(0.0-1.0)
       */
      double get_success_rate() const
      {
        auto executed = tasks_executed.load(std::memory_order_relaxed);
        if (executed == 0)
          return 1.0;

        auto failed = tasks_failed.load(std::memory_order_relaxed);
        return static_cast<double>(executed - failed) / executed;
      }

      /**
       * @brief 获取线程利用率
       * @return 利用率(0.0-1.0)
       */
      double get_utilization() const
      {
        auto now = std::chrono::steady_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();

        if (total_time == 0)
          return 0.0;

        auto execution_time = total_execution_time.load(std::memory_order_relaxed);
        return static_cast<double>(execution_time) / total_time;
      }
    };
    /**
     * @class base_worker
     * @brief 工作线程基类
     *
     * 定义工作线程的基本接口和行为，所有具体的工作线程类型都继承自此类
     *
     * 设计模式： 模板方法模式：定义线程执行流程，策略模式：支持不同的任务获取策略
     *
     * 调用关系：被`thread_pool`管理和调用， 从`cohort_struct`获取任务， 执行`base_task`及其派生类
     */
    class worker_base
    {
    protected:
      std::unique_ptr<std::jthread> _worker_thread; // 线程对象

      std::atomic<bool> _stop{false}; // 停止标志
      std::atomic<bool> _detached{false}; // 分离标志
      std::atomic<worker_state> _state{worker_state::idle}; // 状态标志

      std::string _worker_name; // 线程名称
      worker_statistics _statistics; // 统计信息

      std::shared_mutex _state_mutex; // 状态互斥锁
      std::condition_variable _condition; // 条件变量

      _interior_cohort_ptr _task_queue; // 任务队列

      std::function<void(const std::string&, _interior_task_ptr&)> _starts_callback; // 任务开始回调
      std::function<void(const std::string&, _interior_task_ptr&)> _finish_callback; // 任务完成回调

      std::function<void()> _worker_starts_callback; // 线程开始回调
      std::function<void()> _worker_finish_callback; // 线程完成回调

      std::function<void(const std::string&, const std::exception&)> _abnormal_callback; // 任务异常回调 
    public:
      /**
       * @brief 构造工作线程
       * @param worker_name 线程ID
       * @param cohort_struct 任务队列
       */
      worker_base(const std::string &worker_name,_interior_cohort_ptr cohort_struct)
        : _worker_name(worker_name), _task_queue(std::move(cohort_struct)) {}
      /**
       * @brief 虚析构函数
       */
      virtual ~worker_base()
      {
        stop();
        if (_worker_thread && _worker_thread->joinable())
        {
          _worker_thread->join();
        }
      }
      // 禁用拷贝和移动
      worker_base(const worker_base &) = delete;
      worker_base &operator=(const worker_base &) = delete;
      worker_base(worker_base &&) = delete;
      worker_base &operator=(worker_base &&) = delete;
      /**
       * @brief 启动工作线程
       * @return `true` 启动成功，`false` 启动失败
       */
      virtual bool start()
      {
        std::unique_lock<std::shared_mutex> lock(_state_mutex);

        if (_state.load(std::memory_order_acquire) != worker_state::idle)
        {
          return false;
        }

        try
        {
          _stop.store(false, std::memory_order_release);
          _worker_thread = std::make_unique<std::jthread>(&worker_base::interior_run, this);
          _state.store(worker_state::running, std::memory_order_release);
          _statistics.start_time = std::chrono::steady_clock::now();

          lock.unlock();
          _condition.notify_all();
          return true;
        }
        catch (const std::exception &e)
        {
          _state.store(worker_state::error, std::memory_order_release);
          if (_abnormal_callback)
          {
            _abnormal_callback(_worker_name, e);
          }
          return false;
        }
      }
      /**
       * @brief 停止工作线程
       * @param wait_for_completion 是否等待当前任务完成
       */
      virtual void stop(bool wait_for_completion = true)
      {
        _stop.store(true, std::memory_order_release);

        {
          std::unique_lock<std::shared_mutex> lock(_state_mutex);
          _state.store(worker_state::stopping, std::memory_order_release);
        }
        _condition.notify_all();

        if (_worker_thread && _worker_thread->joinable() && wait_for_completion)
        {
          _worker_thread->join();
        }
      }
      /**
       * @brief 分离工作线程
       */
      virtual void detach()
      {
        if (_worker_thread && _worker_thread->joinable())
        {
          _worker_thread->detach();
          _detached.store(true, std::memory_order_release);
        }
      }
      /**
       * @brief 等待线程结束
       * @param timeout 超时时间
       * @return `true` 线程已结束，`false` 超时
       */
      template <typename rep, typename period>
      bool wait_for_stop(const std::chrono::duration<rep, period> &timeout)
      {
        std::shared_lock<std::shared_mutex> lock(_state_mutex);
        auto state_function = [this]()
        {
          auto state = _state.load(std::memory_order_acquire);
          return state == worker_state::stopped || state == worker_state::error;
        };
        return _condition.wait_for(lock, timeout, state_function);
      }
      /**
       * @brief 获取工作线程ID
       * @return 线程ID
       */
      const std::string& get_worker_name() const
      {
        return _worker_name;
      }

      /**
       * @brief 获取线程状态
       * @return 当前状态
       */
      worker_state get_state() const
      {
        return _state.load(std::memory_order_acquire);
      }

      /**
       * @brief 检查线程是否正在运行
       * @return `true` 正在运行，`false` 未运行
       */
      bool is_running() const
      {
        auto state = _state.load(std::memory_order_acquire);
        return state == worker_state::running;
      }

      /**
       * @brief 检查线程是否已停止
       * @return true 已停止，false 未停止
       */
      bool is_stopped() const
      {
        auto state = _state.load(std::memory_order_acquire);
        return state == worker_state::stopped;
      }

      /**
       * @brief 获取统计信息
       * @return 统计信息的常量引用
       */
      const worker_statistics &get_statistics() const
      {
        return _statistics;
      }

      /**
       * @brief 重置统计信息
       */
      void reset_statistics()
      {
        _statistics.reset();
      }
      /**
       * @brief 设置错误处理回调
       * @param handler 错误处理函数
       */
      void set_abnormal_callback(std::function<void(const std::string&, const std::exception& )> handler)
      {
        _abnormal_callback = std::move(handler);
      }
      /**
       * @brief 设置任务开始回调
       * @param callback 任务开始回调函数
       */
      void set_start_callback(std::function<void(const std::string &,_interior_task_ptr)> callback)
      {
        _starts_callback = std::move(callback);
      }
      /**
       * @brief 设置任务结束回调
       * @param callback 任务结束回调函数
       */
      void set_finish_callback(std::function<void(const std::string &,_interior_task_ptr)> callback)
      {
        _finish_callback = std::move(callback);
      }
      /**
       * @brief 获取系统线程ID
       * @return 系统线程ID
       */
      std::thread::id get_thread_id() const
      {
        if (_worker_thread)
        {
          return _worker_thread->get_id();
        }
        return std::thread::id{};
      }
      /**
       * @brief 设置线程开始时回调
       */
      void on_thread_start(std::function<void()> callback)
      {
        _worker_starts_callback = std::move(callback);
      }
      /**
       * @brief 设置线程结束时回调
       */
      void on_thread_stop(std::function<void()> callback)
      {
        _worker_finish_callback = std::move(callback);
      }
    protected:
      /**
       * @brief 线程主循环 - 模板方法
       *
       * 定义工作线程的执行流程，子类可以重写特定步骤
       */
      virtual void interior_run()
      {
        try
        {
          on_thread_start();
          while (!_stop.load(std::memory_order_acquire))
          {
            auto task = get_next_task();
            if (task)
            {
              execute_task(task);
            }
            else
            {
              handle_no_task();
            }
          }
          on_thread_stop();
        }
        catch (const std::exception &e)
        {
          _state.store(worker_state::error, std::memory_order_release);
          if (_abnormal_callback)
          {
            _abnormal_callback(_worker_name, e);
          }
          else
          {
            throw;
          }
        }

        {
          std::unique_lock<std::shared_mutex> lock(_state_mutex);
          _state.store(worker_state::stopped, std::memory_order_release);
        }
        _condition.notify_all();
      }
      /**
       * @brief 获取下一个任务 - 策略方法
       * @return 任务智能指针，无任务时返回`nullptr`
       */
      virtual _interior_task_ptr get_next_task()
      {
        if (!_task_queue)
        {
          return nullptr;
        }

        return _task_queue->pop();
      }
      /**
       * @brief 执行任务
       * @param task 要执行的任务
       */
      virtual void execute_task(_interior_task_ptr task)
      {
        if (!task)
        {
          return;
        }

        auto start_time = std::chrono::steady_clock::now();

        try
        {
          // 任务开始回调
          if (_starts_callback)
          {
            _starts_callback(_worker_name, task);
          }

          // 检查任务是否已超时
          if (task->is_timeout() == false && task->has_deadline())
          {
            task->mark_timeout();
            _statistics.tasks_failed.fetch_add(1, std::memory_order_relaxed);
            return;
          }

          // 执行任务
          task->execute();

          // 更新统计信息
          auto end_time = std::chrono::steady_clock::now();
          auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

          _statistics.tasks_executed.fetch_add(1, std::memory_order_relaxed);
          _statistics.total_execution_time.fetch_add(execution_time, std::memory_order_relaxed);
          _statistics.last_task_time = end_time;

          // 任务结束回调
          if (_finish_callback)
          {
            _finish_callback(_worker_name, task);
          }
        }
        catch (const std::exception &e)
        {
          _statistics.tasks_failed.fetch_add(1, std::memory_order_relaxed);

          if (_abnormal_callback)
          {
            _abnormal_callback(_worker_name, e);
          }
        }
      }
      /**
       * @brief 处理无任务情况
       */
      virtual void handle_no_task()
      {
        // 更新空闲时间统计
        auto idle_start = std::chrono::steady_clock::now();

        // 休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        auto idle_end = std::chrono::steady_clock::now();
        auto idle_time = std::chrono::duration_cast<std::chrono::microseconds>(idle_end - idle_start).count();
        _statistics.total_idle_time.fetch_add(idle_time, std::memory_order_relaxed);
      }
      /**
       * @brief 线程启动时调用
       */
      virtual void on_thread_start()
      {
        // 派生类重写此方法进行初始化
        if(_worker_starts_callback)
        {
          _worker_starts_callback();
        }
      }
      /**
       * @brief 线程停止时调用
       */
      virtual void on_thread_stop()
      {
        // 派生类重写此方法进行清理
        if(_worker_finish_callback)
        {
          _worker_finish_callback();
        }
      }
    };
    /**
     * @class worker_standard
     * @brief 标准工作线程实现
     *
     * 标准的工作线程实现，适用于大多数场景 :简单高效的任务执行,支持任务超时检查,完善的异常处理
     *
     * 调用关系：继承自`worker_baser`, 被`thread_pool`创建和管理, 从`FIFO`或优先级队列获取任务
     */
    class worker_standard : public worker_base
    {
    public:
      /**
       * @brief 构造标准工作线程
       * @param worker_name 线程ID
       * @param cohort_struct 任务队列
       */
      worker_standard(const std::string &worker_name, _interior_cohort_ptr cohort_struct)
        : worker_base(worker_name, std::move(cohort_struct)) {}
      /**
       * @brief 析构函数
       */
      ~worker_standard() override = default;
    };
    /**
     * @class worker_priority
     * @brief 优先级工作线程实现
     *
     * 专门处理优先级任务的工作线程，支持优先级调度
     *
     * 特点：优先处理高优先级任务, 支持任务抢占机制, 动态调整处理策略
     *
     * 调用关系：
     *   - 继承自`worker_base`,主要从`cohort_prior`获取任务,支持多级优先级调度
     */
    class worker_priority : public worker_base
    {
    private:
      std::atomic<bool> _preemptive_mode{false}; // 是否处于抢占模式
      std::atomic<structure_u::weight> _min_priority; // 当前最低处理优先级
    public:
      /**
       * @brief 构造优先级工作线程
       * @param worker_name 线程ID
       * @param cohort_struct 任务队列
       * @param min_priority 最低处理优先级
       */
      worker_priority(const std::string &worker_name,_interior_cohort_ptr cohort_struct,
      structure_u::weight min_priority = structure_u::weight::low)
        : worker_base(worker_name, std::move(cohort_struct)), _min_priority(min_priority) {}
      /**
       * @brief 析构函数
       */
      ~worker_priority() override = default;
      /**
       * @brief 设置最低处理优先级
       * @param priority 最低优先级
       */
      void set_min_priority(structure_u::weight priority)
      {
        _min_priority.store(priority, std::memory_order_release);
      }
      /**
       * @brief 获取最低处理优先级
       * @return 最低优先级
       */
      structure_u::weight get_min_priority() const
      {
        return _min_priority.load(std::memory_order_acquire);
      }
      /**
       * @brief 设置抢占模式
       * @param enabled 是否启用抢占
       */
      void set_preemptive_mode(bool enabled)
      {
        _preemptive_mode.store(enabled, std::memory_order_release);
      }
      /**
       * @brief 检查是否启用抢占模式
       * @return `true` 启用抢占，`false` 未启用
       */
      bool is_preemptive_mode() const
      {
        return _preemptive_mode.load(std::memory_order_acquire);
      }
    protected:
    /**
       * @brief 获取下一个任务（优先级版本）
       * @return 任务智能指针
       */
      _interior_task_ptr get_next_task() override
      {
        if (!_task_queue)
        {
          return nullptr;
        }

        // 获取
        auto task = _task_queue->pop();

        // 检查任务优先级
        if (task)
        {
          auto min_priority = _min_priority.load(std::memory_order_acquire);
          if (task->get_priority() < static_cast<int>(min_priority))
          {
            // 优先级不够，重新放回队列
            _task_queue->push(task);
            return nullptr;
          }
        }

        return task;
      }
      /**
       * @brief 执行任务（优先级版本）
       * @param task 要执行的任务
       */
      void execute_task(_interior_task_ptr task) override
      {
        if (!task)
        {
          return;
        }

        // 在抢占模式下，检查是否有更高优先级的任务
        if (_preemptive_mode.load(std::memory_order_acquire))
        {
          auto higher_priority_task = _task_queue->try_pop();
          if (higher_priority_task && higher_priority_task->get_priority() > task->get_priority())
          {
            // 有更高优先级任务，先执行高优先级任务
            _task_queue->push(task); // 当前任务重新入队
            worker_base::execute_task(higher_priority_task);
            return;
          }
          else if (higher_priority_task)
          {
            // 没有更高优先级，将任务放回队列
            _task_queue->push(higher_priority_task);
          }
        }
        worker_base::execute_task(task);
      }
    };
    /**
     * @class worker_fibersvr
     * @brief 协程工作线程实现
     *
     * 支持C++20协程的工作线程，适用于异步任务处理
     *
     * 特点：支持协程任务执行,高并发异步处理,低内存占用
     *
     * 调用关系：继承自`worker_base`,专门处理`task_coro`,支持协程调度和管理
     */
    class worker_fibersvr : public worker_base
    {
    private:
      std::atomic<std::size_t> _active_coroutines{0}; // 当前活跃协程数
      std::atomic<std::size_t> _max_concurrent_coroutines{100}; // 最大并发协程数
    public:
      /**
       * @brief 构造协程工作线程
       * @param worker_name 线程ID
       * @param cohort_struct 任务队列
       * @param max_concurrent 最大并发协程数
       */
      worker_fibersvr(const std::string &worker_name,_interior_cohort_ptr cohort_struct,std::size_t max_concurrent = 100)
        : worker_base(worker_name, std::move(cohort_struct)), _max_concurrent_coroutines(max_concurrent){}
      /**
       * @brief 析构函数
       */
      ~worker_fibersvr() override = default;
      /**
       * @brief 设置最大并发协程数
       * @param max_concurrent 最大并发数
       */
      void set_max_concurrent_coroutines(std::size_t max_concurrent)
      {
        _max_concurrent_coroutines.store(max_concurrent, std::memory_order_release);
      }
      /**
       * @brief 获取最大并发协程数
       * @return 最大并发数
       */
      std::size_t get_max_concurrent_coroutines() const
      {
        return _max_concurrent_coroutines.load(std::memory_order_acquire);
      }
      /**
       * @brief 获取当前活跃协程数
       * @return 活跃协程数
       */
      std::size_t get_active_coroutines() const
      {
        return _active_coroutines.load(std::memory_order_acquire);
      }
    protected:
      /**
       * @brief 执行任务（协程版本）
       * @param task 要执行的任务
       */
      void execute_task(_interior_task_ptr task) override
      {
        if (!task)
        {
          return;
        }
        execute_coroutine_task(task);
        // 检查是否为协程任务
        // auto coroutine_task = std::dynamic_pointer_cast<structure_u::task_coro>(task);
        // if (coroutine_task)
        // {
        //   execute_coroutine_task(coroutine_task);
        // }
        // else
        // {
        //   // 普通任务，使用基类方法执行
        //   worker_base::execute_task(task);
        // }
      }
    private:
      /**
       * @brief 执行协程任务
       * @param task 协程任务
       */
      void execute_coroutine_task(_interior_task_ptr task)
      {
        // 检查并发限制
        auto current_coroutines = _active_coroutines.load(std::memory_order_acquire);
        auto max_coroutines = _max_concurrent_coroutines.load(std::memory_order_acquire);

        if (current_coroutines >= max_coroutines)
        {
          // 达到并发限制，将任务重新放回队列
          _task_queue->push(task);
          return;
        }

        // 增加活跃协程计数
        _active_coroutines.fetch_add(1, std::memory_order_relaxed);

        auto start_time = std::chrono::steady_clock::now();

        try
        {
          // 任务开始回调
          if (_starts_callback)
          {
            _starts_callback(_worker_name, task);
          }

          // 执行协程任务
          task->execute();

          // 更新统计信息
          auto end_time = std::chrono::steady_clock::now();
          auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

          _statistics.tasks_executed.fetch_add(1, std::memory_order_relaxed);
          _statistics.total_execution_time.fetch_add(execution_time, std::memory_order_relaxed);
          _statistics.last_task_time = end_time;

          // 任务结束回调
          if (_finish_callback)
          {
            _finish_callback(_worker_name, task);
          }
        }
        catch (const std::exception &e)
        {
          _statistics.tasks_failed.fetch_add(1, std::memory_order_relaxed);

          if (_abnormal_callback)
          {
            _abnormal_callback(_worker_name, e);
          }
        }
        // 减少活跃协程计数
        _active_coroutines.fetch_sub(1, std::memory_order_relaxed);
      }
    };
    /**
     * @class worker_adaptive
     * @brief 自适应工作线程实现
     *
     * 能够根据负载情况自动调整行为的智能工作线程
     *
     * 特点：动态调整任务获取策略,自适应休眠时间,负载感知优化
     *
     * 调用关系：继承自`worker_base`,支持多种任务队列类型,根据系统负载动态调整
     */
    class worker_adaptive : public worker_base
    {
    private:
      static constexpr std::size_t LOAD_SAMPLE_SIZE = 100;  ///< 负载采样大小
      static constexpr std::size_t MAX_SLEEP_TIME_MS = 100; ///< 最大休眠时间(毫秒)

      std::atomic<double> _load_factor{0.0}; // 负载因子
      std::atomic<std::size_t> _consecutive_empty_polls{0};  // 连续空轮询次数
      std::atomic<std::chrono::milliseconds> _adaptive_sleep_time{std::chrono::milliseconds(1)}; // 自适应休眠时间  
    public:
    /**
       * @brief 构造自适应工作线程
       * @param worker_name 线程ID
       * @param cohort_struct 任务队列
       */
      worker_adaptive(const std::string &worker_name,_interior_cohort_ptr cohort_struct)
        : worker_base(worker_name, std::move(cohort_struct)){}
      /**
       * @brief 析构函数
       */
      ~worker_adaptive() override = default;
      /**
       * @brief 获取当前负载因子
       * @return 负载因子(0.0-1.0)
       */
      double get_load_factor() const
      {
        return _load_factor.load(std::memory_order_acquire);
      }
      /**
       * @brief 获取当前自适应休眠时间
       * @return 休眠时间
       */
      std::chrono::milliseconds get_adaptive_sleep_time() const
      {
        return _adaptive_sleep_time.load(std::memory_order_acquire);
      }
    protected:
      /**
       * @brief 获取下一个任务（自适应版本）
       * @return 任务智能指针
       */
      _interior_task_ptr get_next_task() override
      {
        if (!_task_queue)
        {
          return nullptr;
        }

        // 根据负载调整超时时间
        auto load = _load_factor.load(std::memory_order_acquire);
        auto timeout = std::chrono::milliseconds(static_cast<long>(50 + load * 50));

        auto task = _task_queue->try_pop_for(timeout);

        if (task)
        {
          // 获取到任务，重置空轮询计数
          _consecutive_empty_polls.store(0, std::memory_order_relaxed);
          update_load_factor(true);
        }
        else
        {
          // 未获取到任务，增加空轮询计数
          auto empty_polls = _consecutive_empty_polls.fetch_add(1, std::memory_order_relaxed);
          update_load_factor(false);
          adjust_sleep_time(empty_polls + 1);
        }
        return task;
      }
      /**
       * @brief 处理无任务情况（自适应版本）
       */
      void handle_no_task() override
      {
        auto sleep_time = _adaptive_sleep_time.load(std::memory_order_acquire);

        auto idle_start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(sleep_time);
        auto idle_end = std::chrono::steady_clock::now();

        auto idle_time = std::chrono::duration_cast<std::chrono::microseconds>(idle_end - idle_start).count();
        _statistics.total_idle_time.fetch_add(idle_time, std::memory_order_relaxed);
      }
    private:
      /**
       * @brief 调整休眠时间
       * @param empty_polls 连续空轮询次数
       */
      void adjust_sleep_time(std::size_t empty_polls)
      {
        // 根据连续空轮询次数调整休眠时间
        std::size_t sleep_ms = std::min(empty_polls / 10, MAX_SLEEP_TIME_MS);
        _adaptive_sleep_time.store(std::chrono::milliseconds(sleep_ms), std::memory_order_release);
      }

      /**
       * @brief 更新负载因子
       * @param got_task 是否获取到任务
       */
      void update_load_factor(bool got_task)
      {
        // 使用指数移动平均更新负载因子
        constexpr double alpha = 0.1; // 平滑因子
        auto current_load = _load_factor.load(std::memory_order_acquire);
        auto new_sample = got_task ? 1.0 : 0.0;
        auto new_load = alpha * new_sample + (1.0 - alpha) * current_load;
        _load_factor.store(new_load, std::memory_order_release);
      }
    };
     /**
     * @brief 工作线程工厂函数 - 创建标准工作线程
     * @param worker_name 线程ID
     * @param cohort_struct 任务队列
     * @return 工作线程智能指针
     */
    inline std::unique_ptr<worker_base> make_worker_standard(const std::string &worker_name,_interior_cohort_ptr cohort_struct)
    {
      return std::make_unique<worker_standard>(worker_name, std::move(cohort_struct));
    }
    /**
     * @brief 工作线程工厂函数 - 创建优先级工作线程
     * @param worker_name 线程ID
     * @param cohort_struct 任务队列
     * @param min_priority 最低处理优先级
     * @return 工作线程智能指针
     */
    inline std::unique_ptr<worker_base> make_worker_priority(const std::string &worker_name,_interior_cohort_ptr cohort_struct,
      structure_u::weight min_priority =  structure_u::weight::low)
    {
      return std::make_unique<worker_priority>(worker_name, std::move(cohort_struct), min_priority);
    }
    /**
     * @brief 工作线程工厂函数 - 创建协程工作线程
     * @param worker_name 线程ID
     * @param cohort_struct 任务队列
     * @param max_concurrent 最大并发协程数
     * @return 工作线程智能指针
     */
    inline std::unique_ptr<worker_base> make_worker_fibersvr(const std::string &worker_name,_interior_cohort_ptr cohort_struct,
      std::size_t max_concurrent = 100)
    {
      return std::make_unique<worker_fibersvr>(worker_name, std::move(cohort_struct), max_concurrent);
    }
    /**
     * @brief 工作线程工厂函数 - 创建自适应工作线程
     * @param worker_name 线程ID
     * @param cohort_struct 任务队列
     * @return 工作线程智能指针
     */
    inline std::unique_ptr<worker_base> make_worker_adaptive(const std::string &worker_name,_interior_cohort_ptr cohort_struct)
    {
      return std::make_unique<worker_adaptive>(worker_name, std::move(cohort_struct));
    }
  }
}

namespace pool
{
  using internals::structure_w::worker_adaptive;
  using internals::structure_w::worker_fibersvr;
  using internals::structure_w::worker_priority;
  using internals::structure_w::worker_standard;

  using internals::structure_w::make_worker_standard;
  using internals::structure_w::make_worker_adaptive;
  using internals::structure_w::make_worker_priority;
  using internals::structure_w::make_worker_fibersvr;

  using internals::structure_w::worker_statistics;
  using internals::structure_w::worker_state;
}