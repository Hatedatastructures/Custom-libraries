#pragma once
#include "Task.hpp"
#include "Cohort.hpp"
#include "Worker.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <numeric>

namespace internals
{
  namespace structure_s
  {
    /**
     * @enum scheduling_tactics
     * @brief 调度策略枚举
     *
     * 定义不同的任务调度策略，用于优化不同场景下的性能
     */
    enum class scheduling_tactics
    {
      round_robin,    // 轮询调度   - 平均分配任务
      least_loaded,   // 最少负载   - 分配给负载最轻的线程
      priority_based, // 优先级调度 - 基于任务优先级
      adaptive,       // 自适应调度 - 根据性能动态调整
      work_stealing,  // 工作窃取   - 空闲线程从忙碌线程窃取任务
      locality_aware  // 局部性感知 - 考虑数据局部性
    };
    /**
     * @enum expansion_strategy
     * @brief 扩缩容策略枚举
     *
     * 定义线程池动态扩缩容的策略
     */
    enum class expansion_strategy
    {
      conservative, // 保守策略 - 缓慢调整
      aggressive,   // 激进策略 - 快速调整
      predictive,   // 预测策略 - 基于历史数据预测
      reactive,     // 响应策略 - 基于当前负载
      hybrid        // 混合策略 - 结合多种策略
    };
    /**
     * @struct load_metrics
     * @brief 负载指标结构
     *
     * 记录系统负载的各种指标，用于调度决策
     */
    class load_metrics
    {
    public:
      std::atomic<double> throughput{0.0};               // 吞吐量(任务/秒)
      std::atomic<double> memory_usage{0.0};             // 内存使用率
      std::atomic<double> cpu_utilization{0.0};          // CPU利用率
      std::atomic<double> average_task_time{0.0};        // 平均任务执行时间

      std::atomic<std::size_t> queue_length{0};          // 队列长度
      std::atomic<std::size_t> active_threads{0};        // 活跃线程数

      std::chrono::steady_clock::time_point last_update; // 最后更新时间

      /**
       * @brief 重置指标
       */
      void reset()
      {
        throughput.store(0.0, std::memory_order_relaxed);
        memory_usage.store(0.0, std::memory_order_relaxed);
        cpu_utilization.store(0.0, std::memory_order_relaxed);
        average_task_time.store(0.0, std::memory_order_relaxed);

        queue_length.store(0, std::memory_order_relaxed);
        active_threads.store(0, std::memory_order_relaxed);

        last_update = std::chrono::steady_clock::now();
      }

      /**
       * @brief 计算综合负载分数
       * @return 负载分数(0.0-1.0)
       */
      double calculate_load_score() const
      {
        auto cpu = cpu_utilization.load(std::memory_order_relaxed);
        auto memory = memory_usage.load(std::memory_order_relaxed);
        auto queue_factor = std::min(queue_length.load(std::memory_order_relaxed) / 100.0, 1.0);

        // 加权计算综合负载分数
        return 0.4 * cpu + 0.3 * memory + 0.3 * queue_factor;
      }
    };
    /**
     * @struct scaling_config
     * @brief 扩缩容配置结构
     *
     * 定义线程池扩缩容的各种参数
     */
    class scaling_config
    {
    public:
      std::size_t min_threads = 1;                      // 最小线程数
      std::size_t max_threads = 64;                     // 最大线程数
      std::size_t core_threads = 4;                     // 核心线程数
      double scale_up_threshold = 0.8;                  // 扩容阈值
      double scale_down_threshold = 0.4;                // 缩容阈值

      std::size_t scale_up_step = 1;                    // 扩容步长
      std::size_t scale_down_step = 1;                  // 缩容步长
      bool enable_predictive_scaling = true;            // 启用预测性扩缩容

      std::chrono::milliseconds scale_up_delay{1000};   // 扩容延迟
      std::chrono::milliseconds scale_down_delay{5000}; // 缩容延迟
    };
    using _interior_task_ptr   = std::shared_ptr<internals::structure_u::uint_ordinary>;
    using _interior_cohort_ptr = std::shared_ptr<internals::structure_c::cohort_base>;
    using _interior_thread_ptr = std::unique_ptr<internals::structure_w::worker_base>;
    /**
     * @class scheduler_base
     * @brief 调度器基类
     *
     * 定义调度器的基本接口和行为，所有具体调度器都继承自此类
     *
     * 设计模式：策略模式：支持不同的调度策略,观察者模式：监控系统状态变化,
     *  模板方法模式：定义调度流程
     *
     * 调用关系： 被`thread_pool`管理和调用,
     * 管理`worker`和`cohort`,监控系统性能指标
     */
    class scheduler_base
    {
    protected:
    _interior_cohort_ptr _task_queue; // 任务队列
      std::vector<_interior_thread_ptr> _workers; // 工作线程列表

