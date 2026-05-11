/**
  * @file worker.hpp
  * @brief 工作线程定义
  * @details 提供工作线程的定义与操作，包括线程的创建、销毁、调度等功能
  */
#pragma once
#include "unit.hpp"
#include "rank.hpp"
#include "integration.hpp"
#include <thread>
#include <string>
#include <atomic>
#include <memory>
#include <iostream>
#include <condition_variable>

namespace internals
{
    namespace structure_w{}
}
namespace internals::structure_w
{
    using safety_unit_pointer = internals::structure_r::safety_unit_pointer;
    using safety_rank_pointer = std::shared_ptr<internals::structure_r::rank_ordinary>;
    /**
      * @class worker_ordinary
      * @brief 工作线程基类
      * 
      * 定义工作线程的基本接口和行为，所有具体的工作线程类型都继承自此类
      * 
      * 设计模式： 模板方法模式：定义线程执行流程，策略模式：支持不同的任务获取策略
      * 
      * 调用关系：被`thread_pool`管理和调用， 从`rank_ordinary`获取任务， 执行`unit_ordinary`及其派生类
      */
class worker_ordinary
    {
    protected:
        /// @brief 
        std::unique_ptr<std::jthread> worker_thread_; // 线程对象

        std::atomic<bool> stop_{false}; // 停止标志
        std::atomic<bool> detached_{false}; // 分离标志
        std::atomic<worker_state> state_{worker_state::idle}; // 状态标志

        std::string worker_name_; // 线程名称
        worker_statistics statistics_; // 统计信息

        std::shared_mutex state_mutex_; // 状态互斥锁
        std::condition_variable condition_; // 条件变量

        safety_rank_pointer unit_rank_; // 任务队列

        std::function<void(const std::string&, safety_unit_pointer)> unit_starts_callback_; // 任务开始回调
        std::function<void(const std::string&, safety_unit_pointer)> unit_finish_callback_; // 任务完成回调

        std::function<void()> worker_starts_callback_; // 线程开始回调
        std::function<void()> worker_finish_callback_; // 线程完成回调

