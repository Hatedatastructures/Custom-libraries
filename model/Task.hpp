#pragma once

// #include "./standard_concurrent/Concurrent_container.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <future>
#include <condition_variable>
#include <coroutine>
#include <typeinfo>
#include <any>
#include <string>
#include <utility>
#include <type_traits> 
namespace _implemented_internally
{
  namespace task_structure
  {
    /**
     * @class derivation
     * @brief 任务返回值封装类
     * @details 用于封装任务的返回值，支持任意类型的返回值，同时支持void类型的返回值
     * @warning 不支持对void类型的返回值进行转换,不能用auto来接收
     */
    class task_anomaly;
    class derivation
    { 
    private:
      std::any _data;
      bool _void;
    public:
      template<typename return_type>
      derivation(return_type&& value) : _data(std::forward<return_type>(value)), _void(false) {}
      derivation() : _void(true) {}
      /**
       * @brief 隐式类型转换
       * @tparam implicit_type 
       */
      template<typename implicit_type>
      operator implicit_type() const
      {
        static_assert(!std::is_void_v<implicit_type>, "不能对void类型转换");
        if(_void)
        {
          throw task_anomaly("void类型不能转换",0);
        }
        return std::any_cast<implicit_type>(_data);
      }
      /**
       * @brief 判断任务是否为void类型
       * @return `true` - `void`类型, `false` - 非`void`类型
       */
      bool is_void() const noexcept
      {
        return _void;
      }
      /**
       * @brief 判断任务是否有返回值
       * @return `true` - 有返回值, `false` - 无返回值
       */
      bool has_value()const noexcept
      {
        return !_void && _data.has_value();
      }
      /**
       * @brief 显式获取任务返回值
       * @tparam return_type 任务返回值类型
       */
      template<typename return_type>
      return_type get() const
      {
        if (_void) 
        {
          throw task_anomaly("任务无返回值，无法获取结果", 0);
        }
        try 
        {
          return std::any_cast<return_type>(_data);
        }
        catch (const std::bad_any_cast& e) 
        {
          throw task_anomaly("类型转换失败: " + std::string(e.what()), 0);
        }
      }
    };
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
     * @enum urgency_level
     * @brief 任务优先级枚举
     */
    enum class urgency_level : std::int32_t
    {
      lowest = -100, // 最低优先级
      low = -50,     // 低优先级
      normal = 0,    // 普通优先级
      high = 50,     // 高优先级
      highest = 100, // 最高优先级
      critical = 200 // 关键优先级
    };
    inline std::string task_state_to_string(task_state state) noexcept
    {
      switch (state)
      {
        case task_state::pending:    return "pending";
        case task_state::running:    return "running";
        case task_state::completed:  return "completed";
        case task_state::cancelled:  return "cancelled";
        case task_state::timeout:    return "timeout";
        case task_state::failed:     return "failed";
        default:                     return "unknown";
      }
    }