      std::atomic<bool> _running{false}; // 调度器运行状态
      std::atomic<bool> _should_stop{false}; // 停止标志

      std::mutex _scaling_mutex; // 扩缩容互斥锁
      mutable std::shared_mutex _workers_mutex; // 工作线程读写锁

      std::condition_variable _scaling_cv; // 扩缩容条件变量

      std::unique_ptr<std::jthread> _monitor_thread; // 监控线程
      std::unique_ptr<std::jthread> _scaling_thread; // 扩缩容线程

      load_metrics _metrics; // 负载指标
      scheduling_tactics _policy; // 调度策略
      scaling_config _scaling_config; // 扩缩容配置
      expansion_strategy _scaling_policy; // 扩缩容策略

      std::function<void(const std::string &)> _event_callback; // 事件回调
      std::function<_interior_thread_ptr(const std::string &)> _worker_factory; // 工作线程工厂

      std::chrono::steady_clock::time_point _start_time;       // 启动时间
      std::atomic<std::uint64_t> _total_tasks_scheduled{0};    // 总调度任务数
      std::atomic<std::uint64_t> _total_scaling_operations{0}; // 总扩缩容操作数
    public:
      scheduler_base(_interior_cohort_ptr task_queue, scheduling_tactics policy = scheduling_tactics::adaptive,
      expansion_strategy scaling_policy = expansion_strategy::hybrid)
      : _task_queue(std::move(task_queue)),_policy(policy), _scaling_policy(scaling_policy)
      {
        _start_time = std::chrono::steady_clock::now();
        _worker_factory = [this] (const std::string &name) -> _interior_thread_ptr 
        {
          return internals::structure_w::make_worker_standard(name, _task_queue); 
        };
      }
      /**
       * @brief 虚析构函数
       */
      virtual ~scheduler_base()
      {
        stop();
      }
      // 禁用拷贝和移动
      scheduler_base(const scheduler_base &) = delete;
      scheduler_base &operator=(const scheduler_base &) = delete;
      scheduler_base(scheduler_base &&) = delete;
      scheduler_base &operator=(scheduler_base &&) = delete;
      /**
       * @brief 启动调度器
       * @param initial_threads 初始线程数
       * @return true 启动成功，false 启动失败
       */
      virtual bool start(std::size_t initial_threads = 0)
      {
        if (_running.load(std::memory_order_acquire))
        {
          return false;
        }

        try
        {
          // 确定初始线程数
          if (initial_threads == 0)
          {
            initial_threads = _scaling_config.core_threads;
          }
          initial_threads = std::clamp(initial_threads, _scaling_config.min_threads, _scaling_config.max_threads);

          // 创建初始工作线程
          if (!create_workers(initial_threads))
          {
            return false;
          }

          // 启动监控和扩缩容线程
          _should_stop.store(false, std::memory_order_release);
          _running.store(true, std::memory_order_release);

          _monitor_thread = std::make_unique<std::jthread>(&scheduler_base::monitor_loop, this);
          _scaling_thread = std::make_unique<std::jthread>(&scheduler_base::scaling_loop, this);

          if (_event_callback)
          {
            _event_callback("Scheduler started with " + std::to_string(initial_threads) + " threads");
          }

          return true;
        }
        catch (const std::exception &e)
        {
          if (_event_callback)
          {
            _event_callback("Failed to start scheduler: " + std::string(e.what()));
          }
          return false;
        }
      }
      /**
       * @brief 停止调度器
       * @param wait_for_completion 是否等待任务完成
       */
      virtual void stop(bool wait_for_completion = true)
      {
        if (!_running.load(std::memory_order_acquire))
        {
          return;
        }

        _should_stop.store(true, std::memory_order_release);
        _scaling_cv.notify_all();

        // 停止监控和扩缩容线程
        if (_monitor_thread && _monitor_thread->joinable())
        {
          _monitor_thread->join();
        }
        if (_scaling_thread && _scaling_thread->joinable())
        {
          _scaling_thread->join();
        }

        // 停止所有工作线程
        stop_all_workers(wait_for_completion);

        _running.store(false, std::memory_order_release);

        if (_event_callback)
        {
          _event_callback("Scheduler stopped");
        }
      }
      /**
       * @brief 提交任务
       * @param task 要提交的任务
       * @return `true` 提交成功，`false` 提交失败
       */
      virtual bool submit_task(_interior_task_ptr task)
      {
        if (!task || !_running.load(std::memory_order_acquire))
        {
          return false;
        }

        // 执行调度策略
        bool result = schedule_task(task);

        if (result)
        {
          _total_tasks_scheduled.fetch_add(1, std::memory_order_relaxed);
          update_metrics_on_task_submit();
        }
        return result;
      }
      /**
       * @brief 获取当前线程数
       * @return 线程数量
       */
      std::size_t get_thread_count() const
      {
        std::shared_lock<std::shared_mutex> lock(_workers_mutex);
        return _workers.size();
      }

