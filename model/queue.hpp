/**
 * @file task_queue.hpp
 * @brief 线程安全任务队列系统 - 支持多种队列类型和调度策略
 * @author Thread Pool Framework
 * @date 2024
 * 
 * 本文件实现了线程池的任务队列系统，包括：
 * - 基础任务队列接口
 * - 普通FIFO任务队列
 * - 优先级任务队列
 * - 延迟任务队列
 * - 多级队列调度器
 * 
 * 依赖关系：
 * - 依赖于task.hpp中的任务类型定义
 * - 被worker.hpp和scheduler.hpp调用
 * - 使用标准库的并发容器和同步原语
 */

#pragma once

#include "task.hpp"
#include <queue>
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <functional>
#include <thread>
#include <semaphore>

namespace thread_pool
{
    /**
     * @enum queue_policy
     * @brief 队列调度策略枚举
     * 
     * 定义了不同的任务队列调度策略：
     * - fifo: 先进先出策略
     * - priority: 优先级调度策略
     * - delay: 延迟调度策略
     * - round_robin: 轮询调度策略
     */
    enum class queue_policy
    {
        fifo,           ///< 先进先出策略
        priority,       ///< 优先级调度策略
        delay,          ///< 延迟调度策略
        round_robin     ///< 轮询调度策略
    };

    /**
     * @class base_task_queue
     * @brief 任务队列基类 - 定义任务队列的通用接口
     * 
     * 适用场景：
     *   - 作为所有任务队列的基类
     *   - 定义统一的队列操作接口
     *   - 支持多态队列管理
     * 
     * 调用关系：
     *   - 被具体队列类继承
     *   - 被scheduler和worker调用
     *   - 管理base_task类型的任务
     */
    class base_task_queue
    {
    public:
        /**
         * @brief 虚析构函数
         */
        virtual ~base_task_queue() = default;

        /**
         * @brief 向队列中添加任务
         * @param task 要添加的任务
         * @return true 添加成功，false 添加失败
         */
        virtual bool push(std::shared_ptr<base_task> task) = 0;

        /**
         * @brief 从队列中取出任务（阻塞）
         * @return 任务智能指针，队列关闭时返回nullptr
         */
        virtual std::shared_ptr<base_task> pop() = 0;

        /**
         * @brief 尝试从队列中取出任务（非阻塞）
         * @return 任务智能指针，队列为空时返回nullptr
         */
        virtual std::shared_ptr<base_task> try_pop() = 0;

        /**
         * @brief 带超时的取出任务
         * @param timeout 超时时间
         * @return 任务智能指针，超时或队列关闭时返回nullptr
         */
        template<typename Rep, typename Period>
        virtual std::shared_ptr<base_task> try_pop_for(const std::chrono::duration<Rep, Period>& timeout) = 0;

        /**
         * @brief 获取队列大小
         * @return 队列中任务数量
         */
        virtual std::size_t size() const = 0;

        /**
         * @brief 检查队列是否为空
         * @return true 队列为空，false 队列非空
         */
        virtual bool empty() const = 0;

        /**
         * @brief 清空队列
         */
        virtual void clear() = 0;

        /**
         * @brief 关闭队列
         */
        virtual void close() = 0;

        /**
         * @brief 检查队列是否已关闭
         * @return true 队列已关闭，false 队列未关闭
         */
        virtual bool is_closed() const = 0;

        /**
         * @brief 获取队列策略
         * @return 队列调度策略
         */
        virtual queue_policy get_policy() const = 0;
    };

    /**
     * @class fifo_task_queue
     * @brief FIFO任务队列 - 先进先出的任务队列
     * 
     * 适用场景：
     *   - 普通任务的顺序执行
     *   - 简单的任务调度需求
     *   - 高吞吐量场景
     * 
     * 调用关系：
     *   - 继承自base_task_queue
     *   - 使用std::queue作为底层容器
     *   - 通过mutex和condition_variable实现线程安全
     */
    class fifo_task_queue : public base_task_queue
    {
    private:
        mutable std::mutex _queue_mutex;                       ///< 队列访问互斥锁
        std::condition_variable _not_empty_cv;                ///< 非空条件变量
        std::queue<std::shared_ptr<base_task>> _task_queue;    ///< 任务队列
        std::atomic<bool> _closed{false};                     ///< 队列关闭标志
        std::atomic<std::size_t> _max_size{0};               ///< 最大队列大小，0表示无限制

