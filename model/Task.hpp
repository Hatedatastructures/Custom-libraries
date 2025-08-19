#pragma once
#include "./standard_concurrent/Concurrent_container.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <future>
#include <condition_variable>
#include <coroutine>
#include <typeinfo>
#include <any>
#include <string>

namespace tp
{
  /**
   * @enum task_state
   * @brief 任务状态枚举
   */
  enum class task_state : std::uint8_t
  {
    pending = 0,   // 等待执行
    running = 1,   // 正在执行
    completed = 2, // 执行完成
    cancelled = 3, // 已取消
    timeout = 4,   // 执行超时
    failed = 5     // 执行失败
  };
  /**
   * @enum task_priority
   * @brief 任务优先级枚举
   */
  enum class task_priority : std::int32_t
  {
    lowest = -100, // 最低优先级
    low = -50,     // 低优先级
    normal = 0,    // 普通优先级
    high = 50,     // 高优先级
    highest = 100, // 最高优先级
    critical = 200 // 关键优先级
  };
  /**
   * @class task_exception
   * @brief 任务执行异常类
   */
  class task_exception : public std::exception
  {
  private:
    std::string _message;
    std::uint64_t _task_id;

  public:
    /**
     * @brief 构造任务异常
     * @param message 异常消息
     * @param task_id 任务ID
     */
    explicit task_exception(const std::string &message, std::uint64_t task_id = 0)
        : _message(message), _task_id(task_id) {}

    /**
     * @brief 获取异常消息
     * @return 异常消息字符串
     */
    const char *what() const noexcept override
    {
      return _message.c_str();
    }

    /**
     * @brief 获取任务ID
     * @return 任务ID
     */
    std::uint64_t get_task_id() const noexcept
    {
      return _task_id;
    }
  };
  /**
   * @class base_task
   * @brief 任务基类 - 定义所有任务的通用接口
   *
   * 核心功能： 线程安全的任务状态管理, 优先级设置和获取, 超时时间管理
   * ,任务取消机制, 执行时间统计
   * 
   * 被调用者：`worker`线程、`scheduler`调度器
   * 
   *  调用者：衍生任务类、`thread_pool`管理器
   */
  class base_task
  {
  protected:
    /**
     * @brief 标记任务开始执行（由`worker`线程调用）
     * @return `true` 标记成功，`false` 任务已被取消或超时
     */
    bool mark_running()
    {
      task_state expected = task_state::pending;
      if (_state.compare_exchange_strong(expected, task_state::running, std::memory_order_acq_rel))
      {
        _start_time = std::chrono::steady_clock::now();
        return true;
      }
      return false;
    }

    /**
     * @brief 标记任务完成（由`worker`线程调用）
     */
    void mark_completed()
    {
      _end_time = std::chrono::steady_clock::now();
      _state.store(task_state::completed, std::memory_order_release);
      std::lock_guard<std::mutex> lock(_state_mutex);
      _state_cv.notify_all();
    }

    /**
     * @brief 标记任务失败（由`worker`线程调用）
     */
    void mark_failed()
    {
      _end_time = std::chrono::steady_clock::now();
      _state.store(task_state::failed, std::memory_order_release);
      std::lock_guard<std::mutex> lock(_state_mutex);
      _state_cv.notify_all();
    }
    std::string coverage_string(const std::string &str) const
    {
      if (str.empty())
      {
        return ("task_" + std::to_string(_task_id));
      }
      return str;
    }
  protected:
    std::uint64_t _task_id;                              // 任务唯一标识
    static std::atomic<std::uint64_t> _next_task_id;     // 全局任务ID生成器

    std::string _task_name;                              // 任务名称
    std::atomic<task_state> _state{task_state::pending}; // 任务状态（原子操作保证线程安全）

    std::chrono::steady_clock::time_point _deadline;     // 超时时间点
    std::chrono::steady_clock::time_point _submit_time;  // 提交时间
    std::chrono::steady_clock::time_point _start_time;   // 开始执行时间
    std::chrono::steady_clock::time_point _end_time;     // 结束执行时间

