#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include "Task.hpp"
#include <typeinfo>
#include <stdexcept>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
namespace internals
{
  namespace structure_c
  {
    using _interior_task_ptr = std::shared_ptr<internals::structure_u::uint_ordinary>;
    /**
     * @brief 任务队列类型的安全转换
     * @tparam originally_type 要转换的类型
     * @tparam function 转换函数
     * @tparam downgrade_function 降级调用函数 
     * @param pointer 队列 指针
     * @param conversion_call 转换函数值
     * @param downgrade  降级调用函数值
     * @return `true` 转换类型成功，`false` 转换类型失败
     * @warning 转换函数和失败调用函数的参数需要用智能指针来维护内存安全
     */
    template<class originally_type,class function,class downgrade_function>
    bool automatic_derivation(_interior_task_ptr pointer,function&& conversion_call, downgrade_function&& downgrade)
    {
      if(pointer.get() != nullptr)
      {
        if(auto concrete_queue = std::dynamic_pointer_cast<originally_type>(pointer))
        {
          std::invoke(conversion_call, concrete_queue);
          return true;
        }
        else
        {
          std::invoke(downgrade, pointer);
        }
      }
      return false;
    }
    /**
     * @enum rank_strategy
     * @brief 队列调度策略枚举
     *
     * 定义了不同的任务队列调度策略：`fifo`: 先进先出策略,`priority`: 优先级调度策略,`delay`: 延迟调度策略
     * `round_robin`: 轮询调度策略
     */
    enum class rank_strategy
    {
      fifo,       // 先进先出
      priority,   // 优先级
      delay,      // 延迟
      round_robin // 轮询
    };
    /**
     * @class base_task_queue
     * @brief 任务队列基类 - 定义任务队列的通用接口
     *
     * 适用场景：作为所有任务队列的基类, 定义统一的队列操作接口,支持多态队列管理
     *
     * 调用关系：被具体队列类继承, 被`scheduler`和`worker`调用, 
     * 管理`uint_ordinary`类型的任务
     */
    class cohort_base
    {
    public:
      /**
       * @brief 虚析构函数
       */
      virtual ~cohort_base() = default;
      /**
       * @brief 向队列中添加任务
       * @param task 要添加的任务
       * @return `true` 添加成功，`false` 添加失败
       */
      virtual bool push(_interior_task_ptr pointer) = 0;
      /**
       * @brief 从队列中取出任务（阻塞）
       * @return 任务智能指针，队列关闭时返回`nullptr`
       */
      virtual _interior_task_ptr pop() = 0;
      /**
       * @brief 尝试从队列中取出任务（非阻塞）
       * @return 任务智能指针，队列为空时返回`nullptr`
       */
      virtual _interior_task_ptr try_pop() = 0;
      /**
       * @brief 带超时的取出任务
       * @param timeout 超时时间
       * @return 任务智能指针，超时或队列关闭时返回`nullptr`
       */
      virtual _interior_task_ptr try_pop_for(const std::chrono::milliseconds& timeout) = 0;
      /**
       * @brief 获取队列大小
       * @return 队列中任务数量
       */
      virtual size_t size()const = 0;
      /**
       * @brief 检查队列是否为空
       * @return `true` 队列为空，`false` 队列非空
       */
      virtual bool empty()const = 0;
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
       * @return `true` 队列已关闭，`false` 队列未关闭
       */
      virtual bool closed()const = 0;
      /**
       * @brief 获取队列策略
       * @return 队列调度策略
       */
      virtual rank_strategy get_strategy() const = 0;
    };
    /**
     * @class cohort_order
     * @brief `FIFO`任务队列 - 先进先出的任务队列
     *
     * 适用场景：普通任务的顺序执行, 简单的任务调度需求, 高吞吐量场景
     *
     * 调用关系：
     *   继承自`cohort_base`, 使用`std::queue`作为底层容器,
     *   通过`mutex`和`condition_variable`实现线程安全
     */
    class cohort_order : public cohort_base
    {
    private:
      mutable std::mutex _cohort_mutex;                   // 队列访问互斥锁
      std::condition_variable _judge_empty_cv;            // 非空条件变量