      /**
       * @brief 获取活跃线程数
       * @return 活跃线程数量
       */
      std::size_t get_active_thread_count() const
      {
        std::shared_lock<std::shared_mutex> lock(_workers_mutex);
        auto active_count = [this](const _interior_thread_ptr &worker)
        { return worker->is_running(); };
        return std::count_if(_workers.begin(), _workers.end(), active_count);
      }
      /**
       * @brief 获取负载指标
       * @return 负载指标的常量引用
       */
      const load_metrics &get_metrics() const
      {
        return _metrics;
      }
      /**
       * @brief 设置扩缩容配置
       * @param config 扩缩容配置
       */
      void set_scaling_config(const scaling_config &config)
      {
        std::lock_guard<std::mutex> lock(_scaling_mutex);
        _scaling_config = config;
        _scaling_cv.notify_one();
      }
      /**
       * @brief 获取扩缩容配置
       * @return 扩缩容配置的常量引用
       */
      const scaling_config & get_scaling_config() const
      {
        return _scaling_config;
      }
      /**
       * @brief 设置调度策略
       * @param policy 调度策略
       */
      void set_scheduling_policy(scheduling_tactics policy)
      {
        _policy = policy;
      }
      /**
       * @brief 获取调度策略
       * @return 当前调度策略
       */
      scheduling_tactics get_scheduling_policy() const
      {
        return _policy;
      }
      /**
       * @brief 设置扩缩容策略
       * @param policy 扩缩容策略
       */
      void set_scaling_policy(expansion_strategy policy)
      {
        _scaling_policy = policy;
      }

      /**
       * @brief 获取扩缩容策略
       * @return 当前扩缩容策略
       */
      expansion_strategy get_scaling_policy() const
      {
        return _scaling_policy;
      }