    public:
        /**
         * @brief 构造FIFO任务队列
         * @param max_size 最大队列大小，0表示无限制
         */
        explicit fifo_task_queue(std::size_t max_size = 0)
            : _max_size(max_size)
        {
        }

        /**
         * @brief 析构函数
         */
        ~fifo_task_queue() override
        {
            close();
        }

        /**
         * @brief 向队列中添加任务
         * @param task 要添加的任务
         * @return true 添加成功，false 添加失败（队列已关闭或已满）
         */
        bool push(std::shared_ptr<base_task> task) override
        {
            if (!task || _closed.load(std::memory_order_acquire))
            {
                return false;
            }

            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            // 检查队列大小限制
            auto max_size = _max_size.load(std::memory_order_relaxed);
            if (max_size > 0 && _task_queue.size() >= max_size)
            {
                return false;
            }

            _task_queue.push(task);
            _not_empty_cv.notify_one();
            return true;
        }

        /**
         * @brief 从队列中取出任务（阻塞）
         * @return 任务智能指针，队列关闭时返回nullptr
         */
        std::shared_ptr<base_task> pop() override
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            
            _not_empty_cv.wait(lock, [this] {
                return !_task_queue.empty() || _closed.load(std::memory_order_acquire);
            });

            if (_task_queue.empty())
            {
                return nullptr; // 队列已关闭
            }

            auto task = _task_queue.front();
            _task_queue.pop();
            return task;
        }

        /**
         * @brief 尝试从队列中取出任务（非阻塞）
         * @return 任务智能指针，队列为空时返回nullptr
         */
        std::shared_ptr<base_task> try_pop() override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            if (_task_queue.empty())
            {
                return nullptr;
            }