      std::atomic<bool> _closed{false};                   // 队列关闭标志
      std::queue<_interior_task_ptr> _task_cohort;        // 任务队列

      std::atomic<std::size_t> _max_size{0};              // 最大队列大小，0表示无限制
    public:
      /**
       * @brief 构造FIFO任务队列
       * @param max_size 最大队列大小，0表示无限制
       */
      explicit cohort_order(std::size_t max_size = 0)
        : _max_size(max_size) {}
      /**
       * @brief 析构函数
       */
      ~cohort_order() override
      {
        close();
      }
      /**
       * @brief 向队列中添加任务
       * @param task 要添加的任务
       * @return true 添加成功，false 添加失败（队列已关闭或已满）
       */
      bool push(_interior_task_ptr task) override
      {
        if (task.get() == nullptr || _closed.load(std::memory_order_acquire))
        {
          return false;
        }

        std::lock_guard<std::mutex> lock(_cohort_mutex);

        // 检查队列大小限制
        std::size_t max_size = _max_size.load(std::memory_order_relaxed);
        if (max_size > 0 && _task_cohort.size() >= max_size)
        {
          return false;
        }

        _task_cohort.push(task);
        _judge_empty_cv.notify_one();
        return true;
      }
      /**
       * @brief 从队列中取出任务（阻塞）
       * @return 任务智能指针，队列关闭时返回`nullptr`
       */
      _interior_task_ptr pop() override
      {
        std::unique_lock<std::mutex> lock(_cohort_mutex);
        auto wait_logic = [this]()
        {
          return !_task_cohort.empty() || _closed.load(std::memory_order_acquire);
        };
        _judge_empty_cv.wait(lock, wait_logic);

        if (_task_cohort.empty())
        {
          return nullptr;
        }

        auto task = _task_cohort.front();
        _task_cohort.pop();
        return task;
      }
      /**
       * @brief 尝试从队列中取出任务（非阻塞）
       * @return 任务智能指针，队列为空时返回`nullptr`
       */
      _interior_task_ptr try_pop() override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);

        if (_task_cohort.empty())
        {
          return nullptr;
        }