      /**
       * @brief 设置事件回调
       * @param callback 事件回调函数
       */
      void set_event_callback(std::function<void(const std::string &)> callback)
      {
        _event_callback = std::move(callback);
      }
      /**
       * @brief 设置工作线程工厂
       * @param factory 工作线程工厂函数
       */
      void set_worker_factory(std::function<_interior_thread_ptr(const std::string &)> factory)
      {
        _worker_factory = std::move(factory);
      }
      /**
       * @brief 获取总调度任务数
       * @return 总任务数
       */
      std::uint64_t get_total_tasks_scheduled() const
      {
        return _total_tasks_scheduled.load(std::memory_order_relaxed);
      }
      /**
       * @brief 获取总扩缩容操作数
       * @return 总操作数
       */
      std::uint64_t get_total_scaling_operations() const
      {
        return _total_scaling_operations.load(std::memory_order_relaxed);
      }
      /**
       * @brief 获取运行时间
       * @return 运行时间(秒)
       */
      double get_uptime() const
      {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - _start_time);
        return duration.count();
      }
      /**
       * @brief 手动扩容
       * @warning 按照配置扩容步长来控制扩容数量
       */
      void manual_scale_up()
      {
        scale_up();
      }
      /**
       * @brief 手动扩容
       * @param count 扩容数量
       */
      void mutual_scale_ups(std::size_t count)
      {
        auto scale_threads = _scaling_config.max_threads - get_thread_count();
        auto scale_count = std::min(count, scale_threads);

        if (scale_count > 0 && create_workers(scale_count))
        {
          _total_scaling_operations.fetch_add(1, std::memory_order_relaxed);

          if (_event_callback)
          {
            _event_callback("Scaled up by " + std::to_string(scale_count) + " threads");
          }
        }
      }
      /**
       * @brief 手动缩容
       * @warning 按照缩容步长来缩容
       */
      void manual_scale_down()
      {
        scale_down();
      }
      /**
       * @brief 手动缩容
       * @param count 缩容数量
       */
      void manual_scale_downs(std::size_t count)
      {
        auto scale_threads = get_thread_count() - _scaling_config.min_threads;
        auto scale_count = std::min(count, scale_threads);

        if (scale_count > 0)
        {
          std::unique_lock<std::shared_mutex> lock(_workers_mutex);

          // 停止最后几个工作线程
          for (std::size_t i = 0; i < scale_count && !_workers.empty(); ++i)
          {
            auto &worker = _workers.back();
            if (worker)
            {
              worker->stop(true);
            }
            _workers.pop_back();
          }

          _total_scaling_operations.fetch_add(1, std::memory_order_relaxed);

          if (_event_callback)
          {
            _event_callback("Scaled down by " + std::to_string(scale_count) + " threads");
          }
        }
      }
      /**
       * @brief 检查调度器是否正在运行
       * @return `true` 正在运行，`false` 未运行
       */
      bool is_running() const
      {
        return _running.load(std::memory_order_acquire);
      }
    protected:
      /**
       * @brief 调度任务 - 策略方法
       * @param task 要调度的任务
       * @return true 调度成功，false 调度失败
       */
      virtual bool schedule_task(_interior_task_ptr task) = 0;
      /**
       * @brief 创建工作线程
       * @param count 线程数量
       * @return `true` 创建成功，`false` 创建失败
       */
      virtual bool create_workers(std::size_t count)
      {
        std::unique_lock<std::shared_mutex> lock(_workers_mutex);

        for (std::size_t i = 0; i < count; ++i)
        {
          auto worker_id = "worker_" + std::to_string(_workers.size());
          auto worker = _worker_factory(worker_id);

          if (!worker || !worker->start())
          {
            return false;
          }

          _workers.push_back(std::move(worker));
        }
        return true;
      }
      /**
       * @brief 停止所有工作线程
       * @param wait_for_completion 是否等待任务完成
       */
      virtual void stop_all_workers(bool wait_for_completion)
      {
        std::unique_lock<std::shared_mutex> lock(_workers_mutex);

        for (auto &worker : _workers)
        {
          if (worker)
          {
            worker->stop(wait_for_completion);
          }
        }
        _workers.clear();
      }
      /**
       * @brief 监控循环
       */
      virtual void monitor_loop()
      {
        while (!_should_stop.load(std::memory_order_acquire))
        {
          update_metrics();
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
      /**
       * @brief 扩缩容循环
       */
      virtual void scaling_loop()
      {
        while (!_should_stop.load(std::memory_order_acquire))
        {
          std::unique_lock<std::mutex> lock(_scaling_mutex);
          auto time_out_func = [this] { return _should_stop.load(); };
          _scaling_cv.wait_for(lock, std::chrono::seconds(1), time_out_func);
          if (!_should_stop.load())
          {
            evaluate_scaling();
          }
        }
      }
      /**
       * @brief 更新性能指标
       */
      virtual void update_metrics()
      {
        // 更新队列长度
        _metrics.queue_length.store(_task_queue->size(), std::memory_order_relaxed);

        // 更新活跃线程数
        _metrics.active_threads.store(get_active_thread_count(), std::memory_order_relaxed);

        // 更新时间戳
        _metrics.last_update = std::chrono::steady_clock::now();

        // 子类可以重写此方法添加更多指标
      }
      /**
       * @brief 任务提交时更新指标
       */
      virtual void update_metrics_on_task_submit()
      {
        // 子类可以重写此方法
      }
      /**
       * @brief 评估扩缩容需求
       */
      virtual void evaluate_scaling()
      {
        auto load_score = _metrics.calculate_load_score();
        auto current_threads = get_thread_count();

        if (load_score > _scaling_config.scale_up_threshold && current_threads < _scaling_config.max_threads)
        {
          // 需要扩容
          scale_up();
        }
        else if (load_score < _scaling_config.scale_down_threshold && current_threads > _scaling_config.min_threads)
        {
          // 需要缩容
          scale_down();
        }
      }
      /**
       * @brief 扩容操作
       */
      virtual void scale_up()
      {
        auto scale_threads = _scaling_config.max_threads - get_thread_count();
        auto scale_count = std::min(_scaling_config.scale_up_step,scale_threads);

        if (scale_count > 0 && create_workers(scale_count))
        {
          _total_scaling_operations.fetch_add(1, std::memory_order_relaxed);

          if (_event_callback)
          {
            _event_callback("Scaled up by " + std::to_string(scale_count) + " threads");
          }
        }
      }
      /**
       * @brief 缩容操作
       */
      virtual void scale_down()
      {
        auto scale_threads = get_thread_count() - _scaling_config.min_threads;
        auto scale_count = std::min(_scaling_config.scale_down_step, scale_threads);

        if (scale_count > 0)
        {
          std::unique_lock<std::shared_mutex> lock(_workers_mutex);

          // 停止最后几个工作线程
          for (std::size_t i = 0; i < scale_count && !_workers.empty(); ++i)
          {
            auto &worker = _workers.back();
            if (worker)
            {
              worker->stop(true);
            }
            _workers.pop_back();
          }

          _total_scaling_operations.fetch_add(1, std::memory_order_relaxed);

          if (_event_callback)
          {
            _event_callback("Scaled down by " + std::to_string(scale_count) + " threads");
          }
        }
      }
    };
    /**
     * @class scheduler_standard
     * @brief 标准调度器实现
     *
     * 实现基本的任务调度功能，支持多种调度策略
     *
     * 特性：支持轮询、最少负载等基本策略, 简单高效的任务分发,适用于一般场景
     *
     * 调用关系：继承自`scheduler_base`, 
     * 被`thread_pool`使用, 管理标准工作线程
     */
    class scheduler_standard : public scheduler_base
    {
    private:
      std::atomic<std::size_t> _round_robin_index{0}; // 轮询索引
    public: 
      /**
       * @brief 构造标准调度器
       * @param task_queue 任务队列
       * @param policy 调度策略
       * @param expansion_strategy 扩缩容策略
       */
      scheduler_standard(_interior_cohort_ptr task_queue,scheduling_tactics policy = scheduling_tactics::round_robin,
      expansion_strategy expansion_strategy = expansion_strategy::reactive)
        :scheduler_base(std::move(task_queue), policy, expansion_strategy) {}
    protected:
      /**
       * @brief 调度任务实现
       * @param task 要调度的任务
       * @return true 调度成功，false 调度失败
       */
      bool schedule_task(_interior_task_ptr task) override
      {
        switch (_policy)
        {
        case scheduling_tactics::round_robin:
          return schedule_round_robin(task);
        case scheduling_tactics::least_loaded:
          return schedule_least_loaded(task);
        case scheduling_tactics::priority_based:
          return schedule_priority_based(task);
        case scheduling_tactics::adaptive:
          return schedule_adaptive(task);
        default:
          return _task_queue->push(task);
        }
      }
    private:
      /**
       * @brief 轮询调度
       * @param task 任务
       * @return 调度结果
       */
      bool schedule_round_robin(_interior_task_ptr task)
      {
        // 简单的轮询策略，直接放入队列
        return _task_queue->push(task);
      }

      /**
       * @brief 最少负载调度
       * @param task 任务
       * @return 调度结果
       */
      bool schedule_least_loaded(_interior_task_ptr task)
      {
        // 基于队列长度的简单负载均衡
        if (_task_queue->size() < get_thread_count() * 2)
        {
          return _task_queue->push(task);
        }

        // 队列过长时延迟提交
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return _task_queue->push(task);
      }

      /**
       * @brief 优先级调度
       * @param task 任务
       * @return 调度结果
       */
      bool schedule_priority_based(_interior_task_ptr task)
      {
        // 直接使用优先级队列的特性
        return _task_queue->push(task);
      }

      /**
       * @brief 自适应调度
       * @param task 任务
       * @return 调度结果
       */
      bool schedule_adaptive(_interior_task_ptr task)
      {
        auto load_score = _metrics.calculate_load_score();

        if (load_score < 0.5)
        {
          // 低负载时使用轮询
          return schedule_round_robin(task);
        }
        else if (load_score < 0.8)
        {
          // 中等负载时使用最少负载
          return schedule_least_loaded(task);
        }
        else
        {
          // 高负载时使用优先级
          return schedule_priority_based(task);
        }
      }
    };
    /**
     * @class scheduler_priority
     * @brief 优先级调度器
     *
     * 专门针对优先级任务优化的调度器
     *
     * 特性：严格按优先级调度,支持优先级抢占,防止优先级反转
     *
     * 调用关系：继承自`scheduler_base`,使用`cohort_prior`,管理`worker_priority`
     */
    class scheduler_priority : public scheduler_base
    {
    private:
      mutable std::mutex _stats_mutex; // 统计锁
      std::unordered_map<internals::structure_u::weight,std::size_t> _priority_stats; //优先级统计
    public:
      /**
       * @brief 构造优先级调度器
       * @param task_queue 任务队列(应为`cohort_prior`)
       * @param expansion_strategy 扩缩容策略
       */
      scheduler_priority(_interior_cohort_ptr task_queue,expansion_strategy expansion_strategy = expansion_strategy::conservative)
        : scheduler_base(std::move(task_queue), scheduling_tactics::priority_based, expansion_strategy)
      {
        // 设置优先级工作线程工厂
        _worker_factory = [this](const std::string &worker_id)
        {
          return internals::structure_w::make_worker_priority(worker_id, _task_queue, 
            internals::structure_u::weight::lowest);
        };
      }
      /**
       * @brief 获取优先级统计
       * @return 优先级统计映射
       */
      std::unordered_map<internals::structure_u::weight,std::size_t> get_priority_statistics() const
      {
        std::lock_guard<std::mutex> lock(_stats_mutex);
        return _priority_stats;
      }
    protected:
      /**
       * @brief 调度任务实现
       * @param task 要调度的任务
       * @return true 调度成功，false 调度失败
       */
      bool schedule_task(_interior_task_ptr task) override
      {
        {
          std::lock_guard<std::mutex> lock(_stats_mutex);
          _priority_stats[static_cast<internals::structure_u::weight>(task->get_priority())]++;
        }

        // 检查是否需要优先级抢占
        if (task->get_priority() == static_cast<std::int32_t>
        (internals::structure_u::weight::critical))
        {
          handle_critical_task(task);
        }

        return _task_queue->push(task);
      }

      /**
       * @brief 更新性能指标
       */
      void update_metrics() override
      {
        scheduler_base::update_metrics();

        // 添加优先级相关指标
        std::lock_guard<std::mutex> lock(_stats_mutex);
        auto traverse_funcion = [](std::size_t sum, const auto &pair)
        {
          return sum + pair.second; 
        };
        std::size_t total_tasks = std::accumulate(_priority_stats.begin(), _priority_stats.end(), 0ULL, traverse_funcion);

        if (total_tasks > 0)
        {
          auto critical_ratio = static_cast<double>
          (_priority_stats[internals::structure_u::weight::critical]) / total_tasks;
          // 可以基于关键任务比例调整调度策略
          if (critical_ratio > 0.1)
          {
            if (get_thread_count() < _scaling_config.max_threads)
            {
              scale_up();
            }
          }
          else
          {
            if (get_thread_count() > _scaling_config.min_threads)
            {
              scale_down();
            }
          }
        }
      }
    private:
      /**
       * @brief 处理关键任务
       * @param task 关键任务
       */
      void handle_critical_task(_interior_task_ptr task)
      {
        if (_event_callback)
        {
          _event_callback("Critical task submitted: " + task->get_task_name());
        }

        // 可以考虑立即创建新线程处理关键任务
        if (get_thread_count() < _scaling_config.max_threads)
        {
          auto current_load = _metrics.calculate_load_score();
          if (current_load > 0.7) // 高负载时为关键任务扩容
          {
            scale_up();
          }
        }
      }
    };
    /**
     * @class scheduler_adaptive
     * @brief 自适应调度器
     *
     * 基于机器学习和历史数据的智能调度器
     *
     * 特性：动态学习任务模式, 预测性调度, 自动优化参数
     *
     * 调用关系：继承自scheduler_base,使用worker_adaptive,
     * 集成性能分析器
     * @warning 自适应基于历史数据,可能需要较长时间才能收敛
     */
    class scheduler_adaptive : public scheduler_base
    {
    private: 
      struct task_pattern
      {
        double success_rate = 1.0; // 成功率
        std::size_t execution_count = 0; // 执行次数
        std::chrono::milliseconds average_duration{0}; // 平均执行时间
        std::chrono::steady_clock::time_point last_seen; // 最后出现时间
      };

      mutable std::shared_mutex _patterns_mutex; // 模式读写锁
      std::unordered_map<std::string, task_pattern> _task_patterns; // 任务模式

      std::queue<double> _load_history; // 负载历史
      mutable std::mutex _history_mutex; // 历史互斥锁
      static constexpr std::size_t MAX_HISTORY_SIZE = 100; // 最大历史记录数

      std::atomic<double> _learning_rate{0.1}; // 学习率
      std::atomic<bool> _prediction_enabled{true}; // 预测启用标志
    public:
      /**
       * @brief 构造自适应调度器
       * @param task_queue 任务队列
       * @param expansion_strategy 扩缩容策略
       */
      scheduler_adaptive(_interior_cohort_ptr task_queue,expansion_strategy expansion_strategy = expansion_strategy::predictive)
        : scheduler_base(std::move(task_queue),scheduling_tactics::adaptive, expansion_strategy)
      {
        _worker_factory = [this](const std::string &worker_id) 
        {
          return internals::structure_w::make_worker_adaptive(worker_id, _task_queue);
        };
      }
      /**
       * @brief 设置学习率
       * @param rate 学习率(0.0-1.0)
       */
      void set_learning_rate(double rate)
      {
        _learning_rate.store(std::clamp(rate, 0.0, 1.0), std::memory_order_relaxed);
      }
      /**
       * @brief 获取学习率
       * @return 当前学习率
       */
      double get_learning_rate() const
      {
        return _learning_rate.load(std::memory_order_relaxed);
      }
      /**
       * @brief 启用/禁用预测
       * @param enabled 是否启用
       */
      void set_prediction_enabled(bool enabled)
      {
        _prediction_enabled.store(enabled, std::memory_order_relaxed);
      }

      /**
       * @brief 获取任务模式统计
       * @return 任务模式映射
       */
      std::unordered_map<std::string, task_pattern> get_task_patterns() const
      {
        std::shared_lock<std::shared_mutex> lock(_patterns_mutex);
        return _task_patterns;
      }
    protected:
      /**
       * @brief 调度任务实现
       * @param task 要调度的任务
       * @return true 调度成功，false 调度失败
       */
      bool schedule_task(_interior_task_ptr task) override
      {
        learn_task_pattern(task);

        if (_prediction_enabled.load(std::memory_order_relaxed))
        {
          predict_and_adjust(task);
        }

        // 自适应策略选择
        auto strategy = select_optimal_strategy();
        return execute_strategy(task, strategy);
      }
      /**
       * @brief 更新性能指标
       */
      void update_metrics() override
      {
        scheduler_base::update_metrics();

        // 更新负载历史
        auto current_load = _metrics.calculate_load_score();
        {
          std::lock_guard<std::mutex> lock(_history_mutex);
          _load_history.push(current_load);

          if (_load_history.size() > MAX_HISTORY_SIZE)
          {
            _load_history.pop();
          }
        }
      }

      /**
       * @brief 评估扩缩容需求
       */
      void evaluate_scaling() override
      {
        if (_prediction_enabled.load(std::memory_order_relaxed))
        {
          // 基于预测的扩缩容
          auto predicted_load = predict_future_load();

          if (predicted_load > _scaling_config.scale_up_threshold)
          {
            scale_up();
          }
          else if (predicted_load < _scaling_config.scale_down_threshold)
          {
            scale_down();
          }
        }
        else
        {
          // 回退到基础扩缩容策略
          scheduler_base::evaluate_scaling();
        }
      }
    private:
      /**
       * @brief 学习任务模式
       * @param task 任务
       */
      void learn_task_pattern(_interior_task_ptr task)
      {
        auto task_name = task->get_task_name();
        auto now = std::chrono::steady_clock::now();

        std::unique_lock<std::shared_mutex> lock(_patterns_mutex);
        auto &pattern = _task_patterns[task_name];

        pattern.execution_count++;
        pattern.last_seen = now;

        // 简单的指数移动平均
        // auto learning_rate = _learning_rate.load(std::memory_order_relaxed);
        if (pattern.execution_count == 1)
        {
          pattern.average_duration = std::chrono::milliseconds(10); // 默认值
        }
        
        if(_task_patterns.size() == 100)
        {
          _task_patterns.clear();
        }
        // 实际的学习逻辑会在任务完成后更新
      }

      /**
       * @brief 预测并调整
       * @param task 任务
       */
      void predict_and_adjust(_interior_task_ptr task)
      {
        auto predicted_duration = predict_task_duration(task);
        auto current_load = _metrics.calculate_load_score();

        // 基于预测调整任务优先级或延迟
        if (predicted_duration > std::chrono::milliseconds(1000) && current_load > 0.8)
        {
          // 长任务在高负载时降低优先级
          if (task->get_priority() > static_cast<std::int32_t>(internals::structure_u::weight::low))
          {
            task->set_priority(static_cast<internals::structure_u::weight>
              (static_cast<int>(task->get_priority()) - 1));
          }
        }
      }

      /**
       * @brief 选择最优策略
       * @return 调度策略
       */
      scheduling_tactics select_optimal_strategy()
      {
        auto load_score = _metrics.calculate_load_score();
        // auto queue_size = _metrics.queue_length.load(std::memory_order_relaxed);

        if (load_score < 0.3)
        {
          return scheduling_tactics::round_robin;
        }
        else if (load_score < 0.7)
        {
          return scheduling_tactics::least_loaded;
        }
        else
        {
          return scheduling_tactics::priority_based;
        }
      }

      /**
       * @brief 执行调度策略
       * @param task 任务
       * @param strategy 策略
       * @return 调度结果
       */
      bool execute_strategy(_interior_task_ptr task, scheduling_tactics strategy)
      {
        // 根据策略执行不同的调度逻辑
        switch (strategy)
        {
        case scheduling_tactics::round_robin:
        case scheduling_tactics::least_loaded:
        case scheduling_tactics::priority_based:
        default:
          return _task_queue->push(task);
        }
      }

      /**
       * @brief 预测任务执行时间
       * @param task 任务
       * @return 预测时间
       */
      std::chrono::milliseconds predict_task_duration(_interior_task_ptr task)
      {
        std::shared_lock<std::shared_mutex> lock(_patterns_mutex);
        auto it = _task_patterns.find(task->get_task_name());

        if (it != _task_patterns.end())
        {
          return it->second.average_duration;
        }

        return std::chrono::milliseconds(100); // 默认预测值
      }

      /**
       * @brief 预测未来负载
       * @return 预测负载值
       */
      double predict_future_load()
      {
        std::lock_guard<std::mutex> lock(_history_mutex);

        if (_load_history.size() < 3)
        {
          return _metrics.calculate_load_score();
        }

        // 简单的线性预测
        auto history_copy = _load_history;
        double sum = 0.0;
        std::size_t count = 0;

        while (!history_copy.empty() && count < 5) // 使用最近5个数据点
        {
          sum += history_copy.back();
          history_copy.pop();
          count++;
        }

        return count > 0 ? sum / count : _metrics.calculate_load_score();
      }
    };
    /**
     * @brief 创建标准调度器
     * @param task_queue 任务队列
     * @param policy 调度策略
     * @param expansion_strategy 扩缩容策略
     * @return 调度器智能指针
     */
    inline std::unique_ptr<scheduler_base> make_scheduler_standard(_interior_cohort_ptr task_queue,
    scheduling_tactics policy = scheduling_tactics::round_robin,expansion_strategy expansion_strategy = expansion_strategy::reactive)
    {
      return std::make_unique<scheduler_standard>(std::move(task_queue), policy, expansion_strategy);
    }
    /**
     * @brief 创建优先级调度器
     * @param task_queue 任务队列
     * @param expansion_strategy 扩缩容策略
     * @return 调度器智能指针
     */
    inline std::unique_ptr<scheduler_base> make_scheduler_priority(_interior_cohort_ptr task_queue,
    expansion_strategy expansion_strategy = expansion_strategy::conservative)
    {
      return std::make_unique<scheduler_priority>(std::move(task_queue), expansion_strategy);
    }
    /**
     * @brief 创建自适应调度器
     * @param task_queue 任务队列
     * @param expansion_strategy 扩缩容策略
     * @return 调度器智能指针
     */
    inline std::unique_ptr<scheduler_base> make_scheduler_adaptive( _interior_cohort_ptr task_queue,
    expansion_strategy expansion_strategy = expansion_strategy::predictive)
    {
      return std::make_unique<scheduler_adaptive>(std::move(task_queue), expansion_strategy);
    }
    /**
     * @brief 调度器工厂函数
     * @param scheduler_type 调度器类型名称
     * @param task_queue 任务队列
     * @param scheduling_tactics 调度策略
     * @param expansion_strategy 扩缩容策略
     * @return 调度器智能指针
     */
    inline std::unique_ptr<scheduler_base> make_scheduler(const std::string &scheduler_type,_interior_cohort_ptr task_queue,
    scheduling_tactics scheduling_tactics = scheduling_tactics::adaptive,expansion_strategy expansion_strategy = expansion_strategy::hybrid)
    {
      if (scheduler_type == "standard")
      {
        return make_scheduler_standard(std::move(task_queue), scheduling_tactics, expansion_strategy);
      }
      else if (scheduler_type == "priority")
      {
        return make_scheduler_priority(std::move(task_queue), expansion_strategy);
      }
      else if (scheduler_type == "adaptive")
      {
        return make_scheduler_adaptive(std::move(task_queue), expansion_strategy);
      }
      else
      {
        return make_scheduler_adaptive(std::move(task_queue), expansion_strategy);
      }
    }
  }
}

namespace pool
{

  using internals::structure_s::scheduler_adaptive;
  using internals::structure_s::scheduler_priority;
  using internals::structure_s::scheduler_standard;

  using internals::structure_s::make_scheduler;
  using internals::structure_s::make_scheduler_adaptive;
  using internals::structure_s::make_scheduler_priority;
  using internals::structure_s::make_scheduler_standard;

  using internals::structure_s::load_metrics;
  using internals::structure_s::scaling_config;
  using internals::structure_s::expansion_strategy;
  using internals::structure_s::scheduling_tactics;

}