        std::function<void(const std::string&, const std::exception&)> abnormal_callback_; // 任务异常回调 
    public:
        worker_ordinary(const std::string& name, safety_rank_pointer rank) 
        : worker_name_(name), unit_rank_(std::move(rank)) {}
        virtual ~worker_ordinary()
        {
            if(!detached_.load(std::memory_order_acquire))
            {
                stop();
                if(worker_thread_ && worker_thread_->joinable())
                    worker_thread_->join();
            }
        }
        worker_ordinary(const worker_ordinary &) = delete;
        worker_ordinary &operator=(const worker_ordinary &) = delete;
        worker_ordinary(worker_ordinary &&) = delete;
        worker_ordinary &operator=(worker_ordinary &&) = delete;
        /**
          * @brief 启动工作线程
          * @return `true` 启动成功，`false` 启动失败
          */
        virtual bool start() 
        {
            std::unique_lock<std::shared_mutex> lock(state_mutex_);
            if (state_.load(std::memory_order_acquire) != worker_state::idle)
            {
                return false;
            }
            try
            {
                stop_.store(false, std::memory_order_release);

                worker_thread_ = std::make_unique<std::jthread>(&worker_ordinary::interior_run, this);

                state_.store(worker_state::running, std::memory_order_release);
                statistics_.start_time_ = std::chrono::steady_clock::now();
                lock.unlock();
                condition_.notify_all();
                return true;
            }
            catch (const std::exception &e)
            {
                state_.store(worker_state::error, std::memory_order_release);
                if (abnormal_callback_)
                {
                    abnormal_callback_(worker_name_, e);
                }
                else
                {
                    std::cerr << e.what() << '\n';
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
            if(!detached_.load(std::memory_order_acquire))
            {
                stop_.store(true, std::memory_order_release);

                {
                    std::unique_lock<std::shared_mutex> lock(state_mutex_);
                    state_.store(worker_state::stopping, std::memory_order_release);
                }
                condition_.notify_all();

                if (worker_thread_ && worker_thread_->joinable() && wait_for_completion)
                {
                    worker_thread_->join();
                }
            }
        }
        // 分离工作线程
        virtual void detach()
        {
            if (worker_thread_ && worker_thread_->joinable())
            {
                worker_thread_->detach();
                detached_.store(true, std::memory_order_release);
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
            std::unique_lock<std::shared_mutex> lock(state_mutex_);
            auto state_function = [this]()
            {
                auto state = state_.load(std::memory_order_acquire);
                return state == worker_state::stopped || state == worker_state::error;
            };
            return condition_.wait_for(lock, timeout, state_function);
        }

        const std::string& get_worker_name() const {return worker_name_;}
        worker_state get_state() const {return state_.load(std::memory_order_acquire); }
        const worker_statistics &get_statistics() const {return statistics_;}
        void reset_statistics() {statistics_.reset();}
        /**
          * @brief 检查线程是否正在运行
          * @return `true` 正在运行，`false` 未运行
          */
        bool is_running() const
        {
            auto state = state_.load(std::memory_order_acquire);
            return state == worker_state::running;
        }
        /**
          * @brief 检查线程是否已停止
          * @return `true` 已停止，`false` 未停止
          */
        bool is_stopped() const
        {
            auto state = state_.load(std::memory_order_acquire);
            return state == worker_state::stopped;
        }
        void set_abnormal_callback(std::function<void(const std::string&, const std::exception& )> handler)
        {
            abnormal_callback_ = std::move(handler);
        }
        void set_start_callback(std::function<void(const std::string &,safety_unit_pointer)> callback)
        {
            unit_starts_callback_ = std::move(callback);
        }
        void set_finish_callback(std::function<void(const std::string &,safety_unit_pointer)> callback)
        {
            unit_finish_callback_ = std::move(callback);
        }
        std::thread::id get_thread_id() const
        {
            if (worker_thread_)
            {
                return worker_thread_->get_id();
            }
            return std::thread::id{};
        }
        void set_thread_start(std::function<void()> callback)
        {
            worker_starts_callback_ = std::move(callback);
        }
        void set_thread_stop(std::function<void()> callback)
        {
            worker_finish_callback_ = std::move(callback);
        }
    protected:
        // 线程内部运行函数
        virtual void interior_run()
        {
            try
            {
                call_thread_start();
                while (!stop_.load(std::memory_order_acquire))
                {
                    auto task = get_next_task();
                    if (task)
                        execute_task(task);
                    else
                        handle_no_task();
                }
                call_thread_stop();
            }
            catch(const std::exception& e)
            {
                state_.store(worker_state::error, std::memory_order_release);
                if (abnormal_callback_) abnormal_callback_(worker_name_, e);
                else std::cerr << "Worker " << worker_name_ << " encountered exception: " << e.what() << std::endl;
            }
            {
                std::unique_lock<std::shared_mutex> lock(state_mutex_);
                state_.store(worker_state::stopped, std::memory_order_release);
            }
            condition_.notify_all();
        }
        virtual safety_unit_pointer get_next_task()
        {
            if (unit_rank_)
                return unit_rank_->pop();
            return nullptr;
        }
        virtual void execute_task(safety_unit_pointer task)
        {
            if (!task) return;
            auto start_time = std::chrono::steady_clock::now();

            try
            {
                if (unit_starts_callback_)
                    unit_starts_callback_(worker_name_, task);

                if (task->is_timeout() == false && task->is_timeout())
                {
                    task->mark_timeout();
                    statistics_.tasks_failed.fetch_add(1, std::memory_order_relaxed);
                    return;
                }

                task->execute();

                auto end_time = std::chrono::steady_clock::now();
                auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

                statistics_.tasks_executed.fetch_add(1, std::memory_order_relaxed);
                statistics_.total_execution_time_.fetch_add(execution_time, std::memory_order_relaxed);
                statistics_.last_task_time_ = end_time;

                if (unit_finish_callback_)
                    unit_finish_callback_(worker_name_, task);
            }
            catch (const std::exception &e)
            {
                statistics_.tasks_failed.fetch_add(1, std::memory_order_relaxed);

                if (abnormal_callback_)
                    abnormal_callback_(worker_name_, e);
                else
                    throw;
            }
        }
        // 处理无任务情况
        virtual void handle_no_task()
        {
            auto idle_start = std::chrono::steady_clock::now();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            auto idle_end = std::chrono::steady_clock::now();
            auto idle_time = std::chrono::duration_cast<std::chrono::microseconds>(idle_end - idle_start).count();
            statistics_.total_idle_time_.fetch_add(idle_time, std::memory_order_relaxed);
        }
        // 线程启动时调用
        virtual void call_thread_start()
        {
            if(worker_starts_callback_)
                worker_starts_callback_();
        }
        // 线程停止时调用
        virtual void call_thread_stop()
        {
            if(worker_finish_callback_)
                worker_finish_callback_();
        }
    };
    class worker_adaptive : public worker_ordinary
    {
    private:
        static constexpr std::size_t MAX_SLEEP_TIME_MS = 100; ///< 最大休眠时间(毫秒)
        std::atomic<double> load_factor_{0.0}; // 负载因子
        std::atomic<std::size_t> consecutive_empty_polls_{0};  // 连续空轮询次数
        std::atomic<std::chrono::milliseconds> adaptive_sleep_time_{std::chrono::milliseconds(1)}; // 自适应休眠时间 
    public:
        worker_adaptive(const std::string& name, safety_rank_pointer rank) : worker_ordinary(name,std::move(rank)){}
        double getload_factor_() const
        {
            return load_factor_.load(std::memory_order_acquire);
        }
        void setload_factor_(double load_factor)
        {
            load_factor_.store(load_factor, std::memory_order_release);
        }
        std::chrono::milliseconds getadaptive_sleep_time_() const
        {
            return adaptive_sleep_time_.load(std::memory_order_acquire);
        }
        void setadaptive_sleep_time_(std::chrono::milliseconds sleep_time)
        {
            adaptive_sleep_time_.store(sleep_time, std::memory_order_release);
        }
    protected:
        safety_unit_pointer get_next_task() override
        {
            if (!unit_rank_)
                return nullptr;
            auto load = load_factor_.load(std::memory_order_acquire);
            auto timeout = std::chrono::milliseconds(static_cast<long>(50 + load * 50));

            auto task = unit_rank_->try_pop_for(timeout);
            if (task)
            {
                // 获取到任务，重置空轮询计数
                consecutive_empty_polls_.store(0, std::memory_order_relaxed);
                updateload_factor_(true);
            }
            else
            {
                // 未获取到任务，增加空轮询计数
                auto empty_polls = consecutive_empty_polls_.fetch_add(1, std::memory_order_relaxed);
                updateload_factor_(false);
                adjust_sleep_time(empty_polls + 1);
            }
            return task;
        }
        void handle_no_task() override
        {
            auto sleep_time = adaptive_sleep_time_.load(std::memory_order_acquire);

            auto idle_start = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(sleep_time);
            auto idle_end = std::chrono::steady_clock::now();

            auto idle_time = std::chrono::duration_cast<std::chrono::microseconds>(idle_end - idle_start).count();
            statistics_.total_idle_time_.fetch_add(idle_time, std::memory_order_relaxed);
        }
    private:
        void adjust_sleep_time(std::size_t empty_polls)
        {
            std::size_t sleep_ms = std::min(empty_polls / 10, MAX_SLEEP_TIME_MS);
            adaptive_sleep_time_.store(std::chrono::milliseconds(sleep_ms), std::memory_order_release);
        }
        void updateload_factor_(bool got_task)
        {
            // 使用指数移动平均更新负载因子
            constexpr double alpha = 0.1; // 平滑因子
            auto current_load = load_factor_.load(std::memory_order_acquire);
            auto new_sample = got_task ? 1.0 : 0.0;
            auto new_load = alpha * new_sample + (1.0 - alpha) * current_load;
            load_factor_.store(new_load, std::memory_order_release);
        }
    };
    std::unique_ptr<worker_adaptive> make_worker_adaptive(const std::string& worker_name, safety_rank_pointer worker_rank)
    {
        return std::make_unique<worker_adaptive>(worker_name, std::move(worker_rank));
    }
    std::unique_ptr<worker_ordinary> make_worker_ordinary(const std::string& worker_name, safety_rank_pointer worker_rank)
    {
        return std::make_unique<worker_ordinary>(worker_name, std::move(worker_rank));
    }
}