        auto task = _task_cohort.front();
        _task_cohort.pop();
        return task;
      }
      /**
       * @brief 带超时的取出任务
       * @param timeout 超时时间
       * @return 任务智能指针，超时或队列关闭时返回`nullptr`
       */
      _interior_task_ptr try_pop_for(const std::chrono::milliseconds &timeout) override
      {
        std::unique_lock<std::mutex> lock(_cohort_mutex);
        auto wait_logic = [this]()
        {
          return !_task_cohort.empty() || _closed.load(std::memory_order_acquire);
        };
        if (_judge_empty_cv.wait_for(lock, timeout, wait_logic))
        {
          if (!_task_cohort.empty())
          {
            auto task = _task_cohort.front();
            _task_cohort.pop();
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
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _task_cohort.size();
      }
      /**
       * @brief 检查队列是否为空
       * @return true 队列为空，false 队列非空
       */
      bool empty() const override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _task_cohort.empty();
      }
      /**
       * @brief 清空队列
       */
      void clear() override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        std::queue<_interior_task_ptr> empty_queue;
        _task_cohort.swap(empty_queue);
      }
      /**
       * @brief 关闭队列
       */
      void close() override
      {
        _closed.store(true, std::memory_order_release);
        _judge_empty_cv.notify_all();
      }
      /**
       * @brief 检查队列是否已关闭
       * @return `true` 队列已关闭，`false` 队列未关闭
       */
      bool closed() const override
      {
        return _closed.load(std::memory_order_acquire);
      }
      /**
       * @brief 获取队列策略
       * @return 队列调度策略
       */
      rank_strategy get_strategy() const override
      {
        return rank_strategy::fifo;
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
     * @class cohort_prior
     * @brief 优先级任务队列 - 基于优先级的任务调度
     *
     * 适用场景: 需要按优先级执行的任务, 关键任务优先处理, 多级任务调度
     *
     * 调用关系：继承自`cohort_base`, 使用`std::priority_queue`作为底层容器,
     * 支持`task_prio`类型的任务
     */
    class cohort_prior : public cohort_base
    {
    private: 
      /**
       * @brief 优先级比较器
       */
      struct comparator
      {
        bool operator()(const _interior_task_ptr &lhs, const _interior_task_ptr &rhs) const
        {
          auto lhs_priority = static_cast<int>(lhs->get_priority());
          auto rhs_priority = static_cast<int>(rhs->get_priority());

          if(lhs_priority != rhs_priority)
          {
            return lhs_priority < rhs_priority;
          }
          return lhs->get_submit_time() > rhs->get_submit_time();
        }
      };
      mutable std::mutex _cohort_mutex;
      std::condition_variable _judge_empty_cv;

      std::atomic<bool> _closed{false};
      std::atomic<std::size_t> _max_size{0};

      std::priority_queue<_interior_task_ptr, std::vector<_interior_task_ptr>, comparator> _task_cohort;
    public:
      /**
       * @brief 构造优先级任务队列
       * @param max_size 最大队列大小，0表示无限制
       */
      explicit cohort_prior(std::size_t max_size = 0)
        : _max_size(max_size){}

      /**
       * @brief 析构函数
       */
      ~cohort_prior() override
      {
        close();
      }
      /**
       * @brief 向队列中添加任务
       * @param task 要添加的任务
       * @return `true` 添加成功，`false` 添加失败
       */
      bool push(_interior_task_ptr task) override
      {
        if (task.get() == nullptr || _closed.load(std::memory_order_acquire))
        {
          return false;
        }

        std::lock_guard<std::mutex> lock(_cohort_mutex);

        auto max_size = _max_size.load(std::memory_order_relaxed);
        if (max_size > 0 && _task_cohort.size() >= max_size)
        {
          return false;
        }

        _task_cohort.push(task);
        _judge_empty_cv.notify_one();
        return true;
      }
      /**
       * @brief 从队列中取出任务（阻塞）
       * @return 任务智能指针，队列关闭时返回`nullptr`
       */
      _interior_task_ptr pop() override
      {
        std::unique_lock<std::mutex> lock(_cohort_mutex);
        auto wait_logic = [this]()
        {
          return !_task_cohort.empty() || _closed.load(std::memory_order_acquire);
        };
        _judge_empty_cv.wait(lock, wait_logic);

        if (_task_cohort.empty())
        {
          return nullptr;
        }

        auto task = _task_cohort.top();
        _task_cohort.pop();
        return task;
      }

      /**
       * @brief 尝试从队列中取出任务（非阻塞）
       * @return 任务智能指针，队列为空时返回`nullptr`
       */
      _interior_task_ptr try_pop() override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);

        if (_task_cohort.empty())
        {
          return nullptr;
        }

        auto task = _task_cohort.top();
        _task_cohort.pop();
        return task;
      }

      /**
       * @brief 带超时的取出任务
       * @param timeout 超时时间
       * @return 任务智能指针，超时或队列关闭时返回`nullptr`
       */
      _interior_task_ptr try_pop_for(const std::chrono::milliseconds &timeout) override
      {
        std::unique_lock<std::mutex> lock(_cohort_mutex);
        auto wait_logic = [this]()
        {
          return !_task_cohort.empty() || _closed.load(std::memory_order_acquire);
        };
        if (_judge_empty_cv.wait_for(lock, timeout, wait_logic))
        {
          if (!_task_cohort.empty())
          {
            auto task = _task_cohort.top();
            _task_cohort.pop();
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
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _task_cohort.size();
      }

      /**
       * @brief 检查队列是否为空
       * @return `true` 队列为空，`false` 队列非空
       */
      bool empty() const override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _task_cohort.empty();
      }

      /**
       * @brief 清空队列
       */
      void clear() override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        std::priority_queue<_interior_task_ptr, std::vector<_interior_task_ptr>, comparator> empty_queue;
        _task_cohort.swap(empty_queue);
      }

