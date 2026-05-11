/**
  * @file pool.hpp
  * @brief 线程池定义
  * @details 提供线程池的定义与操作，包括线程的创建、销毁、调度等功能
  */
#pragma once
#include "unit.hpp"
#include "integration.hpp"
#include "rank.hpp"
#include "worker.hpp"
#include "scheduling.hpp"
#include <iostream>

namespace internals
{
    namespace structure_t{}
}
namespace internals::structure_t
{
    using namespace internals::structure_s;
    using namespace internals::structure_r;
    using namespace internals::structure_u;
    using namespace internals::structure_w;
    
    using safety_scheduler_pointer = std::unique_ptr<scheduler_ordinary>;
    /**
      * @brief 可动态调整的线程池框架
      * @note 支持动态调整线程数量，支持任务优先级，支持任务取消，支持任务超时，支持任务统计，支持任务监控，支持性能分析。
      */
    class thread_pool
    {
    private:
        safety_rank_pointer unit_rank_; //单元队列
        safety_scheduler_pointer scheduler_; //调度器

        // 配置
        pool_config config_; // 线程池配置
        std::atomic<pool_state> state_{pool_state::stopped}; // 线程池状态

        // 统计
        pool_statistics statistics_; // 统计信息

        // 同步
        std::mutex config_mutex_; // 配置互斥锁
        std::condition_variable state_cv_; // 状态条件变量
        mutable std::shared_mutex state_mutex_; // 状态读写锁

        // 监控
        std::unique_ptr<std::jthread> monitor_thread_; // 监控线程
        std::function<void(const pool_statistics &)> statistics_handler_; // 统计处理器
        std::function<void(const std::string &, const std::string &)> event_handler_; // 事件处理器

        // 任务
        mutable std::shared_mutex tasks_mutex_; // 任务映射读写锁
        std::unordered_map<std::string, std::shared_ptr<unit_ordinary>> active_tasks_; // 活跃任务映射
        std::atomic<uint64_t> task_id_counter_{0}; // 任务ID计数器

        // 扩展和插件
        mutable std::mutex plugins_mutex_; // 插件互斥锁
        std::unordered_map<std::string, std::function<void()>> plugins_; // 插件映射

        // 性能分析
        std::atomic<bool> profiling_enabled_{false};   // 性能分析启用标志
        std::unique_ptr<std::jthread> profiler_thread_; // 性能分析线程
        
    public:
        // 性能指标结构
        struct performance_metrics
        {
            std::chrono::steady_clock::time_point timestamp;
            double throughput;
            double queue_utilization;
            std::size_t active_threads;
            std::size_t total_threads;
            std::size_t pending_tasks;
            std::uint64_t completed_tasks;
        };
        
    private:
        // 回调钩子
        std::function<void(const performance_metrics&)> performance_callback_;
        std::function<void(const std::string&)> error_callback_;
        