    std::atomic<bool> _has_deadline{false};              // 是否设置了超时时间
    mutable std::mutex _state_mutex;                     // 状态变更互斥锁
    mutable std::condition_variable _state_cv;           // 状态变更条件变量

    std::atomic<std::int32_t> _priority;                 // 任务优先级
  public:
    /**
     * @brief 构造任务基类
     * @param name 任务名称
     * @param priority 任务优先级
     */
    explicit base_task(const std::string &name = "", task_priority priority = task_priority::normal)
    : _task_id(_next_task_id.fetch_add(1, std::memory_order_relaxed)),
    _task_name(coverage_string(name)), _submit_time(std::chrono::steady_clock::now()),
    _priority(static_cast<std::int32_t>(priority))  {}
    virtual ~base_task() = default;
    base_task(const base_task &) = delete;
    base_task &operator=(const base_task &) = delete;
    base_task(base_task &&) = default;
    base_task &operator=(base_task &&) = default;
    /**
     * @brief 执行任务 - 纯虚函数，子类必须实现
     * @return 任务执行结果(`std::any`类型支持任意返回值)
     * @throws `task_exception` 任务执行异常
     *
     * 调用者：`worker`线程,被调用者：衍生任务类的具体实现
     */
    virtual std::any execute() = 0;
    /**
     * @brief 取消任务
     * @return `true` 取消成功，`false` 任务已开始执行无法取消
     *
     * 调用者：`scheduler`调度器、用户代码,依赖：原子状态变更、条件变量通知
     */
    virtual bool cancel()
    {
      task_state expected = task_state::pending;
      if (_state.compare_exchange_strong(expected, task_state::cancelled, std::memory_order_acq_rel))
      {
        std::lock_guard<std::mutex> lock(_state_mutex);
        _state_cv.notify_all();
        return true;
      }
      return false;
    }
    /**
     * @brief 检查任务是否超时
     * @return `true` 已超时，`false` 未超时或无超时设置
     *
     * 调用者：`scheduler`调度器,依赖：超时时间点比较
     */
    virtual bool is_timeout() const
    {
      if (!_has_deadline.load(std::memory_order_acquire))
        return false;
      return std::chrono::steady_clock::now() > _deadline;
    }
    /**
     * @brief 标记任务超时
     * @return `true` 标记成功，`false` 任务已开始执行
     *
     * 调用者：`scheduler`调度器
     */
    virtual bool mark_timeout()
    {
      task_state expected = task_state::pending;
      if (_state.compare_exchange_strong(expected, task_state::timeout, std::memory_order_acq_rel))
      {
        std::lock_guard<std::mutex> lock(_state_mutex);
        _state_cv.notify_all();
        return true;
      }
      return false;
    }
    /**
     * @brief 获取任务状态
     * @return 当前任务状态
     */
    task_state get_state() const noexcept
    {
      return _state.load(std::memory_order_acquire);
    }
    /**
     * @brief 获取任务优先级
     * @return 任务优先级值
     */
    std::int32_t get_priority() const noexcept
    {
      return _priority.load(std::memory_order_acquire);
    }

    /**
     * @brief 设置任务优先级
     * @param priority 新的优先级
     */
    void set_priority(task_priority priority) noexcept
    {
      _priority.store(static_cast<std::int32_t>(priority), std::memory_order_release);
    }

    /**
     * @brief 设置任务优先级
     * @param priority 新的优先级值
     */
    void set_priority(std::int32_t priority) noexcept
    {
      _priority.store(priority, std::memory_order_release);
    }

    /**
     * @brief 获取任务ID
     * @return 任务唯一标识
     */
    std::uint64_t get_task_id() const noexcept
    {
      return _task_id;
    }

    /**
     * @brief 获取任务名称
     * @return 任务名称
     */
    const std::string& get_task_name() const noexcept
    {
      return _task_name;
    }