      /**
       * @brief 关闭队列
       */
      void close() override
      {
        _closed.store(true, std::memory_order_release);
        _judge_empty_cv.notify_all();
      }

      /**
       * @brief 检查队列是否已关闭
       * @return `true` 队列已关闭，`false` 队列未关闭
       */
      bool closed() const override
      {
        return _closed.load(std::memory_order_acquire);
      }

      /**
       * @brief 获取队列策略
       * @return 队列调度策略
       */
      rank_strategy get_strategy() const override
      {
        return rank_strategy::priority;
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
     * @class cohort_delay
     * @brief 延迟任务队列 - 支持延迟执行的任务调度
     *
     * 适用场景：定时任务执行, 延迟任务处理, 任务调度系统
     *
     * 调用关系：继承自`cohort_base`,使用时间轮或最小堆管理延迟任务,支持`task_time`类型的任务
     */
    class cohort_delay : public cohort_base
    {
    private:
      struct delayed_task
      {
        _interior_task_ptr task;
        std::chrono::steady_clock::time_point time;
        delayed_task(_interior_task_ptr task, std::chrono::steady_clock::time_point time)
          : task(task), time(time) {}
      };
      struct comparator
      {
        bool operator()(const delayed_task &lhs, const delayed_task &rhs) const
        {
          return lhs.time > rhs.time;
        }
      };
      mutable std::mutex _cohort_mutex;
      std::condition_variable _judge_empty_cv;

      std::queue<_interior_task_ptr> _task_cohort;
      std::priority_queue<delayed_task, std::vector<delayed_task>, comparator> _time_cohort;

      std::atomic<bool> _closed{false};                    
      std::atomic<std::size_t> _max_size{0};   
      std::atomic<bool> _time_running{false}; 

      std::thread _time_thread;                                    
    public:
      /**
       * @brief 构造延迟任务队列
       * @param max_size 最大队列大小，0表示无限制
       */
      explicit cohort_delay(std::size_t max_size = 0)
        : _max_size(max_size)
      {
        start_timer_thread();
      }
      /**
       * @brief 析构函数
       */
      ~cohort_delay() override
      {
        close();
        stop_timer_thread();
      }
      /**
       * @brief 向队列中添加任务
       * @param task 要添加的任务
       * @return `true` 添加成功，`false` 添加失败
       */
      bool push(_interior_task_ptr task) override
      {
        if (task.get() == nullptr || _closed.load(std::memory_order_acquire))
        {
          return false;
        }

        std::lock_guard<std::mutex> lock(_cohort_mutex);

        std::size_t max_size = _max_size.load(std::memory_order_relaxed);
        if (max_size > 0 && (_time_cohort.size() + _task_cohort.size()) >= max_size)
        {
          return false;
        }
        if (task->is_timeout())
        {
          _time_cohort.emplace(task, task->get_deadline());
        }
        else
        {
          _task_cohort.push(task);
          _judge_empty_cv.notify_one();
        }
        return true;
      }
      /**
       * @brief 从队列中取出任务（阻塞）
       * @return 任务智能指针，队列关闭时返回`nullptr`
       */
      _interior_task_ptr pop() override
      {
        std::unique_lock<std::mutex> lock(_cohort_mutex);
        auto wait_logic = [this]()
        {
          return !_task_cohort.empty() || _closed.load(std::memory_order_acquire);
        };
        _judge_empty_cv.wait(lock, wait_logic);

        if (_task_cohort.empty())
        {
          return nullptr;
        }

        auto task = _task_cohort.front();
        _task_cohort.pop();
        return task;
      }
      /**
       * @brief 尝试从队列中取出任务（非阻塞）
       * @return 任务智能指针，队列为空时返回`nullptr`
       */
      _interior_task_ptr try_pop() override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);

        if (_task_cohort.empty())
        {
          return nullptr;
        }

        auto task = _task_cohort.front();
        _task_cohort.pop();
        return task;
      }
      /**
       * @brief 带超时的取出任务
       * @param timeout 超时时间
       * @return 任务智能指针，超时或队列关闭时返回`nullptr`
       */
      _interior_task_ptr try_pop_for(const std::chrono::milliseconds &timeout) override
      {
        std::unique_lock<std::mutex> lock(_cohort_mutex);
        auto wait_logic = [this]()
        {
          return !_task_cohort.empty() || _closed.load(std::memory_order_acquire);
        };
        if (_judge_empty_cv.wait_for(lock, timeout, wait_logic))
        {
          if (!_task_cohort.empty())
          {
            auto task = _task_cohort.front();
            _task_cohort.pop();
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
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _time_cohort.size() + _task_cohort.size();
      }

      /**
       * @brief 检查队列是否为空
       * @return `true` 队列为空，`false` 队列非空
       */
      bool empty() const override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _time_cohort.empty() && _task_cohort.empty();
      }

      /**
       * @brief 清空队列
       */
      void clear() override
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        std::priority_queue<delayed_task, std::vector<delayed_task>, comparator> empty_delayed;
        std::queue<_interior_task_ptr> empty_ready;
        _time_cohort.swap(empty_delayed);
        _task_cohort.swap(empty_ready);
      }