    // 同理，为优先级枚举添加转换函数
    inline std::string urgency_level_to_string(urgency_level level) noexcept
    {
      switch (level)
      {
        case urgency_level::lowest:  return "lowest";
        case urgency_level::low:     return "low";
        case urgency_level::normal:  return "normal";
        case urgency_level::high:    return "high";
        case urgency_level::highest: return "highest";
        case urgency_level::critical:return "critical";
        default:                     return std::to_string(static_cast<int>(level));
      }
    }
    /**
     * @class task_anomaly
     * @brief 任务执行异常类
     */
    class task_anomaly : public std::exception
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
      explicit task_anomaly(const std::string &message, std::uint64_t task_id = 0)
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
     * @class task_base
     * @brief 任务基类 - 定义所有任务的通用接口
     *
     * 核心功能： 线程安全的任务状态管理, 优先级设置和获取, 超时时间管理
     * ,任务取消机制, 执行时间统计
     * 
     * 被调用者：`worker`线程、`scheduler`调度器
     * 
     *  调用者：衍生任务类、`thread_pool`管理器
     */
    class task_base
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
      explicit task_base(const std::string &name = "", urgency_level priority = urgency_level::normal)
      : _task_id(_next_task_id.fetch_add(1, std::memory_order_relaxed)),
      _task_name(coverage_string(name)), _submit_time(std::chrono::steady_clock::now()),
      _priority(static_cast<std::int32_t>(priority))  {}
      virtual ~task_base() = default;
      task_base(const task_base &) = delete;
      task_base &operator=(const task_base &) = delete;
      task_base(task_base &&) = delete;
      task_base &operator=(task_base &&) = delete;
      /**
       * @brief 执行任务 - 纯虚函数，子类必须实现
       * @return 任务执行结果(`derivation`类型支持任意返回值)
       * @throws `task_anomaly` 任务执行异常
       *
       * 调用者：`worker`线程,被调用者：衍生任务类的具体实现
       */
      virtual derivation execute() = 0;
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
       * @brief 获取任务结果
       * @return  任务执行结果(`derivation`类型支持任意返回值)
       */
      virtual derivation get_result() = 0;
      /**
       * @brief 检查任务是否有返回值
       * @return `true` 无返回值，`false` 有返回值
       */
      virtual bool is_void_task() const noexcept = 0;
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
      void set_priority(urgency_level priority) noexcept
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
      template<typename rep, typename period>
      void set_timeout(const std::chrono::duration<rep, period>& timeout)
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
      template<typename rep, typename period>
      bool wait_for(const std::chrono::duration<rep, period>& timeout) const
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
    std::atomic<std::uint64_t> task_base::_next_task_id{1};
    /**
     * @class task_norm
     * @brief 普通任务类 - 无返回值的简单异步任务
     *
     * 适用场景：简单的异步操作, 不需要返回值的任务, 日志记录、数据清理等
     * 
     * 调用关系：继承自`task_base`, 由`thread_pool::submit()`创建, 由`worker`线程执行
     */
    class task_norm : public task_base
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
      explicit task_norm(func&& function, const std::string& name = "",urgency_level priority = urgency_level::normal)
      : task_base(name, priority),_task_func(std::forward<func>(function)) {}

      /**
       * @brief 执行任务
       * @return 空的 `derivation`（普通任务无返回值）
       * @throws `task_anomaly` 任务执行异常
       */
      derivation execute() override
      {
        if(!this->mark_running())
        {
          throw task_anomaly("任务无法启动", get_task_id());
        }
        try
        {
          _task_func();
          this->mark_completed();
          return derivation();
        }
        catch (const std::exception& task_error)
        {
          this->mark_failed();
          throw task_anomaly("正常任务执行失败: " + std::string(task_error.what()), get_task_id());
        }
        catch (...)
        {
          this->mark_failed();
          throw task_anomaly("正常任务执行失败：未知异常: ", get_task_id());
        }
      }
      /**
       * @brief 获取任务结果
       * @return 空的 `derivation`（普通任务无返回值）如果任务有返回值明确类型会隐式转换
       */
      derivation get_result() override
      {
        return derivation();
      }
      /**
       * @brief 检查任务是否有返回值
       * @return `true` 无返回值，`false` 有返回值
       */
      bool is_void_task() const noexcept override
      {
        return true;
      }
    };
    /**
     * @class task_rslt
     * @brief 带返回值任务类 - 支持异步结果获取
     * @tparam return_t 返回值类型
     *
     * 适用场景：需要获取执行结果的任务,计算密集型任务,数据处理和转换
     * 
     * 调用关系：继承自`task_base`,使用`std::promise`和`std::future`实现结果同步
     * 由`thread_pool::submit()`创建,由`worker`线程执行
     */
    template<typename return_t>
    class task_rslt : public task_base
    {
    private:
      std::promise<return_t> _promise;             // 结果承诺
      std::future<return_t> _future;               // 结果期望

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
      explicit task_rslt(func&& function,const std::string& name = "",urgency_level priority = urgency_level::normal)
      : task_base(name, priority),_promise(), _future(_promise.get_future()), _task_func(std::forward<func>(function)) {}