    public:
        explicit thread_pool(const pool_config &config = pool_config()) : config_(config)
        {
            if(!config_.validate())
                throw std::invalid_argument("Invalid thread pool configuration");
            initialize();
        }
        ~thread_pool() noexcept
        {
            try
            {
                shutdown(std::chrono::milliseconds{500});
            }
            catch (...)
            {
                // 析构函数中不能抛出异常，静默处理
                // 可以记录日志但不能抛出
            }
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
            std::unique_lock<std::shared_mutex> lock(state_mutex_);

            if (state_.load() != pool_state::stopped)
            {
                return false;
            }

            state_.store(pool_state::starting);

            try
            {
                // 启动调度器
                if (!scheduler_->start(config_.initial_threads_))
                {
                    state_.store(pool_state::error);
                    return false;
                }

                // 启动监控线程
                if (config_.enable_monitoring_)
                {
                    start_monitoring();
                }

                // 启动性能分析
                if (config_.enable_performance_profiling_)
                {
                    start_profiling();
                }

                statistics_.reset();
                state_.store(pool_state::running);
                state_cv_.notify_all();

                emit_event("lifecycle", "Thread pool started with " + std::to_string(config_.initial_threads_) + " threads");

                return true;
            }
            catch (const std::exception &e)
            {
                // 异常安全：清理已启动的组件
                cleanup_on_error();
                
                state_.store(pool_state::error);
                emit_event("error", "Failed to start thread pool: " + std::string(e.what()));
                return false;
            }
            catch (...)
            {
                // 处理未知异常
                cleanup_on_error();
                
                state_.store(pool_state::error);
                emit_event("error", "Failed to start thread pool: unknown exception");
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
            std::unique_lock<std::shared_mutex> state_lock(state_mutex_);

            auto currentstate_ = state_.load();
            if (currentstate_ == pool_state::stopped || currentstate_ == pool_state::stopping)
            {
                return true;
            }

            state_.store(pool_state::stopping);

            try
            {
                // 停止接受新任务
                if (unit_rank_)
                    unit_rank_->close();

                // 等待任务完成或超时
                if (wait_for_completion)
                {
                    // 释放状态锁，避免wait_for_all_tasks中的死锁
                    state_lock.unlock();
                    wait_for_all_tasks(config_.shutdown_timeout_);
                    state_lock.lock();
                }

                // 停止调度器
                if (scheduler_)
                    scheduler_->stop(wait_for_completion);

                // 清理活跃任务映射（保持锁顺序：先state_lock，再tasks_mutex）
                {
                    std::unique_lock<std::shared_mutex> tasks_lock(tasks_mutex_);
                    active_tasks_.clear();
                }

                // 停止监控和分析线程
                stop_monitoring();
                stop_profiling();

                state_.store(pool_state::stopped);
                state_cv_.notify_all();

                emit_event("lifecycle", "Thread pool stopped");

                return true;
            }
            catch (const std::exception &e)
            {
                try 
                {
                    force_cleanup();
                } catch (...) {}
                
                state_.store(pool_state::error);
                emit_event("error", "Failed to stop thread pool: " + std::string(e.what()));
                return false;
            }
            catch (...)
            {
                try 
                {
                    force_cleanup();
                } catch (...) {}
                
                state_.store(pool_state::error);
                emit_event("error", "Failed to stop thread pool: unknown exception");
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
            std::unique_lock<std::shared_mutex> lock(state_mutex_);

            if (state_.load() != pool_state::running)
            {
                return false;
            }

            state_.store(pool_state::pausing);

            try
            {
                // 暂停任务队列
                if (unit_rank_)
                {
                    unit_rank_->close();
                }

                state_.store(pool_state::paused);
                state_cv_.notify_all();

                emit_event("lifecycle", "Thread pool paused");
                return true;
            }
            catch (const std::exception &e)
            {
                state_.store(pool_state::error);
                emit_event("error", "Failed to pause thread pool: " + std::string(e.what()));
                return false;
            }
        }
        /**
          * @brief 恢复线程池
          * @return `true` 恢复成功，`false` 恢复失败
          */
        bool resume()
        {
            std::unique_lock<std::shared_mutex> lock(state_mutex_);

            if (state_.load() != pool_state::paused)
            {
                return false;
            }

            try
            {
                // 检查队列状态并重新创建
                if (!unit_rank_ || unit_rank_->closed())
                {
                    unit_rank_ = make_rank(config_.queue_policy_, config_.max_queue_size_);
                    if (!unit_rank_)
                    {
                        emit_event("error", "Failed to create execution_unit queue during resume");
                        return false;
                    }
                }

                if (scheduler_)
                {
                    // 停止旧调度器
                    scheduler_->stop(false);
                    
                    // 创建新调度器
                    scheduler_ = make_scheduler_ordinary(unit_rank_, config_.scheduling_tactics_, config_.expansion_strategy_);
                    
                    // 设置调度器配置
                    scaling_config scaling_cfg;
                    scaling_cfg.min_threads = config_.min_threads_;
                    scaling_cfg.max_threads = config_.max_threads_;
                    scaling_cfg.core_threads = config_.core_threads_;
                    scheduler_->set_scaling_config(scaling_cfg);
                    
                    auto event_callback = [this](const std::string& event) 
                    {
                        emit_event("scheduler", event);
                    };
                    scheduler_->set_event_callback(event_callback);
                }

                state_.store(pool_state::running);
                state_cv_.notify_all();

                emit_event("lifecycle", "Thread pool resumed");
                return true;
            }
            catch (const std::exception &e)
            {
                state_.store(pool_state::error);
                emit_event("error", "Failed to resume thread pool: " + std::string(e.what()));
                return false;
            }
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
            // 先等待任务完成，然后停止线程池
            if (!wait_for_all_tasks(timeout))
            {
                // 超时后强制停止
                return stop(false);
            }
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
            {
                throw std::runtime_error("Thread pool is not running");
            }
            auto execution_unit = make_unit_standard(std::bind(std::forward<function>(func), std::forward<Args>(args)...));

            auto future = execution_unit->get_future();

            if (!submit_unit_internal(execution_unit))
            {
                throw std::runtime_error("Failed to submit execution_unit");
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
        std::size_t submit_invalid(function &&func, Args &&...args)
        {
            if (!is_running())
            {
                throw std::runtime_error("Thread pool is not running");
            }

            auto execution_unit = make_unit_standard(std::bind(std::forward<function>(func), std::forward<Args>(args)...));

            auto task_id = execution_unit->get_identifier();

            if (!submit_unit_internal(execution_unit))
            {
                throw std::runtime_error("Failed to submit execution_unit");
            }

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
            {
                throw std::runtime_error("Thread pool is not running");
            }

            auto execution_unit = make_unit_standard(std::bind(std::forward<function>(func), std::forward<Args>(args)...),priority);

            auto future = execution_unit->get_future();

            if (!submit_unit_internal(execution_unit))
            {
                throw std::runtime_error("Failed to submit priority execution_unit");
            }
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
            {
                throw std::runtime_error("Thread pool is not running");
            }

            auto execution_unit = make_task_time(std::bind(std::forward<function>(func), std::forward<Args>(args)...),timeout);

            auto future = std::move(execution_unit->get_future());

            if (!submit_unit_internal(execution_unit))
            {
                throw std::runtime_error("Failed to submit timeout execution_unit");
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

            auto execution_unit = make_unit_overtime(
                    std::bind(std::forward<function>(func), std::forward<Args>(args)...), 
                    delay,
                    [](){
                        // 超时回调函数（空实现）
                        // 延迟任务超时处理，可以在这里添加日志或其他处理逻辑
                    }
            );

            auto future = execution_unit->get_future();

            // 提交延迟任务到调度器
            if (!submit_unit_internal(execution_unit))
            {
                throw std::runtime_error("Failed to submit delayed execution_unit");
            }

            return future;
        }
    private:
        void initialize()
        {
            // 创建任务队列
            unit_rank_ = make_rank(config_.queue_policy_, config_.max_queue_size_);
            if (!unit_rank_)
            {
                throw std::runtime_error("Failed to create execution_unit queue");
            }
            
            // 创建调度器
            scheduler_ = make_scheduler_ordinary( unit_rank_, config_.scheduling_tactics_, config_.expansion_strategy_);
            if (!scheduler_)
            {
                throw std::runtime_error("Failed to create scheduler");
            }
            
            // 设置调度器配置
            scaling_config scaling_cfg;
            scaling_cfg.min_threads = config_.min_threads_;
            scaling_cfg.max_threads = config_.max_threads_;
            scaling_cfg.core_threads = config_.core_threads_;
            scheduler_->set_scaling_config(scaling_cfg);
            
            auto event_callback = [this](const std::string& event) 
            {
                emit_event("scheduler", event);
            };
            scheduler_->set_event_callback(event_callback);
            
            statistics_.reset();
        }
        bool submit_unit_internal(safety_unit_pointer execution_unit)
        {
            if (!execution_unit)
            {
                emit_event("error", "Attempted to submit null execution_unit");
                return false;
            }

            // 检查线程池状态
            if (state_.load(std::memory_order_acquire) != pool_state::running)
            {
                emit_event("warning", "Cannot submit execution_unit: thread pool is not running");
                return false;
            }

            // 生成唯一任务ID（优化：减少字符串操作）
            auto unique_task_id = task_id_counter_.fetch_add(1, std::memory_order_relaxed);
            
            // 优化：只在需要跟踪任务时才添加到映射表
            bool need_tracking = config_.enable_monitoring_ || config_.enable_performance_profiling_;
            std::string task_id_str;
            
            if (need_tracking)
                task_id_str = "task_" + std::to_string(unique_task_id);
            
            try
            {
                bool result = scheduler_->submit_uint(execution_unit);

                if (result)
                {
                    statistics_.total_tasks_submitted_.fetch_add(1, std::memory_order_relaxed);
                    statistics_.last_task_time_ = std::chrono::steady_clock::now();
                    
                    if (need_tracking)
                    {
                        std::unique_lock<std::shared_mutex> lock(tasks_mutex_);
                        active_tasks_[task_id_str] = execution_unit;
                        emit_event("task_submitted", "Task " + task_id_str + " submitted successfully");
                    }
                }
                else
                {
                    if (need_tracking) 
                        emit_event("error", "Failed to submit unit " + task_id_str + " to scheduler");
                }
                return result;
            }
            catch (const std::exception& e)
            {
                statistics_.total_tasks_failed_.fetch_add(1, std::memory_order_relaxed);
                emit_event("error", "Exception in submit_unit_internal for execution_unit " + task_id_str + ": " + e.what());
                return false;
            }
            catch (...)
            {
                statistics_.total_tasks_failed_.fetch_add(1, std::memory_order_relaxed);
                emit_event("error", "Unknown exception in submit_unit_internal for execution_unit " + task_id_str);
                return false;
            }
        }
        void start_monitoring()
        {
            auto monitoring_functions = [this]()
            {
                // 优化：使用更高效的循环间隔和批量处理
                auto last_cleanup = std::chrono::steady_clock::now();
                const auto cleanup_interval = std::chrono::seconds(3); // 减少清理频率
                
                while (state_.load(std::memory_order_relaxed) == pool_state::running)
                {
                    auto now = std::chrono::steady_clock::now();
                    
                    // 每次都更新统计信息（轻量级操作）
                    updatestatistics_();
                    
                    // 定期清理过期任务（重量级操作）
                    if (now - last_cleanup >= cleanup_interval)
                    {
                        cleanup_stale_tasks();
                        last_cleanup = now;
                    }
                    
                    // 只在有处理器时才调用
                    if (statistics_handler_)
                        statistics_handler_(statistics_);
                    
                    std::this_thread::sleep_for(config_.monitoring_interval_);
                }
            };
            monitor_thread_ = std::make_unique<std::jthread>(std::move(monitoring_functions));
        }
        /**
          * @brief 停止监控
          */
        void stop_monitoring()
        {
            if (monitor_thread_ && monitor_thread_->joinable())
            {
                monitor_thread_->join();
            }
        }
        /**
          * @brief 启动性能分析
          */
        void start_profiling()
        {
            profiling_enabled_.store(true, std::memory_order_relaxed);
            auto performance_analysis = [this]()
            {
                const auto profiling_interval = std::chrono::milliseconds(1000);
                
                while (profiling_enabled_.load(std::memory_order_relaxed))
                {
                    try
                    {
                        // 收集基本性能指标
                        performance_metrics metrics;
                        metrics.timestamp = std::chrono::steady_clock::now();
                        metrics.throughput = statistics_.current_throughput_.load(std::memory_order_relaxed);
                        metrics.queue_utilization = get_rank_utilization();
                        metrics.active_threads = statistics_.active_thread_count_.load(std::memory_order_relaxed);
                        metrics.total_threads = statistics_.current_thread_count_.load(std::memory_order_relaxed);
                        metrics.pending_tasks = get_rank_size();
                        metrics.completed_tasks = statistics_.total_tasks_completed_.load(std::memory_order_relaxed);
                        
                        // 调用性能分析钩子
                        if (performance_callback_)
                            performance_callback_(metrics);
                        
                        std::this_thread::sleep_for(profiling_interval);
                    }
                    catch (const std::exception& e)
                      {
                          if (error_callback_)
                              error_callback_("Profiling error: " + std::string(e.what()));
                      }
                }
            };
            profiler_thread_ = std::make_unique<std::jthread>(std::move(performance_analysis));
        }
        /**
          * @brief 停止性能分析
          */
        void stop_profiling()
        {
            profiling_enabled_.store(false, std::memory_order_relaxed);
            if (profiler_thread_ && profiler_thread_->joinable())
            {
                profiler_thread_->join();
            }
        }
        /**
          * @brief 更新统计信息
          */
        void updatestatistics_()
        {
            // 更新线程统计
            auto current_threads = scheduler_->get_thread_count();
            auto active_threads = scheduler_->get_active_thread_count();
            
            statistics_.current_thread_count_.store(current_threads, std::memory_order_relaxed);
            statistics_.active_thread_count_.store(active_threads, std::memory_order_relaxed);
            
            // 使用compare_exchange_weak避免峰值线程数更新的竞态条件
            auto expected_peak_threads = statistics_.peak_thread_count_.load(std::memory_order_relaxed);
            while (current_threads > expected_peak_threads && 
            !statistics_.peak_thread_count_.compare_exchange_weak(expected_peak_threads, current_threads))
            {
                // 循环直到成功更新或发现更大的峰值
            }

            // 更新队列统计
            auto queue_size = unit_rank_->size();
            statistics_.current_queue_size_.store(queue_size, std::memory_order_relaxed);

            // 使用compare_exchange_weak避免峰值队列大小更新的竞态条件
            auto expected_peak = statistics_.peak_queue_size_.load(std::memory_order_relaxed);
            while (queue_size > expected_peak && 
            !statistics_.peak_queue_size_.compare_exchange_weak(expected_peak, queue_size))
            {
                // 循环直到成功更新或发现更大的峰值
            }

            calculate_throughput();
        }
        //计算吞吐量
        void calculate_throughput()
        {
            auto now = std::chrono::steady_clock::now();
            auto current_completed = statistics_.total_tasks_completed_.load(std::memory_order_relaxed);

            // 使用原子操作确保线程安全的时间戳更新
            auto last_time = statistics_.last_throughput_time_.load(std::memory_order_relaxed);
            auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() - last_time;
            
            if (time_diff >= 1000) // 每秒计算一次
            {
                // 原子地更新时间戳，避免重复计算
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                if (statistics_.last_throughput_time_.compare_exchange_strong(last_time, now_ms, std::memory_order_relaxed))
                {
                    auto last_completed = statistics_.last_completed_count_.load(std::memory_order_relaxed);
                    auto task_diff = current_completed - last_completed;
                    auto throughput = static_cast<double>(task_diff) / (time_diff / 1000.0);

                    statistics_.current_throughput_.store(throughput, std::memory_order_relaxed);
                    statistics_.last_completed_count_.store(current_completed, std::memory_order_relaxed);

                    // 使用compare_exchange_weak避免峰值更新的竞态条件
                    auto expected_peak = statistics_.peak_throughput_.load(std::memory_order_relaxed);
                    while (throughput > expected_peak && 
                    !statistics_.peak_throughput_.compare_exchange_weak(expected_peak, throughput))
                    {
                        // 循环直到成功更新或发现更大的峰值
                    }
                }
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
            const auto sleep_interval = std::chrono::milliseconds(10);

            while (std::chrono::steady_clock::now() - start_time < timeout)
            {
                bool all_tasks_done = false;
                {
                    std::shared_lock<std::shared_mutex> lock(tasks_mutex_);
                    // 检查活跃任务和队列是否都为空
                    all_tasks_done = active_tasks_.empty() && (unit_rank_ ? unit_rank_->empty() : true);
                }
                
                if (all_tasks_done)
                {
                    return true;
                }

                std::this_thread::sleep_for(sleep_interval);
            }
            return false;
        }
        /**
          * @brief 发送事件
          * @param category 事件类别
          * @param message 事件消息
          */
        void emit_event(const std::string &category, const std::string &message) noexcept
        {
            try
            {
                if (event_handler_)
                {
                    event_handler_(category, message);
                }
            }
            catch (...)
            {
                // 事件处理器异常不应影响主逻辑
                // 可以记录到内部日志但不抛出异常
            }
        }
        
        /**
          * @brief 清理过期和孤儿任务
          */
        void cleanup_stale_tasks() noexcept
        {
            try
            {
                std::unique_lock<std::shared_mutex> lock(tasks_mutex_);
                
                auto now = std::chrono::steady_clock::now();
                auto it = active_tasks_.begin();
                
                while (it != active_tasks_.end())
                  {
                      try
                      {
                          auto& execution_unit = it->second;
                          if (!execution_unit)
                          {
                              // 移除空指针任务
                              it = active_tasks_.erase(it);
                              continue;
                          }
                          
                          // 检查任务是否已完成但未清理
                          auto taskstate_ = execution_unit->get_state();
                          if (taskstate_ == current_status::completed || 
                                  taskstate_ == current_status::cancelled ||
                                  taskstate_ == current_status::failed)
                          {
                              emit_event("cleanup", "Removing completed/cancelled/failed execution_unit: " + it->first);
                              it = active_tasks_.erase(it);
                              continue;
                          }
                          
                          // 检查任务是否超时
                          if (config_.task_timeout_.count() > 0)
                          {
                              auto task_age = now - execution_unit->get_submit_time();
                              if (task_age > config_.task_timeout_)
                              {
                                  if (execution_unit->cancel())
                                  {
                                      emit_event("cleanup", "Cancelled timeout execution_unit: " + it->first);
                                      statistics_.total_tasks_cancelled_.fetch_add(1, std::memory_order_relaxed);
                                  }
                                  it = active_tasks_.erase(it);
                                  continue;
                              }
                          }
                          
                          ++it;
                      }
                      catch (...)
                      {
                          // 单个任务清理失败时，跳过该任务继续处理其他任务
                          ++it;
                      }
                  }
            }
            catch (...)
            {
                // 静默处理清理过程中的异常，防止影响线程池稳定性
            }
        }
        
        /**
          * @brief 为任务设置完成回调函数
          * @param execution_unit 任务指针
          * @param callback 完成回调函数
          */
        void set_task_completion_callback(safety_unit_pointer execution_unit, std::function<void()> callback)
        {
            if (!execution_unit || !callback)
            {
                return;
            }
        }
        
        /**
          * @brief 错误时的清理函数
          */
        void cleanup_on_error() noexcept
        {
            try 
            {
                if (scheduler_)
                    scheduler_->stop(false);
                stop_monitoring();
                stop_profiling();
            }
            catch (...)
            {
                // 忽略清理过程中的异常
            }
        }
        
        /**
          * @brief 强制清理资源
          */
        void force_cleanup() noexcept
        {
            try
            {
                // 清理活跃任务映射
                std::unique_lock<std::shared_mutex> tasks_lock(tasks_mutex_);
                active_tasks_.clear();
            }
            catch (...) {}
            
            try
            {
                stop_monitoring();
                stop_profiling();
            }
            catch (...) {}
        }
    public: 
        /**
          * @brief 检查线程池是否正在运行
          * @return true 正在运行，false 未运行
          */
        bool is_running() const
        {
            return state_.load() == pool_state::running;
        }

        /**
          * @brief 获取线程池状态
          * @return 当前状态
          */
        pool_state get_state() const
        {
            return state_.load();
        }

        /**
          * @brief 获取线程池配置
          * @return 配置的常量引用
          */
        const pool_config & get_config() const
        {
            return config_;
        }

        /**
          * @brief 获取统计信息
          * @return 统计信息的常量引用
          */
        const pool_statistics & getstatistics_() const
        {
            return statistics_;
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
            {
                throw std::runtime_error("Thread pool is not running");
            }

            auto execution_unit = make_unit_reliance(std::bind(std::forward<function>(func), std::forward<Args>(args)...),reliance);

            auto future = std::move(execution_unit->get_future());

            if (!submit_unit_internal(execution_unit))
            {
                throw std::runtime_error("Failed to submit dependency execution_unit");
            }
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
            {
                throw std::runtime_error("Thread pool is not running");
            }

            std::size_t submitted_count = 0;

            for(const auto &execution_unit : tasks)
            {
                if (submit_unit_internal(execution_unit))
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
        bool cancel_unit(const std::string &task_id)
        {
            if (state_.load(std::memory_order_acquire) != pool_state::running)
                return false;
            std::unique_lock<std::shared_mutex> lock(tasks_mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end())
            {
                auto execution_unit = it->second;
                if (execution_unit && execution_unit->cancel())
                {
                    active_tasks_.erase(it);
                    statistics_.total_tasks_cancelled_.fetch_add(1, std::memory_order_relaxed);
                    emit_event("task_cancelled", "Task cancelled: " + task_id);
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
        std::size_t cancel_units(const std::vector<std::string> &task_ids)
        {
            if (state_.load(std::memory_order_acquire) != pool_state::running)
                return 0;
            std::size_t cancelled_count = 0;

            for (const auto &task_id : task_ids)
            {
                if (cancel_unit(task_id))
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
        std::size_t cancel_all_pending_units()
        {
            std::size_t cancelled_count = 0;
            std::vector<std::string> cancelled_task_ids;

            {
                std::unique_lock<std::shared_mutex> lock(tasks_mutex_);
                
                auto it = active_tasks_.begin();
                while (it != active_tasks_.end())
                {
                    if (it->second->get_state() == current_status::pending && it->second->cancel())
                    {
                        cancelled_task_ids.push_back(it->first);
                        it = active_tasks_.erase(it);
                        ++cancelled_count;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            
            for (const auto& task_id : cancelled_task_ids)
            {
                emit_event("task_cancelled", "Pending execution_unit cancelled: " + task_id);
            }
            
            statistics_.total_tasks_cancelled_.fetch_add(cancelled_count, std::memory_order_relaxed);
            return cancelled_count;
        }
        /**
          * @brief 获取任务状态
          * @param task_id 任务ID
          * @return 任务状态
          */
        current_status get_unitstate_(const std::string &task_id) const
        {
            std::shared_lock<std::shared_mutex> lock(tasks_mutex_);

            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end())
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
        bool wait_for_unit(const std::string &task_id,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
        {
            std::shared_lock<std::shared_mutex> lock(tasks_mutex_);

            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end())
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
        std::size_t wait_for_units(const std::vector<std::string> &task_ids,
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

                if (wait_for_unit(task_id, remaining_time))
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
        std::vector<std::string> get_active_unit_ids() const
        {
            std::shared_lock<std::shared_mutex> lock(tasks_mutex_);

            std::vector<std::string> unit_ids;
            unit_ids.reserve(active_tasks_.size());

            for (const auto &[unit_id, unit] : active_tasks_)
            {
                unit_ids.push_back(unit_id);
            }

            return unit_ids;
        }
        /**
          * @brief 获取当前线程数
          * @return 线程数量
          */
        std::size_t get_thread_count() const
        {
            return scheduler_->get_thread_count();
        }
        /**
          * @brief 获取活跃线程数
          * @return 活跃线程数量
          */
        std::size_t get_active_thread_count() const
        {
            return scheduler_->get_active_thread_count();
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
            auto new_count = std::min(current_count + count, config_.max_threads_);

            if (new_count > current_count)
            {
                scheduler_->manual_scale_downs(count);
                // statistics_._total_scale_up_operations.fetch_add(1, std::memory_order_relaxed);

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
            auto new_count = std::max(current_count - count, config_.min_threads_);

            if (new_count < current_count)
            {
                scheduler_->manual_scale_down();
                // statistics_._total_scale_down_operations.fetch_add(1, std::memory_order_relaxed);

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

            count = std::clamp(count, config_.min_threads_, config_.max_threads_);
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
        std::size_t get_rank_size() const
        {
            return unit_rank_->size();
        }

        /**
          * @brief 检查队列是否为空
          * @return true 队列为空，false 队列非空
          */
        bool is_rank_empty() const
        {
            return unit_rank_->empty();
        }

        /**
          * @brief 清空任务队列
          * @return 清除的任务数量
          */
        std::size_t clear_rank()
        {
            auto size = unit_rank_->size();
            unit_rank_->clear();

            emit_event("queue", "Cleared " + std::to_string(size) + " tasks from queue");

            return size;
        }

        /**
          * @brief 获取队列容量
          * @return 队列最大容量
          */
        std::size_t get_rank_capacity() const
        {
            return config_.max_queue_size_;
        }

        /**
          * @brief 设置队列最大大小
          * @param max_size 最大大小
          * @return true 设置成功，false 设置失败
          */
        bool set_rank_max_size(std::size_t max_size)
        {
            if (max_size == 0)
            {
                return false;
            }
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_.max_queue_size_ = max_size;
            // 如果队列支持动态调整大小
            auto transitionstate_ = std::dynamic_pointer_cast<rank_ordinary>(unit_rank_);
            if(transitionstate_.get() != nullptr)
            {
                transitionstate_->set_max_size(max_size);
                return true;
            }
            return false;
        }
        /**
          * @brief 获取队列使用率
          * @return 使用率(0.0-1.0)
          */
        double get_rank_utilization() const
        {
            auto current_size = get_rank_size();
            auto max_size = get_rank_capacity();

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
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_.scheduling_tactics_ = policy;

            if (scheduler_)
            {
                scheduler_->set_scheduling_policy(policy);
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
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_.expansion_strategy_ = policy;

            if (scheduler_)
            {
                scheduler_->set_scaling_policy(policy);
            }

            return true;
        }
        /**
          * @brief 设置任务超时时间
          * @param timeout 超时时间
          */
        void set_unit_timeout(std::chrono::milliseconds timeout)
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_.task_timeout_ = timeout;
        }

        /**
          * @brief 设置线程空闲超时时间
          * @param timeout 超时时间
          */
        void setidle_timeout_(std::chrono::milliseconds timeout)
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_.idle_timeout_ = timeout;
        }
        /**
          * @brief 重置统计信息
          */
        void reset_statistics()
        {
            statistics_.reset();
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
                if (!scheduler_ || !scheduler_->is_running())
                {
                    if (scheduler_)
                    {
                        scheduler_->stop(false);
                    }

                    scheduler_ = make_scheduler_ordinary(unit_rank_,config_.scheduling_tactics_, config_.expansion_strategy_);
                    scheduler_->start(config_.initial_threads_);
                }

                // 检查线程数量
                auto thread_count = get_thread_count();
                if (thread_count < config_.min_threads_)
                {
                    scale_up(config_.min_threads_ - thread_count);
                }
                else if (thread_count > config_.max_threads_)
                {
                    scale_down(thread_count - config_.max_threads_);
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
        
        /**
          * @brief 设置性能分析回调
          * @param callback 性能分析回调函数
          */
        void setperformance_callback_(std::function<void(const performance_metrics&)> callback)
        {
            performance_callback_ = std::move(callback);
        }
        
        /**
          * @brief 设置错误回调
          * @param callback 错误回调函数
          */
        void seterror_callback_(std::function<void(const std::string&)> callback)
        {
            error_callback_ = std::move(callback);
        }
        
        /**
          * @brief 设置事件处理器
          * @param handler 事件处理函数，接收事件类别和消息
          */
        void setevent_handler_(std::function<void(const std::string&, const std::string&)> handler)
        {
            event_handler_ = std::move(handler);
        }
        
        /**
          * @brief 设置统计处理器
          * @param handler 统计处理函数
          */
        void setstatistics_handler_(std::function<void(const pool_statistics&)> handler)
        {
            statistics_handler_ = std::move(handler);
        }
        
        /**
          * @brief 健康检查
          * @return true 健康，false 不健康
          */
        bool health_check() const
        {
            if (state_.load() != pool_state::running)
              return false;
            
            if (!scheduler_ || !scheduler_->is_running())
              return false;
            
            // 检查线程数量是否在合理范围内
            auto thread_count = get_thread_count();
            if (thread_count < config_.min_threads_ || thread_count > config_.max_threads_)
                return false;
            
            // 检查队列是否过载
            auto queue_utilization = get_rank_utilization();
            if (queue_utilization > 0.95)  // 队列使用率超过95%
                return false;
            
            return true;
        }
    }; //  thread_pool class

    /**
      * @brief 创建标准线程池
      * @param thread_count 线程数量
      * @param queue_size 队列大小
      * @return 线程池智能指针
      */
    inline std::unique_ptr<thread_pool> make_thread_pool(std::size_t thread_count,std::size_t queue_size = 10000)
    {
        pool_config config;
        config.max_queue_size_     = queue_size;
        config.initial_threads_    = thread_count;
        config.min_threads_        = 1;
        config.max_threads_        = thread_count;
        config.core_threads_       = thread_count;
        config.queue_policy_       = rank_strategy::fifo;
        config.scheduling_tactics_ = scheduling_tactics::round_robin;
        config.expansion_strategy_ = expansion_strategy::aggressive;
        config.enable_monitoring_    = false;
        config.enable_performance_profiling_ = false;
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
    inline std::unique_ptr<thread_pool> make_performance_pool(std::size_t thread_count)
    {
        pool_config config;
        config.initial_threads_      = thread_count;
        config.min_threads_          = thread_count;
        config.max_threads_          = thread_count * 2;
        config.core_threads_         = thread_count;
        config.queue_policy_         = rank_strategy::priority;
        config.scheduling_tactics_   = scheduling_tactics::adaptive;
        config.expansion_strategy_   = expansion_strategy::aggressive;
        config.enable_monitoring_    = true;
        config.enable_performance_profiling_ = true;

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
        config.initial_threads_    = thread_count;
        config.min_threads_        = thread_count;
        config.max_threads_        = thread_count;
        config.core_threads_       = thread_count;
        config.queue_policy_       = rank_strategy::fifo;
        config.scheduling_tactics_ = scheduling_tactics::round_robin;
        config.expansion_strategy_ = expansion_strategy::conservative;
        config.enable_monitoring_    = false;
        config.enable_performance_profiling_ = false;

        if(config.validate())
        {
            return std::make_unique<thread_pool>(config);
        }
        return nullptr;
    }

} //  structure_t

namespace wan
{
    /**
      * @brief 线程池模块
      * @note 提供线程池的创建、管理、监控等功能
      */
    namespace pool
    {
        using internals::structure_t::make_thread_pool;
        using internals::structure_t::make_lightweight_pool;
        using internals::structure_t::make_performance_pool;

        using internals::structure_t::thread_pool;
        using namespace internals::structure_t;
    }
}