      /**
       * @brief 关闭队列
       */
      void close() override
      {
        _closed.store(true, std::memory_order_release);
        _judge_empty_cv.notify_all();
      }

      /**
       * @brief 检查队列是否已关闭
       * @return true 队列已关闭，false 队列未关闭
       */
      bool closed() const override
      {
        return _closed.load(std::memory_order_acquire);
      }

      /**
       * @brief 获取队列策略
       * @return 队列调度策略
       */
      rank_strategy get_strategy() const override
      {
        return rank_strategy::delay;
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
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _time_cohort.size();
      }

      /**
       * @brief 获取就绪任务数量
       * @return 就绪任务数量
       */
      std::size_t get_ready_task_count() const
      {
        std::lock_guard<std::mutex> lock(_cohort_mutex);
        return _task_cohort.size();
      }
    private:
      /**
       * @brief 启动定时器线程
       */
      void start_timer_thread()
      {
        _time_running.store(true, std::memory_order_release);
        _time_thread = std::thread([this]{ timer_thread_function(); });
      }
      /**
       * @brief 停止定时器线程
       */
      void stop_timer_thread()
      {
        _time_running.store(false, std::memory_order_release);
        if (_time_thread.joinable())
        {
          _time_thread.join();
        }
      }
      void timer_thread_function()
      {
        while (_time_running.load(std::memory_order_acquire))
        {
          auto now = std::chrono::steady_clock::now();

          {
            std::lock_guard<std::mutex> lock(_cohort_mutex);

            // 将到期的延迟任务移动到就绪队列
            while (!_time_cohort.empty() && _time_cohort.top().time <= now)
            {
              auto delayed_task = _time_cohort.top();
              _time_cohort.pop();
              _task_cohort.push(delayed_task.task);
            }
          }

          // 如果有任务就绪，通知等待的线程
          if (!_task_cohort.empty())
          {
            _judge_empty_cv.notify_all();
          }

          // 休眠一小段时间，避免过度占用CPU
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    };
    /**
     * @class cohort_multi
     * @brief 多级任务队列 - 支持多种调度策略的混合队列
     *
     * 适用场景：复杂的任务调度需求, 多种优先级策略混合, 高性能任务分发
     *
     * 调用关系：继承自`cohort_base`, 内部管理多个子队列, 支持轮询和优先级调度
     */
    class cohort_multi : public cohort_base
    {
    private:
      std::vector<std::unique_ptr<cohort_base>> _cohort_list;

      std::atomic<bool> _closed{false};
      std::atomic<std::size_t> _current_index{0};
      
      mutable std::shared_mutex _multi_cohort_mutex;

      rank_strategy _strategy;
    public:
      /**
       * @brief 构造多级任务队列
       * @param policy 调度策略
       */
      explicit cohort_multi(rank_strategy policy = rank_strategy::round_robin)
        : _strategy(policy) {}
      /**
       * @brief 析构函数
       */
      ~cohort_multi() override
      {
        close();
      }
      /**
       * @brief 添加子队列
       * @param cohort 子队列
       */
      void add_sub_queue(std::unique_ptr<cohort_base> cohort)
      {
        if (!cohort)
        {
          return;
        }
        std::unique_lock<std::shared_mutex> lock(_multi_cohort_mutex);
        _cohort_list.push_back(std::move(cohort));
      }
      /**
       * @brief 向队列中添加任务
       * @param task 要添加的任务
       * @return `true` 添加成功，`false` 添加失败
       */
      bool push(_interior_task_ptr task) override
      {
        if (!task || _closed.load(std::memory_order_acquire))
        {
          return false;
        }

        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);

        if (_cohort_list.empty())
        {
          return false;
        }

        // 根据调度策略选择队列
        std::size_t queue_index = select_queue_for_push(task);
        if (queue_index < _cohort_list.size())
        {
          return _cohort_list[queue_index]->push(task);
        }

        return false;
      }
      /**
       * @brief 从队列中取出任务（阻塞）
       * @return 任务智能指针，队列关闭时返回`nullptr`
       */
      _interior_task_ptr pop() override
      {
        while (!_closed.load(std::memory_order_acquire))
        {
          std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);

          if (_cohort_list.empty())
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
          }

          // 根据调度策略选择队列
          auto queue_index = select_queue_for_pop();
          if (queue_index < _cohort_list.size())
          {
            auto task = _cohort_list[queue_index]->try_pop();
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
       * @return 任务智能指针，队列为空时返回`nullptr`
       */
      _interior_task_ptr try_pop() override
      {
        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);

        if (_cohort_list.empty())
        {
          return nullptr;
        }

        // 尝试从所有队列中获取任务
        for (std::size_t i = 0; i < _cohort_list.size(); ++i)
        {
          auto queue_index = select_queue_for_pop();
          if (queue_index < _cohort_list.size())
          {
            auto task = _cohort_list[queue_index]->try_pop();
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
       * @return 任务智能指针，超时或队列关闭时返回`nullptr`
       */
      _interior_task_ptr try_pop_for(const std::chrono::milliseconds &timeout) override
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
        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);

        std::size_t total_size = 0;
        for (const auto &queue : _cohort_list)
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
        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);

        for (const auto &queue : _cohort_list)
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
        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);

        for (auto &queue : _cohort_list)
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

        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);
        for (auto &queue : _cohort_list)
        {
          queue->close();
        }
      }

      /**
       * @brief 检查队列是否已关闭
       * @return `true` 队列已关闭，`false` 队列未关闭
       */
      bool closed() const override
      {
        return _closed.load(std::memory_order_acquire);
      }

      /**
       * @brief 获取队列策略
       * @return 队列调度策略
       */
      rank_strategy get_strategy() const override
      {
        return _strategy;
      }

      /**
       * @brief 获取子队列数量
       * @return 子队列数量
       */
      std::size_t get_sub_queue_count() const
      {
        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);
        return _cohort_list.size();
      }

      /**
       * @brief 获取指定索引的子队列大小
       * @param index 子队列索引
       * @return 子队列大小
       */
      std::size_t get_sub_queue_size(std::size_t index) const
      {
        std::shared_lock<std::shared_mutex> lock(_multi_cohort_mutex);
        if (index < _cohort_list.size())
        {
          return _cohort_list[index]->size();
        }
        return 0;
      }
    private:
      /**
       * @brief 为推送任务选择队列
       * @param task 要推送的任务
       * @return 队列索引
       */
      std::size_t select_queue_for_push(_interior_task_ptr task)
      {
        switch (_strategy)
        {
        case rank_strategy::priority:
        {
          // 根据任务优先级选择队列
          auto priority = static_cast<int>(task->get_priority());
          return std::min(static_cast<std::size_t>(priority), _cohort_list.size() - 1);
        }
        case rank_strategy::round_robin:
        default:
        {
          // 轮询选择队列
          auto index = _current_index.fetch_add(1, std::memory_order_relaxed);
          return index % _cohort_list.size();
        }
        }
      }
      /**
       * @brief 为弹出任务选择队列
       * @return 队列索引
       */
      std::size_t select_queue_for_pop()
      {
        switch (_strategy)
        {
        case rank_strategy::priority:
        {
          // 优先级策略：从高优先级队列开始查找
          for (std::size_t i = _cohort_list.size(); i > 0; --i)
          {
            std::size_t index = i - 1;
            if (!_cohort_list[index]->empty())
            {
              return index;
            }
          }
          return 0;
        }
        case rank_strategy::round_robin:
        default:
        {
          // 轮询策略
          auto index = _current_index.fetch_add(1, std::memory_order_relaxed);
          return index % _cohort_list.size();
        }
        }
      }
    };
    /**
     * @brief 任务队列工厂函数 - 创建FIFO队列
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<cohort_order> make_cohort_order(std::size_t max = 0)
    {
      return std::make_unique<cohort_order>(max);
    }
    /**
     * @brief 任务队列工厂函数 - 创建优先级队列
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<cohort_prior> make_cohort_prior(std::size_t max_size = 0)
    {
      return std::make_unique<cohort_prior>(max_size);
    }
    /**
     * @brief 任务队列工厂函数 - 创建延迟队列
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<cohort_delay> make_cohort_delay(std::size_t max_size = 0)
    {
      return std::make_unique<cohort_delay>(max_size);
    }
    /**
     * @brief 任务队列工厂函数 - 创建多级队列
     * @param policy 调度策略
     * @return 队列智能指针
     */
    inline std::unique_ptr<cohort_multi> make_cohort_multi(rank_strategy policy = rank_strategy::round_robin)
    {
      return std::make_unique<cohort_multi>(policy);
    }
    /**
     * @brief 任务队列工厂函数 - 根据策略创建队列
     * @param policy 队列策略
     * @param max_size 最大队列大小
     * @return 队列智能指针
     */
    inline std::unique_ptr<cohort_base> make_cohort(rank_strategy policy, std::size_t max_size = 0)
    {
      switch (policy)
      {
      case rank_strategy::fifo:
        return make_cohort_order(max_size);
      case rank_strategy::priority:
        return make_cohort_prior(max_size);
      case rank_strategy::delay:
        return make_cohort_delay(max_size); 
      case rank_strategy::round_robin:
        return make_cohort_multi(policy);
      default:
        return make_cohort_order(max_size);
      }
    }
  }
}
namespace pool
{
  using internals::structure_c::automatic_derivation;

  using internals::structure_c::cohort_delay;
  using internals::structure_c::cohort_multi;
  using internals::structure_c::cohort_order;
  using internals::structure_c::cohort_prior;

  using internals::structure_c::make_cohort_delay;
  using internals::structure_c::make_cohort_multi;
  using internals::structure_c::make_cohort_order;
  using internals::structure_c::make_cohort_prior;

  using internals::structure_c::rank_strategy;
  using internals::structure_c::make_cohort;
}