      /**
       * @brief 执行任务
       * @return 任务执行结果（包装在 `std::any` 中）
       * @throws `task_anomaly` 任务执行异常
       */
      derivation execute() override
      {
        if(!this->mark_running())
        {
          _promise.set_exception(std::make_exception_ptr(task_anomaly("任务无法启动",get_task_id())));
          _result_ready.store(true, std::memory_order_release);
          throw task_anomaly("任务无法启动", get_task_id());
        }
        try
        {
          if constexpr (std::is_void_v<return_t>)
          {
            _task_func();
            _promise.set_value();
            _result_ready.store(true, std::memory_order_release);
            this->mark_completed();
            return derivation();
          }
          else
          {
            auto result = _task_func();
            _promise.set_value(result);
            _result_ready.store(true, std::memory_order_release);
            this->mark_completed();
            return derivation(result);
          }
        }
        catch (const std::exception& e)
        {
          this->mark_failed();
          _promise.set_exception(std::current_exception());
          _result_ready.store(true, std::memory_order_release);
          throw task_anomaly("任务执行失败: " + std::string(e.what()), get_task_id());
        }
        catch (...)
        {
          _promise.set_exception(std::current_exception());
          _result_ready.store(true, std::memory_order_release);
          this->mark_failed();
          throw task_anomaly("任务执行失败: 未知错误", get_task_id());
        }
      }
      /**
       * @brief 检查任务是否有返回值
       * @return `true` 无返回值，`false` 有返回值
       */
      bool is_void_task() const noexcept override
      {
        return false;
      }

