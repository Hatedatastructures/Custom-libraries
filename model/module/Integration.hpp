#pragma once
#include <chrono>
#include <string>
/**
 * @brief #### 时间转换工具类
 */
class convert_time
{
public:
  /**
   * @brief #### 将任意时间单位转换为毫秒
   */
  template <typename rep, typename period>
  static std::chrono::milliseconds to_milliseconds(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为秒
   */
  template <typename rep, typename period>
  static std::chrono::seconds to_seconds(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::seconds>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为分钟
   */
  template <typename rep, typename period>
  static std::chrono::minutes to_minutes(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::minutes>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为小时
   */
  template <typename rep, typename period>
  static std::chrono::hours to_hours(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::hours>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为天
   */
  template <typename rep, typename period>
  static std::chrono::days to_days(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::days>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为周
   */
  template <typename rep, typename period>
  static std::chrono::weeks to_weeks(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::weeks>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为月
   */
  template <typename rep, typename period>
  static std::chrono::months to_months(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::months>(duration);
  }
  /**
   * @brief #### 将任意时间单位转换为年
   */
  template <typename rep, typename period>
  static std::chrono::years to_years(const std::chrono::duration<rep, period>& duration)
  {
    return std::chrono::duration_cast<std::chrono::years>(duration);
  }
  /**
   * @brief #### 时间点格式化为 UTC 字符串
   */
  static std::string to_utc_string(const std::chrono::system_clock::time_point& tp,
  std::string_view fmt = "%Y-%m-%dT%H:%M:%SZ") noexcept
  {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{}; 
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::array<char, 128> buf{};
    std::strftime(buf.data(), buf.size(), fmt.data(), &tm);
    return std::string(buf.data());
  }
  /**
   * @brief #### 时间点格式化为本地时间字符串
   */
  static std::string to_local_string(const std::chrono::system_clock::time_point& tp,
  std::string_view fmt = "%Y-%m-%d %H:%M:%S") noexcept
  {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::array<char, 128> buf{};
    std::strftime(buf.data(), buf.size(), fmt.data(), &tm);
    return std::string(buf.data());
  }
};

// #### 任务当前状态枚举
enum class current_status : std::uint8_t
{
  pending = 0,     // 等待执行
  running = 1,     // 正在执行
  completed = 2,   // 执行完成
  cancelled = 3,   // 已取消
  timeout = 4,     // 执行超时
  failed = 5       // 执行失败
};

// #### 任务优先级枚举
enum class weight : std::int32_t
{
  lowest = -100, // 最低优先级
  low = -50,     // 低优先级
  normal = 0,    // 普通优先级
  high = 50,     // 高优先级
  highest = 100, // 最高优先级
  critical = 200 // 关键优先级
};

// #### 队列满时的处理策略枚举
enum class backpressure : std::uint8_t
{ 
  block,     // 阻塞
  drop,      // 丢弃
  overwrite, // 覆盖
  exception  // 抛出
}; 
// #### 任务调度策略枚举
enum class rank_strategy
{
  fifo, // 先进先出
  priority, // 优先级
  delay, // 延迟
  round_robin, // 轮询
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

inline std::string to_string(weight level) noexcept
{
  switch (level)
  {
    case weight::lowest:      return "lowest";
    case weight::low:         return "low";
    case weight::normal:      return "normal";
    case weight::high:        return "high";
    case weight::highest:     return "highest";
    case weight::critical:    return "critical";
    default:                         return std::to_string(static_cast<int>(level));
  }
}

/**
 * @class execution_exception
 * @brief #### 执行期间异常类
 */
class execution_exception : public std::exception
{
private:
  std::string _message; // 异常消息
  std::uint64_t _identifier; // 任务ID

public:
  /**
   * @brief 构造函数
   * @param message 异常消息
   * @param task_id 任务`ID`，默认为`0`
   */
  explicit execution_exception(std::string message, std::uint64_t task_id = 0) noexcept
    : _message(std::move(message)), _identifier(task_id)  {}
  
  execution_exception(const execution_exception& other) noexcept
    : _message(other._message), _identifier(other._identifier) {}

  execution_exception(execution_exception&& other) noexcept
    : _message(std::move(other._message)), _identifier(other._identifier) {}
  
  execution_exception& operator=(const execution_exception& other) noexcept
  {
    if (this != &other)
    {
      _message = other._message;
      _identifier = other._identifier;
    }
    return *this;
  }
  
  execution_exception& operator=(execution_exception&& other) noexcept
  {
    if (this != &other)
    {
      _message = std::move(other._message);
      _identifier = other._identifier;
    }
    return *this;
  }
  
  ~execution_exception() override = default;
  
  const char* what() const noexcept override
  {
    return _message.c_str();
  }
  
  std::uint64_t get_identifier() const noexcept
  {
    return _identifier;
  }
  
  void set_identifier(std::uint64_t id) noexcept
  {
    _identifier = id;
  }
  
  void swap(execution_exception& other) noexcept
  {
    using std::swap;
    swap(_message, other._message);
    swap(_identifier, other._identifier);
  }
};

/**
 * @brief #### 操作期间异常类
 */
class operation_exception : public std::exception
{
private:
  std::string _message; // 异常消息
  std::chrono::system_clock::time_point _time; // 异常时间点

public:
  operation_exception(std::string message) noexcept
    : _message(std::move(message)), _time(std::chrono::system_clock::now()) {}

  operation_exception(const operation_exception& other) noexcept
    : _message(other._message), _time(other._time) {}

  operation_exception(operation_exception&& other) noexcept
    : _message(std::move(other._message)), _time(other._time) {}

  operation_exception& operator=(const operation_exception& other) noexcept
  {
    if (this != &other)
    {
      _message = other._message;
      _time = other._time;
    }
    return *this;
  }

  operation_exception& operator=(operation_exception&& other) noexcept
  {
    if (this != &other)
    {
      _message = std::move(other._message);
      _time = other._time;
    }
    return *this;
  }

  ~operation_exception() override = default;

  const char* what() const noexcept override
  {
    return _message.c_str();
  }

  std::chrono::system_clock::time_point get_time() const noexcept
  {
    return _time;
  }

  void set_time(std::chrono::system_clock::time_point time) noexcept
  {
    _time = time;
  }

  std::string get_time_string(std::string_view fmt = "%Y-%m-%d %H:%M:%S") const noexcept
  {
    return convert_time::to_local_string(_time, fmt);
  }
};

/**
 * @class derivation
 * @brief 任务返回类型封装类，支持任意类型的返回值，包括 `void` 类型
 * @warning 不支持对 `void` 类型的返回值进行转换
 */
class derivation
{ 
private:

  std::any _data;
  bool _void;

public:
  derivation() : _void(true) {}

  template<typename convert_t>
  derivation(convert_t&& value) 
  : _data(std::forward<convert_t>(value)), _void(false) {}

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

  derivation(const derivation& ) = delete;

  derivation& operator= (const derivation&) = delete;

  template<typename implicit_type>
  operator implicit_type() const
  {
    static_assert(!std::is_void_v<implicit_type>, "Cannot convert to void type");
    if(_void)
    {
      throw execution_exception("The void type cannot be converted.",0);
    }
    try
    {
      return std::any_cast<implicit_type>(_data);
    }
    catch(const std::bad_any_cast& conversion_e)
    {
      throw std::runtime_error(std::string("Type conversion failed: ") + conversion_e.what());
    }
  }

  // #### 检查是否为 void 类型
  bool is_void() const noexcept
  {
    return _void;
  }

  // 检查是否有值
  bool has_value() const noexcept
  {
    return !_void && _data.has_value();
  }

  // #### 显式获取任务返回值
  template<typename convert_t>
  auto get() const
  {
    if constexpr (std::is_void_v<convert_t>)
    {
      if(!_void)
      {
        throw execution_exception("The task has a return value and cannot be obtained as void", 0);
      }
      return;
    } 
    else
    {
      if(_void)
      {
        throw execution_exception("Cannot get value from void derivation", 0);
      }
      try
      {
        return std::any_cast<convert_t>(_data);
      }
      catch(const std::bad_any_cast& conversion_e)
      {
        throw std::runtime_error(std::string("Type conversion failed: ") + conversion_e.what());
      }    
    }
  }

  // 获取存储的类型信息
  const std::type_info& type() const noexcept
  {
    return _void ? typeid(void) : _data.type();
  }

  // 就地构造新值
  template<typename convert_t, typename... Args>
  void emplace(Args&&... args)
  { // void 类型不允许存储值
    static_assert(!std::is_void_v<convert_t>, "Cannot emplace void type");
    _data.emplace<convert_t>(std::forward<Args>(args)...);
    _void = false;
  }
};