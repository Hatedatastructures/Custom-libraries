#pragma once
#include <variant>
#include <atomic>
#include <algorithm>
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
#include <optional>

namespace internals
{
  namespace structure_t
  {
    /**
     * @class anomaly
     * @brief #### 任务执行异常类
     */
    class anomaly : public std::exception
    {
    private:
      std::string _message; // 异常消息
      std::uint64_t _identifier; // 任务ID

    public:
      explicit anomaly(const std::string &message, std::uint64_t task_id = 0)
        : _message(message), _identifier(task_id) {}

      const char *what() const noexcept override
      {
        return _message.c_str();
      }
      std::uint64_t get_identifier() const noexcept
      {
        return _identifier;
      }
    };
    /**
     * @class derivation
     * @brief #### 任务返回类型封装类
     * @details 用于封装任务的返回值，支持任意类型的返回值，同时支持`void`类型的返回值
     * @warning 不支持对`void`类型的返回值进行转换
     */
    class derivation
    { 
    private:
      std::any _data;
      bool _void;
    public:
      template<typename convert_t>
      derivation(convert_t&& value) : _data(std::forward<convert_t>(value)), _void(false) {}
      derivation(derivation&& other) noexcept 
      : _data(std::move(other._data)), _void(std::move(other._void)) {}
      derivation& operator= (derivation&& other) noexcept
      {
        if(this != &other)
        {
          _data = std::move(other._data);
          _void = other._void;
        }
        return *this;
      }
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
          throw anomaly("void类型不能转换",0);
        }
        return std::any_cast<implicit_type>(_data);
      }
      /**
       * @brief  #### 判断任务是否为void类型
       * @return `true` - `void`类型, `false` - 非`void`类型
       */
      bool is_void() const noexcept
      {
        return _void;
      }
      /**
       * @brief #### 判断任务是否有返回值
       * @return `true` - 有返回值, `false` - 无返回值
       */
      bool has_value()const noexcept
      {
        return !_void && _data.has_value();
      }
      /**
       * @brief #### 显式获取任务返回值
       * @tparam convert_t 任务返回值类型
       */
      template<typename convert_t>
      convert_t get() const
      {
        if (_void) 
        {
          throw anomaly("任务无返回值，无法获取结果", 0);
        }
        try 
        {
          return std::any_cast<convert_t>(_data);
        }
        catch (const std::bad_any_cast& e) 
        {
          throw anomaly("类型转换失败: " + std::string(e.what()), 0);
        }
      }
    };
    /**
     * @enum current_status
     * @brief #### 任务当前状态枚举
     */
    enum class current_status : std::uint8_t
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
     * @brief #### 任务优先级枚举
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
    inline std::string to_string(current_status state) noexcept
    {
      switch (state)
      {
        case current_status::pending:    return "pending";
        case current_status::running:    return "running";
        case current_status::completed:  return "completed";
        case current_status::cancelled:  return "cancelled";
        case current_status::timeout:    return "timeout";
        case current_status::failed:     return "failed";
        default:                         return "unknown";
      }
    }
    inline std::string to_string(urgency_level level) noexcept
    {
      switch (level)
      {
        case urgency_level::lowest:      return "lowest";
        case urgency_level::low:         return "low";
        case urgency_level::normal:      return "normal";
        case urgency_level::high:        return "high";
        case urgency_level::highest:     return "highest";
        case urgency_level::critical:    return "critical";
        default:                         return std::to_string(static_cast<int>(level));
      }
    }
    /**
     * @class uint_ordinary
     * @brief #### 标准任务类 - 定义所有任务的通用接口
     * @tparam execute_function 任务执行函数类型,可以在上层自动推导来获取任务返回值类型
     * @warning #### 计算结果不可重复获取，创建任务对象通过工厂函数创建
     *
     * 核心功能： 线程安全的任务状态管理, 优先级设置和获取, 超时时间管理
     * ,任务取消机制, 执行时间统计
     * 
     * 被调用者：`worker`线程、`scheduler`调度器
     * 
     *  调用者：衍生任务类、`thread_pool`管理器
     */
    class uint_ordinary
    {
    public:
      /**
       * @brief #### 标记任务开始执行（由`worker`线程调用）
       * @return `true` 标记成功，`false` 任务已被取消或超时
       */
      bool mark_running()
      {
        current_status expected = current_status::pending;
        if (_state.compare_exchange_strong(expected, current_status::running, std::memory_order_acq_rel))
        {
          _start_time = std::chrono::steady_clock::now();
          return true;
        }
        return false;
      }
      /**
       * @brief #### 标记任务完成（由`worker`线程调用）
       */
      void mark_completed()
      {
        _end_time = std::chrono::steady_clock::now();
        _state.store(current_status::completed, std::memory_order_release);
        std::lock_guard<std::mutex> lock(_state_mutex);
        _state_cv.notify_all();
      }
      /**
       * @brief #### 标记任务失败（由`worker`线程调用）
       */
      void mark_failed()
      {
        _end_time = std::chrono::steady_clock::now();
        _state.store(current_status::failed, std::memory_order_release);
        std::lock_guard<std::mutex> lock(_state_mutex);
        _state_cv.notify_all();
      }
    protected:
      constexpr std::string coverage_string(const std::string &str) const
      {
        if (str.empty())
        {
          return ("task_" + std::to_string(_identifier));
        }
        return str;
      }
    protected:
      std::uint64_t _identifier; // 任务唯一标识
      static std::atomic<std::uint64_t> current_unique_identifier; // 全局任务ID生成器

      std::function<void()> _ordinary_execution;  // 任务执行函数

      std::string _task_name; // 任务名称
      std::atomic<current_status> _state{current_status::pending}; // 任务状态（原子操作保证线程安全）

      std::chrono::steady_clock::time_point _deadline; // 超时时间点
      std::chrono::steady_clock::time_point _end_time; // 结束执行时间
      std::chrono::steady_clock::time_point _start_time; // 开始执行时间
      std::chrono::steady_clock::time_point _submit_time; // 提交时间

      mutable std::mutex _state_mutex; // 状态变更互斥锁
      mutable std::condition_variable _state_cv; // 状态变更条件变量
      std::atomic<bool> _has_deadline{false}; // 是否设置了超时时间

      std::atomic<std::int32_t> _priority; // 任务优先级
    public:
      template <typename execute_function>
      uint_ordinary(execute_function&& function,const std::string &name = "", 
        urgency_level priority = urgency_level::normal)
      :_identifier(current_unique_identifier.fetch_add(1, std::memory_order_relaxed)),
      _ordinary_execution(std::forward<execute_function>(function)),
      _task_name(coverage_string(name)),_submit_time(std::chrono::steady_clock::now()),
      _priority(static_cast<std::int32_t>(priority)) {}
      virtual ~uint_ordinary() = default;

      uint_ordinary(const uint_ordinary&) = delete;
      uint_ordinary& operator=(const uint_ordinary&) = delete;

      uint_ordinary(uint_ordinary&&) = delete;
      uint_ordinary& operator=(uint_ordinary&&) = delete;
      /**
       * @brief #### 执行任务 - 虚函数，子类根据自身情况实现
       * @return 任务执行结果(`derivation`类型支持任意返回类型)
       * @throws `anomaly` 任务执行异常
       *
       * 调用者：`worker`线程, 被调用者：衍生任务类
       */
      virtual derivation execute()
      {
        if(mark_running() == false)
        {
          throw anomaly("任务不在运行状态,检查任务",get_identifier());
        }
        try
        {
          _ordinary_execution();
          mark_completed();
          return derivation();
        }
        catch (const std::exception& error)
        {
          mark_failed();
          throw anomaly(std::string(error.what()), get_identifier());
        }
        catch(...)
        {
          this->mark_failed();
          throw anomaly("任务执行失败: 未知错误", get_identifier());
        }
      }
      /**
       * @brief 检查任务是否有返回值
       * @return `true` 无返回值，`false` 有返回值
       */
      virtual bool is_void_task() const noexcept 
      {
        return false;
      }
      /**
       * @brief #### 取消任务
       * @return `true` 取消成功，`false` 任务已开始执行无法取消
       *
       * 调用者：`scheduler`调度器、用户代码,依赖：原子状态变更、条件变量通知
       */
      virtual bool cancel()
      {
        current_status expected = current_status::pending;
        if (_state.compare_exchange_strong(expected, current_status::cancelled, std::memory_order_acq_rel))
        {
          std::lock_guard<std::mutex> lock(_state_mutex);
          _state_cv.notify_all();
          return true;
        }
        return false;
      }
      /**
       * @brief #### 检查任务是否超时
       * @return `true` 未超时，`false` 超时
       *
       * 调用者：`scheduler`调度器,依赖：超时时间点比较
       */
      virtual bool is_timeout() const
      {
        return std::chrono::steady_clock::now() < _deadline;
      }
      /**
       * @brief #### 是否设置任务超时
       */
      bool has_deadline() const noexcept
      {
        return _has_deadline.load(std::memory_order_acquire);
      }
      /**
       * @brief #### 标记任务超时
       * @return `true` 标记成功，`false` 任务已开始执行
       *
       * 调用者：`scheduler`调度器
       */
      virtual bool mark_timeout()
      {
        current_status expected = current_status::pending;
        if (_state.compare_exchange_strong(expected, current_status::timeout, std::memory_order_acq_rel))
        {
          std::lock_guard<std::mutex> lock(_state_mutex);
          _state_cv.notify_all();
          return true;
        }
        return false;
      }
      /**
       * @brief #### 获取任务状态
       * @return 当前任务状态
       */
      current_status get_state() const noexcept
      {
        return _state.load(std::memory_order_acquire);
      }
      /**
       * @brief #### 获取任务优先级
       * @return 任务优先级值
       */
      std::int32_t get_priority() const noexcept
      {
        return _priority.load(std::memory_order_acquire);
      }

      /**
       * @brief #### 设置任务优先级
       * @param priority 新的优先级
       */
      void set_priority(urgency_level priority) noexcept
      {
        _priority.store(static_cast<std::int32_t>(priority), std::memory_order_release);
      }

      /**
       * @brief #### 设置任务优先级
       * @param priority 新的优先级值
       */
      void set_priority(std::int32_t priority) noexcept
      {
        _priority.store(priority, std::memory_order_release);
      }

      /**
       * @brief #### 获取任务ID
       * @return 任务唯一标识
       */
      virtual std::uint64_t get_identifier() const noexcept
      {
        return _identifier;
      }

      /**
       * @brief #### 获取任务名称
       * @return 任务名称
       */
      const std::string& get_task_name() const noexcept
      {
        return _task_name;
      }
      /**
       * @brief #### 获取任务超时时间点
       */
      std::chrono::steady_clock::time_point get_deadline() const
      {
        return _deadline;
      }
      /**
       * @brief #### 设置超时时间
       * @param timeout 超时时长
       */
      template<typename rep, typename period>
      void set_timeout(const std::chrono::duration<rep, period>& timeout)
      {
        _deadline = std::chrono::steady_clock::now() + timeout;
        _has_deadline.store(true, std::memory_order_release);
      }

      /**
       * @brief #### 设置绝对超时时间点
       * @param deadline 超时时间点
       */
      void set_deadline(const std::chrono::steady_clock::time_point& deadline)
      {
        _deadline = deadline;
        _has_deadline.store(true, std::memory_order_release);
      }

      /**
       * @brief #### 获取任务提交时间
       * @return 提交时间点
       */
      std::chrono::steady_clock::time_point get_submit_time() const noexcept
      {
        return _submit_time;
      }

      /**
       * @brief #### 获取任务执行时长
       * @return 执行时长（毫秒），如果任务未完成返回0
       */
      std::chrono::milliseconds get_execution_duration() const
      {
        if (_start_time == std::chrono::steady_clock::time_point{} ||_end_time == std::chrono::steady_clock::time_point{})
          return std::chrono::milliseconds{0};
        return std::chrono::duration_cast<std::chrono::milliseconds>(_end_time - _start_time);
      }
      /**
       * @brief #### 获取任务等待时长
       * @return 等待时长（毫秒），从提交到开始执行
       */
      std::chrono::milliseconds get_wait_duration() const
      {
        if (_start_time == std::chrono::steady_clock::time_point{})
          return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _submit_time);
        return std::chrono::duration_cast<std::chrono::milliseconds>(_start_time - _submit_time);
      }
      /**
       * @brief #### 等待任务完成(超时)
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
          return state == current_status::completed || state == current_status::cancelled || state == current_status::timeout ||
          state == current_status::failed;
        };
        return _state_cv.wait_for(lock, timeout, await_func);
      }
      /**
       * @brief #### 等待任务完成（无超时）
       */
      virtual void wait() const
      {
        std::unique_lock<std::mutex> lock(_state_mutex);
        auto await_func = [this]()
        {
          auto state = _state.load(std::memory_order_acquire);
          return state == current_status::completed || state == current_status::cancelled || state == current_status::timeout ||
          state == current_status::failed;
        };
        _state_cv.wait(lock, await_func);
      }
      /**
       * @brief #### 获取任务执行结果（阻塞等待）
       * @return 任务执行结果
       * @throws 任务执行过程中的异常
       */
      virtual derivation get_result()
      {
        return derivation{};
      }
      /**
       * @brief #### 检查结果是否就绪
       * @return `true` 结果就绪，`false` 结果未就绪
       */
      virtual bool is_result_ready() const noexcept
      {
        return false;
      }

      constexpr bool operator<(const uint_ordinary& other) const noexcept
      {
        return this->get_priority() < other.get_priority();
      }
      constexpr bool operator>(const uint_ordinary& other) const noexcept
      {
        return this->get_priority() > other.get_priority();
      }
    };
    std::atomic<std::uint64_t> uint_ordinary::current_unique_identifier{1};

    /**
     * @class uint_standard
     * @brief #### 标准任务类 - 支持异步结果获取
     * @tparam execute_function 任务类型模板
     *
     * 适用场景：需要获取执行结果的任务,计算密集型任务,数据处理和转换
     * 
     * 调用关系：继承自`uint_ordinary`,使用`std::promise`和`std::future`实现结果同步
     * 由`thread_pool::submit()`创建,由`worker`线程执行
     */
    template<typename execute_function, typename result = std::invoke_result_t<execute_function>>
    class uint_standard : public uint_ordinary
    {
      //对于和基类成员变量名相同的成员变量，在多态里默认隐藏，可以通过限定符来访问
    protected:
      std::promise<result> _promise; // 结果承诺
      std::future<result> _future; // 结果期望

      std::atomic<bool> _ready_state{false};  // 结果是否就绪
      std::function<void()> _ordinary_execution;  // 任务执行函数
    public:
      uint_standard(execute_function&& function, const std::string &name = "", 
        urgency_level priority = urgency_level::normal)
      :uint_ordinary(std::forward<execute_function>(function),name,priority),
      _promise(), _future(_promise.get_future()),
       _ordinary_execution(std::forward<execute_function>(function)) {}
      
      derivation execute() override
      {
        if(!this->mark_running())
        {
          _promise.set_exception(std::make_exception_ptr(anomaly("任务无法启动",get_identifier())));
          _ready_state.store(true, std::memory_order_release);
          throw anomaly("任务无法启动", get_identifier());
        }
        try
        {
          if constexpr (std::is_void_v<result>)
          {
            _ordinary_execution();
            _promise.set_value();
            _ready_state.store(true, std::memory_order_release);
            this->mark_completed();
            return derivation();
          }
          else
          {
            auto result_value = _ordinary_execution();
            _promise.set_value(result_value);
            _ready_state.store(true, std::memory_order_release);
            this->mark_completed();
            return derivation(result_value);
          }
        }
        catch (const std::exception& e)
        {
          this->mark_failed();
          _promise.set_exception(std::current_exception());
          _ready_state.store(true, std::memory_order_release);
          throw anomaly("任务执行失败: " + std::string(e.what()), get_identifier());
        }
        catch (...)
        {
          _promise.set_exception(std::current_exception());
          _ready_state.store(true, std::memory_order_release);
          this->mark_failed();
          throw anomaly("任务执行失败: 未知错误", get_identifier());
        }
      }
      bool is_void_task() const noexcept override
      {
        return std::is_void_v<result>;
      }
      bool is_result_ready() const noexcept override
      {
        return _ready_state.load(std::memory_order_acquire);
      }
      /**
       * @brief #### 获取`future`对象
       * @return 关联的`future`对象
       * @note 结果只能获取一次，重复获取会抛出异常,后续结果通过工厂函数直接获取
       */
      std::future<result> get_future()
      {
        return _future;
      }
      const std::future<result>& get_future() const
      {
        return _future;
      }
    };

    /**
     * @class uint_overtime
     * @brief #### 超时任务类 - 支持超时检查和处理
     *
     * 适用场景：有时间限制的任务, 网络请求和`IO`操作, 需要及时响应的任务
     *
     * 调用关系： 继承自`uint_ordinary`, 由`scheduler`定期检查超时, 支持超时回调处理
     */
    template<typename execute_function, typename timeout_function>
    class uint_overtime : public uint_ordinary
    {
    protected:
      std::atomic<bool> _timeout_handled{false}; // 超时是否已处理
      std::function<std::invoke_result_t<timeout_function>> _timeout_callback; // 超时回调函数
    public:
      template<typename func, typename rep, typename period>
      uint_overtime(func&& function, const std::chrono::duration<rep, period>& timeout,
       timeout_function&& timeout_callback, const std::string &name = "")
      :uint_ordinary(std::forward<func>(function),name),
       _timeout_callback(std::forward<timeout_function>(timeout_callback))
      {  this->set_timeout(timeout);  }
      
      /**
       * @brief #### 标记任务超时（重写`uint_ordinary`类方法）
       * @return `true` 标记成功，`false` 任务已开始执行
       */
      bool mark_timeout() override
      {
        if(uint_ordinary::mark_timeout())
        {
          handle_timeout();
          return true;
        }
        return false;
      }
      /**
       * @brief #### 处理超时事件
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
            catch (...) {}
          }
        }
      }
      /**
       * @brief #### 设置超时回调函数
       * @param callback 超时回调函数
       */
      void set_timeout_callback(timeout_function&& callback)
      {
        _timeout_callback = std::forward<timeout_function>(callback);
      }
      /**
       * @brief #### 检查超时是否已处理
       * @return `true` 已处理，`false` 未处理
       */
      bool is_timeout_handled() const noexcept      
      {
        return _timeout_handled.load(std::memory_order_acquire);
      }
    };
    // template<typename execute_function>
    // class uint_reliance : public uint_ordinary
    // {
    // protected:
    //   // std::vector<std::shared_ptr<uint_ordinary<>>> _dependencies; // 依赖任务列表
    // };
    // //分离基类和带返回值派生类实现，删除基类模板
  }
}