      /**
       * @brief 获取任务执行结果（阻塞等待）
       * @return 任务执行结果
       * @throws 任务执行过程中的异常
       */
      derivation get_result() override
      {
        if constexpr (std::is_void_v<return_t>)
        {
          _future.get();
          return derivation();
        }
        else
        {
          return derivation(_future.get());
        }
      }
      /**
       * @brief 获取任务执行结果（带超时）
       * @return 任务执行结果
       */
      return_t get()
      {
        return _future.get();
      }
      /**
       * @brief 获取任务执行结果（带超时）
       * @param timeout 等待超时时间
       * @return 当返回值为非void类型时返回bool类型，反之如果没超时返回任务的结果，
       * 超时返回空值(返回值成员函数has_value判断)
       */
      template<typename rep, typename period>
      std::conditional_t<std::is_void_v<return_t>, bool ,std::optional<return_t>> 
      get_result_for(const std::chrono::duration<rep, period>& timeout)
      {
        if (_future.wait_for(timeout) == std::future_status::ready)
        {
          if constexpr (std::is_void_v<return_t>)
          {
            _future.get();
            return true;
          }
          else
          {
            return _future.get();
          }
        }
        return std::conditional_t<std::is_void_v<return_t>, bool ,std::optional<return_t>>{};
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
       * @return  当返回值为非void类型时返回bool类型，反之如果没超时返回任务的结果，
       * 超时返回空值(返回值成员函数has_value判断)
       */
      std::conditional_t<std::is_void_v<return_t>, bool, std::optional<return_t>>
      try_get_result() 
      {
        if (_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) 
        {
          if constexpr (std::is_void_v<return_t>) 
          {
            _future.get();
            return true;  // void 类型返回是否成功
          } 
          else 
          {
            return _future.get();  // 非 void 类型返回 optional
          }
        }
        return std::conditional_t<std::is_void_v<return_t>, bool, std::optional<return_t>>{};
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
    /**
     * @class task_prio
     * @brief 优先级任务类 - 支持调度器按优先级排序
     *
     * 适用场景：需要优先执行的重要任务，系统关键操作，用户交互响应
     *
     * 调用关系：继承自`task_rslt`,由`task_queue`（基于`concurrent_priority_queue`）按优先级排序
     * 由`scheduler`调度器管理
     */
    template <typename return_t>
    class task_prio : public task_rslt<return_t>
    {
    public:
      /**
       * @brief 构造优先级任务
       * @param func 任务执行函数
       * @param priority 任务优先级
       * @param name 任务名称
       */
      template<typename func>
      explicit task_prio(func&& function,urgency_level priority,const std::string& name = "")
      : task_rslt<return_t>(std::forward<func>(function), name, priority) {}
      /**
       * @brief 构造优先级任务（自定义优先级值）
       * @param func 任务执行函数
       * @param priority_value 自定义优先级值
       * @param name 任务名称
       */
      template<typename func>
      explicit task_prio(func&& function,std::int32_t priority_value,const std::string& name = "")
      : task_rslt<return_t>(std::forward<func>(function), name, urgency_level::normal)
      {
        this->set_priority(priority_value);
      }
      /**
       * @brief 比较操作符（用于优先级队列排序）
       * @param other 另一个任务
       * @return true 当前任务优先级更高
       */
      // 在 task_base 中显式删除移动构造和移动赋值
      
      bool operator<(const task_prio& other) const noexcept
      {
        return this->task_base::get_priority() < other.task_base::get_priority();
      }
      /**
       * @brief 比较操作符（用于优先级队列排序）
       * @param other 另一个任务
       * @return true 当前任务优先级更高
       */
      bool operator>(const task_prio& other) const noexcept
      { //get_priority函数继承至基类任务类
        return this->task_base::get_priority()  > other.task_base::get_priority();
      }
      derivation get_result() override
      {
        return this->task_rslt<return_t>::get_result();
      }
      urgency_level get_priority() const noexcept
      {
        return static_cast<urgency_level>(this->task_base::get_priority());
      }
      return_t get()
      {
        return this->_future.get();

      }
    };
    /**
     * @class task_time
     * @brief 超时任务类 - 支持超时检查和处理
     *
     * 适用场景：有时间限制的任务, 网络请求和`IO`操作, 需要及时响应的任务
     *
     * 调用关系： 继承自`task_rslt`, 由`scheduler`定期检查超时, 支持超时回调处理
     */
    template <typename return_t>
    class task_time : public task_rslt<return_t>
    {
    private:
      std::function<void()> _timeout_callback;       // 超时回调函数
      std::atomic<bool> _timeout_handled{false};     // 超时是否已处理
    public:
      /**
       * @brief 构造超时任务
       * @param func 任务执行函数
       * @param timeout 超时时长
       * @param name 任务名称
       * @param priority 任务优先级
       */
      template<typename func, typename rep, typename period>
      explicit task_time(func&& function, const std::chrono::duration<rep, period>& timeout,const std::string& name = "",
      urgency_level priority = urgency_level::normal)
      : task_rslt<return_t>(std::forward<func>(function), name, priority)
      {
        this->task_base::set_timeout(timeout);
      }
      /**
       * @brief 构造带超时回调的超时任务
       * @param func 任务执行函数
       * @param timeout 超时时长
       * @param timeout_callback 超时回调函数
       * @param name 任务名称
       * @param priority 任务优先级
       */
      template<typename func, typename timeout_func, typename rep, typename period>
      explicit task_time(func&& function,const std::chrono::duration<rep, period>& timeout,
      timeout_func&& timeout_callback,const std::string& name = "",urgency_level priority = urgency_level::normal)
      : task_rslt<return_t>(std::forward<func>(function), name, priority),
      _timeout_callback(std::forward<timeout_func>(timeout_callback))
      {
        this->task_base::set_timeout(timeout);
      }
      /**
       * @brief 标记任务超时（重写`task_base`类方法）
       * @return `true` 标记成功，`false` 任务已开始执行
       */
      bool mark_timeout() override
      {
        if (task_base::mark_timeout())
        {
          handle_timeout();
          return true;
        }
        return false;
      }
      /**
       * @brief 处理超时事件
       */
      void handle_timeout()
      {
        bool expected = false;
        if (_timeout_handled.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
          if (_timeout_callback)
          {
            try
            {
              _timeout_callback();
            }
            catch (...)
            {
              // 超时回调异常不应影响主流程
            }
          }
        }
      }
      /**
       * @brief 设置超时回调函数
       * @param callback 超时回调函数
       */
      template<typename timeout_func>
      void set_timeout_callback(timeout_func&& callback)
      {
        _timeout_callback = std::forward<timeout_func>(callback);
      }

      /**
       * @brief 检查超时是否已处理
       * @return `true` 已处理，`false` 未处理
       */
      bool is_timeout_handled() const noexcept
      {
        return _timeout_handled.load(std::memory_order_acquire);
      }
    };
    /**
     * @class task_depn
     * @brief 依赖任务类 - 支持任务间依赖关系
     *
     * 适用场景：需要按顺序执行的任务链, 数据流水线处理,复杂业务逻辑分解
     *
     * 调用关系：继承自`task_rslt`, 依赖其他任务的完成状态, 由`scheduler`检查依赖关系
     */
    template <typename return_t, uint64_t CACHE_VALIDITY = 100ULL>
    class task_depn : public task_rslt<return_t>
    {
    private:
      std::vector<std::shared_ptr<task_base>> _dependencies;    // 依赖的任务列表
      mutable std::atomic<bool> _dependencies_satisfied{false}; // 依赖是否已满足（缓存）
      mutable std::atomic<std::uint64_t> _last_check_time{0};   // 上次检查时间戳
      mutable std::mutex _dependency_mutex;                     // 依赖检查互斥锁
      mutable std::condition_variable _dependency_cv;           // 依赖状态变更条件变量
      
      static constexpr std::uint64_t CACHE_VALIDITY_MS = CACHE_VALIDITY;   // 缓存有效期（毫秒）
      
      /**
        * @brief 获取当前时间戳（毫秒）
        * @return 当前时间戳
        */
      static std::uint64_t get_current_time_ms()
      {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
      }
      
      /**
        * @brief 不安全的依赖检查（假设已持有锁）
        * @return 如果所有依赖都已完成则返回true
        */
      bool are_dependencies_satisfied_unsafe() const
      {
        auto satisfied_func = [](const std::shared_ptr<task_base>& dep) 
        {
          return dep && dep->get_state() == task_state::completed;
        };
        return std::all_of(_dependencies.begin(), _dependencies.end(),satisfied_func);
      }
    public:
      /**
       * @brief 构造依赖任务
       * @param func 任务执行函数
       * @param dependencies 依赖的任务列表
       * @param name 任务名称
       * @param priority 任务优先级
       */
      template<typename func>
      explicit task_depn(func&& function, const std::vector<std::shared_ptr<task_base>>& dependencies,
      const std::string& name = "", urgency_level priority = urgency_level::normal)
      : task_rslt<return_t>(std::forward<func>(function), name, priority), _dependencies(dependencies)
      {
        // 过滤掉空指针依赖
        _dependencies.erase(std::remove_if(_dependencies.begin(), _dependencies.end(),
        [](const std::shared_ptr<task_base>& dep) { return !dep; }),_dependencies.end());
      }
      /**
       * @brief 构造依赖任务（单个依赖）
       * @param func 任务执行函数
       * @param dependency 依赖的任务
       * @param name 任务名称
       * @param priority 任务优先级
       */
      template <typename func>
      explicit task_depn(func &&function, std::shared_ptr<task_base> dependency, const std::string &name = "",
      urgency_level priority = urgency_level::normal)
      : task_rslt<return_t>(std::forward<func>(function), name, priority)
      {
        if (dependency)
        {
          _dependencies.push_back(std::move(dependency));
        }
      }
      /**
       * @brief 添加依赖任务
       * @param dependency 依赖的任务
       */
      void add_dependency(std::shared_ptr<task_base> dependency)
      {
        std::lock_guard<std::mutex> lock(_dependency_mutex);
        if (dependency && this->task_base::get_state() == task_state::pending)
        {
          _dependencies.push_back(std::move(dependency));
          // 重置缓存状态
          _dependencies_satisfied.store(false, std::memory_order_release);
          _last_check_time.store(0, std::memory_order_release);
        }
      }
      /**
       * @brief 检查所有依赖是否完成
       * @return `true` 所有依赖已完成，`false` 存在未完成的依赖
       */
      bool are_dependencies_satisfied() const
      {
        // 检查缓存是否有效
        auto current_time = get_current_time_ms();
        auto last_check = _last_check_time.load(std::memory_order_acquire);
        
        if (_dependencies_satisfied.load(std::memory_order_acquire) && 
        (current_time - last_check) < CACHE_VALIDITY_MS)
        {
          return true;
        }
        
        std::lock_guard<std::mutex> lock(_dependency_mutex);
        
        // 双重检查锁定模式
        if (_dependencies_satisfied.load(std::memory_order_acquire) && 
        (get_current_time_ms() - _last_check_time.load(std::memory_order_acquire)) < CACHE_VALIDITY_MS)
        {
          return true;
        }
        auto satisfied_func = [](const std::shared_ptr<task_base>& dep) 
        {
          return dep && dep->get_state() == task_state::completed;
        };
        // 批量检查所有依赖
        bool all_satisfied = std::all_of(_dependencies.begin(), _dependencies.end(),satisfied_func);
        
        // 更新缓存
        _dependencies_satisfied.store(all_satisfied, std::memory_order_release);
        _last_check_time.store(get_current_time_ms(), std::memory_order_release);
        
        return all_satisfied;
      }
      /**
       * @brief 获取未完成的依赖任务
       * @return 未完成的依赖任务列表
       */
      std::vector<std::shared_ptr<task_base>> get_pending_dependencies() const
      {
        std::lock_guard<std::mutex> lock(_dependency_mutex);
        std::vector<std::shared_ptr<task_base>> pending;
        pending.reserve(_dependencies.size()); // 预分配内存
        auto get_pending_func = [](const std::shared_ptr<task_base>& dep) 
        {
          if (!dep) return false;
          auto state = dep->get_state();
          return state != task_state::completed && state != task_state::cancelled && 
          state != task_state::timeout && state != task_state::failed;
        };
        std::copy_if(_dependencies.begin(), _dependencies.end(), std::back_inserter(pending),get_pending_func);
        
        return pending;
      }
      /**
       * @brief 获取依赖任务数量
       * @return 依赖任务数量
       */
      std::size_t get_dependency_count() const
      {
        std::lock_guard<std::mutex> lock(_dependency_mutex);
        return _dependencies.size();
      }
      /**
       * @brief 等待所有依赖完成
       * @param timeout 等待超时时间
       * @return true 所有依赖完成，false 等待超时
       */
      template <typename rep, typename period>
      bool wait_for_dependencies(const std::chrono::duration<rep, period> &timeout) const
      {
        std::unique_lock<std::mutex> lock(_dependency_mutex);
        return _dependency_cv.wait_for(lock, timeout, [this]() {
          return are_dependencies_satisfied_unsafe();
        });
      }
    };
    /**
     * @class task_coro
     * @brief 协程任务类 - 支持`C++20`协程
     *
     * 适用场景：异步IO操作, 状态机实现, 复杂的异步流程控制
     *
     * 调用关系：继承自`task_rslt`, 使用`C++20`协程特性, 支持协程的暂停和恢复
     */
    template <typename return_t>
    class task_coro : public task_rslt<return_t>
    {
    public:
      /**
       * @brief 协程任务承诺类型
       */
      struct promise_type
      {
        task_coro get_return_object()
        {
          return task_coro{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void unhandled_exception()
        {
          _exception = std::current_exception();
        }

        template <typename value>
        void return_value(value &&value_data)
          requires(!std::is_void_v<return_t>)
        {
          _result = std::forward<value>(value_data);
        }

        void return_void()
          requires std::is_void_v<return_t>{}

        std::exception_ptr _exception;
        std::conditional_t<std::is_void_v<return_t>, std::monostate, return_t> _result;
      };

      using handle_type = std::coroutine_handle<promise_type>;
    private:
      handle_type _coroutine_handle;                 // 协程句柄
      std::atomic<bool> _coroutine_completed{false}; // 协程是否完成
    public:
      /**
       * @brief 构造协程任务
       * @param handle 协程句柄
       * @param name 任务名称
       * @param priority 任务优先级
       */
      explicit task_coro(handle_type handle,const std::string &name = "",urgency_level priority = urgency_level::normal)
      : task_rslt<return_t>([this]() -> return_t { return execute_coroutine(); }, name, priority),_coroutine_handle(handle) {}
      /**
       * @brief 移动构造函数
       */
      task_coro(task_coro &&other) noexcept
      : task_rslt<return_t>(std::move(other)), _coroutine_handle(std::exchange(other._coroutine_handle, {})),
      _coroutine_completed(other._coroutine_completed.load()) {}
      /**
       * @brief 析构函数
       */
      ~task_coro()
      {
        if (_coroutine_handle)
        {
          _coroutine_handle.destroy();
        }
      }
      /**
       * @brief 检查协程是否完成
       * @return `true` 协程完成，`false` 协程未完成
       */
      bool is_coroutine_done() const noexcept
      {
        return _coroutine_handle.done();
      }
      /**
       * @brief 恢复协程执行
       */
      void resume_coroutine()
      {
        if (_coroutine_handle && !_coroutine_handle.done())
        {
          _coroutine_handle.resume();
        }
      }
    private:
      /**
       * @brief 执行协程
       * @return 协程执行结果
       */
      return_t execute_coroutine()
      {
        if (!_coroutine_handle)
        {
          throw task_anomaly("协程句柄无效", this->task_base::get_task_id());
        }

        // 等待协程完成
        while (!_coroutine_handle.done())
        {
          std::this_thread::yield();
        }

        auto &promise = _coroutine_handle.promise();
        if (promise._exception)
        {
          std::rethrow_exception(promise._exception);
        }

        if constexpr (!std::is_void_v<return_t>)
        {
          return promise._result;
        }
      }
    };
    /**
     * @brief 任务工厂函数 - 创建普通任务
     * @param func 任务执行函数
     * @param name 任务名称
     * @param priority 任务优先级
     * @return 任务智能指针
     */
    template <typename func>
    std::shared_ptr<task_norm> make_task_norm(func &&function, const std::string &name = "",
    urgency_level priority = urgency_level::normal)
    {
      return std::make_shared<task_norm>(std::forward<func>(function), name, priority);
    }
    /**
     * @brief 任务工厂函数 - 创建带返回值任务
     * @param func 任务执行函数
     * @param name 任务名称
     * @param priority 任务优先级
     * @return 任务智能指针
     */
    template <typename func>
    auto make_task_rslt(func &&function, const std::string &name = "",urgency_level priority = urgency_level::normal)
    {
      using return_type = std::invoke_result_t<func>;
      return std::make_shared<task_rslt<return_type>>(std::forward<func>(function), name, priority);
    }
      /**
     * @brief 任务工厂函数 - 创建优先级任务
     * @param func 任务执行函数
     * @param priority 任务优先级
     * @param name 任务名称
     * @return 任务智能指针
     */
    template <typename func>
    std::shared_ptr<task_prio<std::invoke_result_t<func>>> make_task_prio(func &&function, urgency_level priority,
    const std::string &name = "")
    {
      using return_type = std::invoke_result_t<func>;
      return std::make_shared<task_prio<return_type>>(std::forward<func>(function), priority, name);
    }
    /**
     * @brief 任务工厂函数 - 创建超时任务
     * @param func 任务执行函数
     * @param timeout 超时时长
     * @param name 任务名称
     * @param priority 任务优先级
     * @return 任务智能指针
     */
    template <typename func, typename rep, typename period>
    std::shared_ptr<task_time<std::invoke_result_t<func>>> make_task_time(func &&function, 
    const std::chrono::duration<rep, period> &timeout,const std::string &name = "",
    urgency_level priority = urgency_level::normal)
    {
      using return_type = std::invoke_result_t<func>;
      return std::make_shared<task_time<return_type>>(std::forward<func>(function), timeout, name, priority);
    }
    /**
     * @brief 任务工厂函数 - 创建依赖任务
     * @param func 任务执行函数
     * @param dependencies 依赖的任务列表
     * @param name 任务名称
     * @param priority 任务优先级
     * @return 任务智能指针
     */
    template <typename func, uint64_t CACHE_VALIDITY = 100ULL>
    std::shared_ptr<task_depn<std::invoke_result_t<func>, CACHE_VALIDITY>> make_task_depn(func &&function,
    const std::vector<std::shared_ptr<task_base>> &dependencies,const std::string &name = "",
    urgency_level priority = urgency_level::normal)
    {
      using return_type = std::invoke_result_t<func>;
      return std::make_shared<task_depn<return_type, CACHE_VALIDITY>>(std::forward<func>(function), dependencies, name, priority);
    }
    /**
     * @brief 任务工厂函数 - 创建协程任务
     * @param coro 协程句柄
     * @param name 任务名称
     * @param priority 任务优先级
     * @return 任务智能指针
     */
    template <typename coroutine_t>
    std::shared_ptr<task_coro<std::invoke_result_t<coroutine_t>>> make_task_coro(coroutine_t &&coro,
    const std::string &name = "",urgency_level priority = urgency_level::normal)
    {
      using return_type = std::invoke_result_t<coroutine_t>;
      return std::make_shared<task_coro<return_type>>(std::forward<coroutine_t>(coro), name, priority);
    }
  }
}
using task_ptr = std::shared_ptr<_implemented_internally::task_structure::task_base>;
namespace pool
{
  using _implemented_internally::task_structure::task_base;
  using _implemented_internally::task_structure::task_norm;
  using _implemented_internally::task_structure::task_rslt;
  using _implemented_internally::task_structure::task_prio;
  using _implemented_internally::task_structure::task_time;
  using _implemented_internally::task_structure::task_depn;
  using _implemented_internally::task_structure::task_coro;

  using _implemented_internally::task_structure::make_task_norm;
  using _implemented_internally::task_structure::make_task_rslt;
  using _implemented_internally::task_structure::make_task_prio;
  using _implemented_internally::task_structure::make_task_time;
  using _implemented_internally::task_structure::make_task_depn;
  using _implemented_internally::task_structure::make_task_coro;

  using _implemented_internally::task_structure::urgency_level;

  using task_base_ptr = std::shared_ptr<_implemented_internally::task_structure::task_base>;
}