    /**
     * @brief 设置超时时间
     * @param timeout 超时时长
     */
    template<typename Rep, typename Period>
    void set_timeout(const std::chrono::duration<Rep, Period>& timeout)
    {
      _deadline = std::chrono::steady_clock::now() + timeout;
      _has_deadline.store(true, std::memory_order_release);
    }

    /**
     * @brief 设置绝对超时时间点
     * @param deadline 超时时间点
     */
    void set_deadline(const std::chrono::steady_clock::time_point& deadline)
    {
      _deadline = deadline;
      _has_deadline.store(true, std::memory_order_release);
    }

    /**
     * @brief 获取任务提交时间
     * @return 提交时间点
     */
    std::chrono::steady_clock::time_point get_submit_time() const noexcept
    {
      return _submit_time;
    }

    /**
     * @brief 获取任务执行时长
     * @return 执行时长（毫秒），如果任务未完成返回0
     */
    std::chrono::milliseconds get_execution_duration() const
    {
      if (_start_time == std::chrono::steady_clock::time_point{} ||_end_time == std::chrono::steady_clock::time_point{})
        return std::chrono::milliseconds{0};
      return std::chrono::duration_cast<std::chrono::milliseconds>(_end_time - _start_time);
    }
    /**
     * @brief 获取任务等待时长
     * @return 等待时长（毫秒），从提交到开始执行
     */
    std::chrono::milliseconds get_wait_duration() const
    {
      if (_start_time == std::chrono::steady_clock::time_point{})
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _submit_time);
      return std::chrono::duration_cast<std::chrono::milliseconds>(_start_time - _submit_time);
    }
    /**
     * @brief 等待任务完成
     * @param timeout 等待超时时间
     * @return `true` 任务完成，`false` 等待超时
     */
    template<typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) const
    {
      std::unique_lock<std::mutex> lock(_state_mutex);
      auto await_func = [this]()
      {
        auto state = _state.load(std::memory_order_acquire);
        return state == task_state::completed || state == task_state::cancelled || state == task_state::timeout ||
        state == task_state::failed;
      };
      return _state_cv.wait_for(lock, timeout, await_func);
    }
    /**
     * @brief 等待任务完成（无超时）
     */
    void wait() const
    {
      std::unique_lock<std::mutex> lock(_state_mutex);
      auto await_func = [this]()
      {
        auto state = _state.load(std::memory_order_acquire);
        return state == task_state::completed || state == task_state::cancelled || state == task_state::timeout ||
        state == task_state::failed;
      };
      _state_cv.wait(lock, await_func);
    }
  };
  std::atomic<std::uint64_t> base_task::_next_task_id{1};
  /**
   * @class normal_task
   * @brief 普通任务类 - 无返回值的简单异步任务
   *
   * 适用场景：简单的异步操作, 不需要返回值的任务, 日志记录、数据清理等
   * 
   * 调用关系：继承自`base_task`, 由`thread_pool::submit()`创建, 由`worker`线程执行
   */
  class normal_task : public base_task
  {
  private:
    std::function<void()> _task_func;  // 任务执行函数

  public:
    /**
     * @brief 构造普通任务
     * @param function 任务执行函数
     * @param name 任务名称
     * @param priority 任务优先级
     */
    template<typename func>
    explicit normal_task(func&& function, const std::string& name = "",task_priority priority = task_priority::normal)
    : base_task(name, priority),_task_func(std::forward<func>(function)) {}

    /**
     * @brief 执行任务
     * @return 空的 `std::any`（普通任务无返回值）
     * @throws `task_exception` 任务执行异常
     */
    std::any execute() override
    {
      try
      {
        _task_func();
        return std::any{};
      }
      catch (const std::exception& task_error)
      {
        throw task_exception("正常任务执行失败: " + std::string(task_error.what()), get_task_id());
      }
      catch (...)
      {
        throw task_exception("正常任务执行失败：未知异常: ", get_task_id());
      }
    }
  };
  /**
   * @class result_task
   * @brief 带返回值任务类 - 支持异步结果获取
   * @tparam return_t 返回值类型
   *
   * 适用场景：需要获取执行结果的任务,计算密集型任务,数据处理和转换
   * 
   * 调用关系：继承自`base_task`,使用`std::promise`和`std::future`实现结果同步
   * 由`thread_pool::submit()`创建,由`worker`线程执行
   */
  template<typename return_t>
  class result_task : public base_task
  {
  private:
    std::future<return_t> _future;               // 结果期望
    std::promise<return_t> _promise;             // 结果承诺

    std::function<return_t()> _task_func;        // 任务执行函数
    std::atomic<bool> _result_ready{false};      // 结果是否就绪

  public:
    /**
     * @brief 构造带返回值任务
     * @param function 任务执行函数
     * @param name 任务名称
     * @param priority 任务优先级
     */
    template<typename func>
    explicit result_task(func&& function,const std::string& name = "",task_priority priority = task_priority::normal)
    : base_task(name, priority), _future(_promise.get_future()), _task_func(std::forward<func>(function)) {}

    /**
     * @brief 执行任务
     * @return 任务执行结果（包装在 `std::any` 中）
     * @throws `task_exception` 任务执行异常
     */
    std::any execute() override
    {
      try
      {
        if constexpr (std::is_void_v<return_t>)
        {
          _task_func();
          _promise.set_value();
          _result_ready.store(true, std::memory_order_release);
          return std::any{};
        }
        else
        {
          auto result = _task_func();
          _promise.set_value(result);
          _result_ready.store(true, std::memory_order_release);
          return std::make_any<return_t>(result);
        }
      }
      catch (const std::exception& e)
      {
        _promise.set_exception(std::current_exception());
        _result_ready.store(true, std::memory_order_release);
        throw task_exception("任务执行失败: " + std::string(e.what()), get_task_id());
      }
      catch (...)
      {
        _promise.set_exception(std::current_exception());
        _result_ready.store(true, std::memory_order_release);
        throw task_exception("任务执行失败: 未知错误", get_task_id());
      }
    }

    /**
     * @brief 获取任务执行结果（阻塞等待）
     * @return 任务执行结果
     * @throws 任务执行过程中的异常
     */
    return_t get_result()
    {
      if constexpr (std::is_void_v<return_t>)
      {
        _future.get();
      }
      else
      {
        return _future.get();
      }
    }

    /**
     * @brief 获取任务执行结果（带超时）
     * @param timeout 等待超时时间
     * @return 任务执行结果的可选值
     */
    template<typename Rep, typename Period>
    std::optional<return_t> get_result_for(const std::chrono::duration<Rep, Period>& timeout)
    {
      if (_future.wait_for(timeout) == std::future_status::ready)
      {
        if constexpr (std::is_void_v<return_t>)
        {
          _future.get();
          return std::optional<return_t>{};
        }
        else
        {
          return _future.get();
        }
      }
      return std::nullopt;
    }

    /**
     * @brief 检查结果是否就绪
     * @return `true` 结果就绪，`false` 结果未就绪
     */
    bool is_result_ready() const noexcept
    {
      return _result_ready.load(std::memory_order_acquire);
    }

    /**
     * @brief 尝试获取结果（非阻塞）
     * @return 结果的可选值
     */
    std::optional<return_t> try_get_result()
    {
      if (_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      {
        if constexpr (std::is_void_v<return_t>)
        {
          _future.get();
          return std::optional<return_t>{};
        }
        else
        {
          return _future.get();
        }
      }
      return std::nullopt;
    }

    /**
     * @brief 获取`future`对象
     * @return 关联的`future`对象
     */
    std::future<return_t>& get_future()
    {
      return _future;
    }

    /**
     * @brief 获取`future`对象（`const`版本）
     * @return 关联的`future`对象
     */
    const std::future<return_t>& get_future() const
    {
      return _future;
    }
  };
}