            auto task = _task_queue.front();
            _task_queue.pop();
            return task;
        }

        /**
         * @brief 带超时的取出任务
         * @param timeout 超时时间
         * @return 任务智能指针，超时或队列关闭时返回nullptr
         */
        template<typename Rep, typename Period>
        std::shared_ptr<base_task> try_pop_for(const std::chrono::duration<Rep, Period>& timeout) override
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            
            if (_not_empty_cv.wait_for(lock, timeout, [this] {
                return !_task_queue.empty() || _closed.load(std::memory_order_acquire);
            }))
            {
                if (!_task_queue.empty())
                {
                    auto task = _task_queue.front();
                    _task_queue.pop();
                    return task;
                }
            }
            
            return nullptr;
        }

        /**
         * @brief 获取队列大小
         * @return 队列中任务数量
         */
        std::size_t size() const override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _task_queue.size();
        }

        /**
         * @brief 检查队列是否为空
         * @return true 队列为空，false 队列非空
         */
        bool empty() const override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _task_queue.empty();
        }

        /**
         * @brief 清空队列
         */
        void clear() override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            std::queue<std::shared_ptr<base_task>> empty_queue;
            _task_queue.swap(empty_queue);
        }

        /**
         * @brief 关闭队列
         */
        void close() override
        {
            _closed.store(true, std::memory_order_release);
            _not_empty_cv.notify_all();
        }

        /**
         * @brief 检查队列是否已关闭
         * @return true 队列已关闭，false 队列未关闭
         */
        bool is_closed() const override
        {
            return _closed.load(std::memory_order_acquire);
        }

        /**
         * @brief 获取队列策略
         * @return 队列调度策略
         */
        queue_policy get_policy() const override
        {
            return queue_policy::fifo;
        }

        /**
         * @brief 设置最大队列大小
         * @param max_size 最大队列大小，0表示无限制
         */
        void set_max_size(std::size_t max_size)
        {
            _max_size.store(max_size, std::memory_order_relaxed);
        }

        /**
         * @brief 获取最大队列大小
         * @return 最大队列大小
         */
        std::size_t get_max_size() const
        {
            return _max_size.load(std::memory_order_relaxed);
        }
    };

    /**
     * @class priority_task_queue
     * @brief 优先级任务队列 - 基于优先级的任务调度
     * 
     * 适用场景：
     *   - 需要按优先级执行的任务
     *   - 关键任务优先处理
     *   - 多级任务调度
     * 
     * 调用关系：
     *   - 继承自base_task_queue
     *   - 使用std::priority_queue作为底层容器
     *   - 支持priority_task类型的任务
     */
    class priority_task_queue : public base_task_queue
    {
    private:
        /**
         * @brief 任务优先级比较器
         */
        struct task_priority_comparator
        {
            bool operator()(const std::shared_ptr<base_task>& lhs,
                          const std::shared_ptr<base_task>& rhs) const
            {
                // 优先级高的任务排在前面（数值越大优先级越高）
                auto lhs_priority = static_cast<int>(lhs->get_priority());
                auto rhs_priority = static_cast<int>(rhs->get_priority());
                
                if (lhs_priority != rhs_priority)
                {
                    return lhs_priority < rhs_priority; // 优先级队列是最大堆，所以用小于号
                }
                
                // 优先级相同时，按提交时间排序（先提交的先执行）
                return lhs->get_submit_time() > rhs->get_submit_time();
            }
        };

        mutable std::mutex _queue_mutex;                       ///< 队列访问互斥锁
        std::condition_variable _not_empty_cv;                ///< 非空条件变量
        std::priority_queue<std::shared_ptr<base_task>,
                           std::vector<std::shared_ptr<base_task>>,
                           task_priority_comparator> _task_queue; ///< 优先级任务队列
        std::atomic<bool> _closed{false};                     ///< 队列关闭标志
        std::atomic<std::size_t> _max_size{0};               ///< 最大队列大小

    public:
        /**
         * @brief 构造优先级任务队列
         * @param max_size 最大队列大小，0表示无限制
         */
        explicit priority_task_queue(std::size_t max_size = 0)
            : _max_size(max_size)
        {
        }

        /**
         * @brief 析构函数
         */
        ~priority_task_queue() override
        {
            close();
        }

        /**
         * @brief 向队列中添加任务
         * @param task 要添加的任务
         * @return true 添加成功，false 添加失败
         */
        bool push(std::shared_ptr<base_task> task) override
        {
            if (!task || _closed.load(std::memory_order_acquire))
            {
                return false;
            }

            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            auto max_size = _max_size.load(std::memory_order_relaxed);
            if (max_size > 0 && _task_queue.size() >= max_size)
            {
                return false;
            }

            _task_queue.push(task);
            _not_empty_cv.notify_one();
            return true;
        }

        /**
         * @brief 从队列中取出任务（阻塞）
         * @return 任务智能指针，队列关闭时返回nullptr
         */
        std::shared_ptr<base_task> pop() override
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            
            _not_empty_cv.wait(lock, [this] {
                return !_task_queue.empty() || _closed.load(std::memory_order_acquire);
            });

            if (_task_queue.empty())
            {
                return nullptr;
            }

            auto task = _task_queue.top();
            _task_queue.pop();
            return task;
        }

        /**
         * @brief 尝试从队列中取出任务（非阻塞）
         * @return 任务智能指针，队列为空时返回nullptr
         */
        std::shared_ptr<base_task> try_pop() override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            if (_task_queue.empty())
            {
                return nullptr;
            }

            auto task = _task_queue.top();
            _task_queue.pop();
            return task;
        }

        /**
         * @brief 带超时的取出任务
         * @param timeout 超时时间
         * @return 任务智能指针，超时或队列关闭时返回nullptr
         */
        template<typename Rep, typename Period>
        std::shared_ptr<base_task> try_pop_for(const std::chrono::duration<Rep, Period>& timeout) override
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            
            if (_not_empty_cv.wait_for(lock, timeout, [this] {
                return !_task_queue.empty() || _closed.load(std::memory_order_acquire);
            }))
            {
                if (!_task_queue.empty())
                {
                    auto task = _task_queue.top();
                    _task_queue.pop();
                    return task;
                }
            }
            
            return nullptr;
        }

        /**
         * @brief 获取队列大小
         * @return 队列中任务数量
         */
        std::size_t size() const override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _task_queue.size();
        }

        /**
         * @brief 检查队列是否为空
         * @return true 队列为空，false 队列非空
         */
        bool empty() const override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _task_queue.empty();
        }

        /**
         * @brief 清空队列
         */
        void clear() override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            std::priority_queue<std::shared_ptr<base_task>,
                               std::vector<std::shared_ptr<base_task>>,
                               task_priority_comparator> empty_queue;
            _task_queue.swap(empty_queue);
        }

        /**
         * @brief 关闭队列
         */
        void close() override
        {
            _closed.store(true, std::memory_order_release);
            _not_empty_cv.notify_all();
        }

        /**
         * @brief 检查队列是否已关闭
         * @return true 队列已关闭，false 队列未关闭
         */
        bool is_closed() const override
        {
            return _closed.load(std::memory_order_acquire);
        }

        /**
         * @brief 获取队列策略
         * @return 队列调度策略
         */
        queue_policy get_policy() const override
        {
            return queue_policy::priority;
        }

        /**
         * @brief 设置最大队列大小
         * @param max_size 最大队列大小，0表示无限制
         */
        void set_max_size(std::size_t max_size)
        {
            _max_size.store(max_size, std::memory_order_relaxed);
        }

        /**
         * @brief 获取最大队列大小
         * @return 最大队列大小
         */
        std::size_t get_max_size() const
        {
            return _max_size.load(std::memory_order_relaxed);
        }
    };

    /**
     * @class delay_task_queue
     * @brief 延迟任务队列 - 支持延迟执行的任务调度
     * 
     * 适用场景：
     *   - 定时任务执行
     *   - 延迟任务处理
     *   - 任务调度系统
     * 
     * 调用关系：
     *   - 继承自base_task_queue
     *   - 使用时间轮或最小堆管理延迟任务
     *   - 支持timeout_task类型的任务
     */
    class delay_task_queue : public base_task_queue
    {
    private:
        /**
         * @brief 延迟任务包装器
         */
        struct delayed_task
        {
            std::shared_ptr<base_task> task;                   ///< 原始任务
            std::chrono::steady_clock::time_point execute_time; ///< 执行时间
            
            delayed_task(std::shared_ptr<base_task> t, std::chrono::steady_clock::time_point et)
                : task(std::move(t)), execute_time(et) {}
        };

        /**
         * @brief 延迟任务比较器（最小堆，最早执行的任务在顶部）
         */
        struct delayed_task_comparator
        {
            bool operator()(const delayed_task& lhs, const delayed_task& rhs) const
            {
                return lhs.execute_time > rhs.execute_time;
            }
        };

        mutable std::mutex _queue_mutex;                       ///< 队列访问互斥锁
        std::condition_variable _not_empty_cv;                ///< 非空条件变量
        std::priority_queue<delayed_task,
                           std::vector<delayed_task>,
                           delayed_task_comparator> _delayed_queue; ///< 延迟任务队列
        std::queue<std::shared_ptr<base_task>> _ready_queue;   ///< 就绪任务队列
        std::atomic<bool> _closed{false};                     ///< 队列关闭标志
        std::atomic<std::size_t> _max_size{0};               ///< 最大队列大小
        std::thread _timer_thread;                            ///< 定时器线程
        std::atomic<bool> _timer_running{false};             ///< 定时器运行标志

    public:
        /**
         * @brief 构造延迟任务队列
         * @param max_size 最大队列大小，0表示无限制
         */
        explicit delay_task_queue(std::size_t max_size = 0)
            : _max_size(max_size)
        {
            start_timer_thread();
        }

        /**
         * @brief 析构函数
         */
        ~delay_task_queue() override
        {
            close();
            stop_timer_thread();
        }

        /**
         * @brief 向队列中添加任务
         * @param task 要添加的任务
         * @return true 添加成功，false 添加失败
         */
        bool push(std::shared_ptr<base_task> task) override
        {
            if (!task || _closed.load(std::memory_order_acquire))
            {
                return false;
            }

            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            auto max_size = _max_size.load(std::memory_order_relaxed);
            if (max_size > 0 && (_delayed_queue.size() + _ready_queue.size()) >= max_size)
            {
                return false;
            }

            // 检查任务是否有延迟设置
            auto timeout_task_ptr = std::dynamic_pointer_cast<timeout_task<void>>(task);
            if (timeout_task_ptr && timeout_task_ptr->get_deadline() > std::chrono::steady_clock::now())
            {
                _delayed_queue.emplace(task, timeout_task_ptr->get_deadline());
            }
            else
            {
                _ready_queue.push(task);
                _not_empty_cv.notify_one();
            }
            
            return true;
        }

        /**
         * @brief 添加延迟任务
         * @param task 要添加的任务
         * @param delay 延迟时间
         * @return true 添加成功，false 添加失败
         */
        template<typename Rep, typename Period>
        bool push_delayed(std::shared_ptr<base_task> task,
                         const std::chrono::duration<Rep, Period>& delay)
        {
            if (!task || _closed.load(std::memory_order_acquire))
            {
                return false;
            }

            auto execute_time = std::chrono::steady_clock::now() + delay;
            
            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            auto max_size = _max_size.load(std::memory_order_relaxed);
            if (max_size > 0 && (_delayed_queue.size() + _ready_queue.size()) >= max_size)
            {
                return false;
            }

            _delayed_queue.emplace(task, execute_time);
            return true;
        }

        /**
         * @brief 从队列中取出任务（阻塞）
         * @return 任务智能指针，队列关闭时返回nullptr
         */
        std::shared_ptr<base_task> pop() override
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            
            _not_empty_cv.wait(lock, [this] {
                return !_ready_queue.empty() || _closed.load(std::memory_order_acquire);
            });

            if (_ready_queue.empty())
            {
                return nullptr;
            }

            auto task = _ready_queue.front();
            _ready_queue.pop();
            return task;
        }

        /**
         * @brief 尝试从队列中取出任务（非阻塞）
         * @return 任务智能指针，队列为空时返回nullptr
         */
        std::shared_ptr<base_task> try_pop() override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            
            if (_ready_queue.empty())
            {
                return nullptr;
            }

            auto task = _ready_queue.front();
            _ready_queue.pop();
            return task;
        }

        /**
         * @brief 带超时的取出任务
         * @param timeout 超时时间
         * @return 任务智能指针，超时或队列关闭时返回nullptr
         */
        template<typename Rep, typename Period>
        std::shared_ptr<base_task> try_pop_for(const std::chrono::duration<Rep, Period>& timeout) override
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            
            if (_not_empty_cv.wait_for(lock, timeout, [this] {
                return !_ready_queue.empty() || _closed.load(std::memory_order_acquire);
            }))
            {
                if (!_ready_queue.empty())
                {
                    auto task = _ready_queue.front();
                    _ready_queue.pop();
                    return task;
                }
            }
            
            return nullptr;
        }

        /**
         * @brief 获取队列大小
         * @return 队列中任务数量
         */
        std::size_t size() const override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _delayed_queue.size() + _ready_queue.size();
        }

        /**
         * @brief 检查队列是否为空
         * @return true 队列为空，false 队列非空
         */
        bool empty() const override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _delayed_queue.empty() && _ready_queue.empty();
        }

        /**
         * @brief 清空队列
         */
        void clear() override
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            std::priority_queue<delayed_task,
                               std::vector<delayed_task>,
                               delayed_task_comparator> empty_delayed_queue;
            std::queue<std::shared_ptr<base_task>> empty_ready_queue;
            _delayed_queue.swap(empty_delayed_queue);
            _ready_queue.swap(empty_ready_queue);
        }

        /**
         * @brief 关闭队列
         */
        void close() override
        {
            _closed.store(true, std::memory_order_release);
            _not_empty_cv.notify_all();
        }

        /**
         * @brief 检查队列是否已关闭
         * @return true 队列已关闭，false 队列未关闭
         */
        bool is_closed() const override
        {
            return _closed.load(std::memory_order_acquire);
        }

        /**
         * @brief 获取队列策略
         * @return 队列调度策略
         */
        queue_policy get_policy() const override
        {
            return queue_policy::delay;
        }

        /**
         * @brief 设置最大队列大小
         * @param max_size 最大队列大小，0表示无限制
         */
        void set_max_size(std::size_t max_size)
        {
            _max_size.store(max_size, std::memory_order_relaxed);
        }

        /**
         * @brief 获取最大队列大小
         * @return 最大队列大小
         */
        std::size_t get_max_size() const
        {
            return _max_size.load(std::memory_order_relaxed);
        }

        /**
         * @brief 获取延迟任务数量
         * @return 延迟任务数量
         */
        std::size_t get_delayed_task_count() const
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _delayed_queue.size();
        }

        /**
         * @brief 获取就绪任务数量
         * @return 就绪任务数量
         */
        std::size_t get_ready_task_count() const
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            return _ready_queue.size();
        }

    private:
        /**
         * @brief 启动定时器线程
         */
        void start_timer_thread()
        {
            _timer_running.store(true, std::memory_order_release);
            _timer_thread = std::thread([this] {
                timer_thread_function();
            });
        }

        /**
         * @brief 停止定时器线程
         */
        void stop_timer_thread()
        {
            _timer_running.store(false, std::memory_order_release);
            if (_timer_thread.joinable())
            {
                _timer_thread.join();
            }
        }

        /**
         * @brief 定时器线程函数
         */
        void timer_thread_function()
        {
            while (_timer_running.load(std::memory_order_acquire))
            {
                auto now = std::chrono::steady_clock::now();
                std::vector<std::shared_ptr<base_task>> ready_tasks;

                {
                    std::lock_guard<std::mutex> lock(_queue_mutex);
                    
                    // 将到期的延迟任务移动到就绪队列
                    while (!_delayed_queue.empty() && _delayed_queue.top().execute_time <= now)
                    {
                        auto delayed_task = _delayed_queue.top();
                        _delayed_queue.pop();
                        _ready_queue.push(delayed_task.task);
                        ready_tasks.push_back(delayed_task.task);
                    }
                }

                // 如果有任务就绪，通知等待的线程
                if (!ready_tasks.empty())
                {
                    _not_empty_cv.notify_all();
                }

                // 休眠一小段时间，避免过度占用CPU
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };

    /**
     * @class multi_level_task_queue
     * @brief 多级任务队列 - 支持多种调度策略的混合队列
     * 
     * 适用场景：
     *   - 复杂的任务调度需求
     *   - 多种优先级策略混合
     *   - 高性能任务分发
     * 
     * 调用关系：
     *   - 继承自base_task_queue
     *   - 内部管理多个子队列
     *   - 支持轮询和优先级调度
     */
    class multi_level_task_queue : public base_task_queue
    {
    private:
        std::vector<std::unique_ptr<base_task_queue>> _sub_queues; ///< 子队列列表
        std::atomic<std::size_t> _current_queue_index{0};         ///< 当前轮询队列索引
        mutable std::shared_mutex _queues_mutex;                  ///< 队列列表读写锁
        std::atomic<bool> _closed{false};                         ///< 队列关闭标志
        queue_policy _policy;                                     ///< 调度策略

    public:
        /**
         * @brief 构造多级任务队列
         * @param policy 调度策略
         */
        explicit multi_level_task_queue(queue_policy policy = queue_policy::round_robin)
            : _policy(policy)
        {
        }

        /**
         * @brief 析构函数
         */
        ~multi_level_task_queue() override
        {
            close();
        }

        /**
         * @brief 添加子队列
         * @param queue 子队列
         */
        void add_sub_queue(std::unique_ptr<base_task_queue> queue)
        {
            if (!queue)
            {
                return;
            }

            std::unique_lock<std::shared_mutex> lock(_queues_mutex);
            _sub_queues.push_back(std::move(queue));
        }

        /**
         * @brief 向队列中添加任务
         * @param task 要添加的任务
         * @return true 添加成功，false 添加失败
         */
        bool push(std::shared_ptr<base_task> task) override
        {
            if (!task || _closed.load(std::memory_order_acquire))
            {
                return false;
            }

            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            
            if (_sub_queues.empty())
            {
                return false;
            }

            // 根据调度策略选择队列
            std::size_t queue_index = select_queue_for_push(task);
            if (queue_index < _sub_queues.size())
            {
                return _sub_queues[queue_index]->push(task);
            }

            return false;
        }

        /**
         * @brief 从队列中取出任务（阻塞）
         * @return 任务智能指针，队列关闭时返回nullptr
         */
        std::shared_ptr<base_task> pop() override
        {
            while (!_closed.load(std::memory_order_acquire))
            {
                std::shared_lock<std::shared_mutex> lock(_queues_mutex);
                
                if (_sub_queues.empty())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                // 根据调度策略选择队列
                auto queue_index = select_queue_for_pop();
                if (queue_index < _sub_queues.size())
                {
                    auto task = _sub_queues[queue_index]->try_pop();
                    if (task)
                    {
                        return task;
                    }
                }

                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return nullptr;
        }

        /**
         * @brief 尝试从队列中取出任务（非阻塞）
         * @return 任务智能指针，队列为空时返回nullptr
         */
        std::shared_ptr<base_task> try_pop() override
        {
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            
            if (_sub_queues.empty())
            {
                return nullptr;
            }

            // 尝试从所有队列中获取任务
            for (std::size_t i = 0; i < _sub_queues.size(); ++i)
            {
                auto queue_index = select_queue_for_pop();
                if (queue_index < _sub_queues.size())
                {
                    auto task = _sub_queues[queue_index]->try_pop();
                    if (task)
                    {
                        return task;
                    }
                }
            }

            return nullptr;
        }

        /**
         * @brief 带超时的取出任务
         * @param timeout 超时时间
         * @return 任务智能指针，超时或队列关闭时返回nullptr
         */
        template<typename Rep, typename Period>
        std::shared_ptr<base_task> try_pop_for(const std::chrono::duration<Rep, Period>& timeout) override
        {
            auto deadline = std::chrono::steady_clock::now() + timeout;
            
            while (std::chrono::steady_clock::now() < deadline && !_closed.load(std::memory_order_acquire))
            {
                auto task = try_pop();
                if (task)
                {
                    return task;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return nullptr;
        }

        /**
         * @brief 获取队列大小
         * @return 队列中任务数量
         */
        std::size_t size() const override
        {
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            
            std::size_t total_size = 0;
            for (const auto& queue : _sub_queues)
            {
                total_size += queue->size();
            }
            
            return total_size;
        }

        /**
         * @brief 检查队列是否为空
         * @return true 队列为空，false 队列非空
         */
        bool empty() const override
        {
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            
            for (const auto& queue : _sub_queues)
            {
                if (!queue->empty())
                {
                    return false;
                }
            }
            
            return true;
        }

        /**
         * @brief 清空队列
         */
        void clear() override
        {
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            
            for (auto& queue : _sub_queues)
            {
                queue->clear();
            }
        }

        /**
         * @brief 关闭队列
         */
        void close() override
        {
            _closed.store(true, std::memory_order_release);
            
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            for (auto& queue : _sub_queues)
            {
                queue->close();
            }
        }

        /**
         * @brief 检查队列是否已关闭
         * @return true 队列已关闭，false 队列未关闭
         */
        bool is_closed() const override
        {
            return _closed.load(std::memory_order_acquire);
        }

        /**
         * @brief 获取队列策略
         * @return 队列调度策略
         */
        queue_policy get_policy() const override
        {
            return _policy;
        }

        /**
         * @brief 获取子队列数量
         * @return 子队列数量
         */
        std::size_t get_sub_queue_count() const
        {
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            return _sub_queues.size();
        }

        /**
         * @brief 获取指定索引的子队列大小
         * @param index 子队列索引
         * @return 子队列大小
         */
        std::size_t get_sub_queue_size(std::size_t index) const
        {
            std::shared_lock<std::shared_mutex> lock(_queues_mutex);
            if (index < _sub_queues.size())
            {
                return _sub_queues[index]->size();
            }
            return 0;
        }

    private:
        /**
         * @brief 为推送任务选择队列
         * @param task 要推送的任务
         * @return 队列索引
         */
        std::size_t select_queue_for_push(std::shared_ptr<base_task> task)
        {
            switch (_policy)
            {
                case queue_policy::priority:
                {
                    // 根据任务优先级选择队列
                    auto priority = static_cast<int>(task->get_priority());
                    return std::min(static_cast<std::size_t>(priority), _sub_queues.size() - 1);
                }
                case queue_policy::round_robin:
                default:
                {
                    // 轮询选择队列
                    auto index = _current_queue_index.fetch_add(1, std::memory_order_relaxed);
                    return index % _sub_queues.size();
                }
            }
        }

        /**
         * @brief 为弹出任务选择队列
         * @return 队列索引
         */
        std::size_t select_queue_for_pop()
        {
            switch (_policy)
            {
                case queue_policy::priority:
                {
                    // 优先级策略：从高优先级队列开始查找
                    for (std::size_t i = _sub_queues.size(); i > 0; --i)
                    {
                        std::size_t index = i - 1;
                        if (!_sub_queues[index]->empty())
                        {
                            return index;
                        }
                    }
                    return 0;
                }
                case queue_policy::round_robin:
                default:
                {
                    // 轮询策略
                    auto index = _current_queue_index.fetch_add(1, std::memory_order_relaxed);
                    return index % _sub_queues.size();
                }
            }
        }
    };

    /**
     * @brief 任务队列工厂函数 - 创建FIFO队列
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<base_task_queue> make_fifo_queue(std::size_t max_size = 0)
    {
        return std::make_unique<fifo_task_queue>(max_size);
    }

    /**
     * @brief 任务队列工厂函数 - 创建优先级队列
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<base_task_queue> make_priority_queue(std::size_t max_size = 0)
    {
        return std::make_unique<priority_task_queue>(max_size);
    }

    /**
     * @brief 任务队列工厂函数 - 创建延迟队列
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<base_task_queue> make_delay_queue(std::size_t max_size = 0)
    {
        return std::make_unique<delay_task_queue>(max_size);
    }

    /**
     * @brief 任务队列工厂函数 - 创建多级队列
     * @param policy 调度策略
     * @return 队列智能指针
     */
    inline std::unique_ptr<base_task_queue> make_multi_level_queue(queue_policy policy = queue_policy::round_robin)
    {
        return std::make_unique<multi_level_task_queue>(policy);
    }

    /**
     * @brief 任务队列工厂函数 - 根据策略创建队列
     * @param policy 队列策略
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<base_task_queue> make_task_queue(queue_policy policy, std::size_t max_size = 0)
    {
        switch (policy)
        {
            case queue_policy::fifo:
                return make_fifo_queue(max_size);
            case queue_policy::priority:
                return make_priority_queue(max_size);
            case queue_policy::delay:
                return make_delay_queue(max_size);
            case queue_policy::round_robin:
                return make_multi_level_queue(policy);
            default:
                return make_fifo_queue(max_size);
        }
    }

} // namespace thread_pool