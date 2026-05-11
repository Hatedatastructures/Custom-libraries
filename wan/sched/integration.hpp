/**
  * @file integration.hpp
  * @brief 集成工具类定义
  * @details 提供时间转换、枚举套件等功能
  */
#pragma once
#include <chrono>
#include <atomic>
#include <string>
#include <algorithm>
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
enum class worker_state
{
    idle,     // 空闲状态 
    running,  // 运行状态 
    stopping, // 停止中   
    stopped,  // 已停止   
    error     // 错误状态 
};
//  调度策略枚举
enum class scheduling_tactics
{
    round_robin,    // 轮询调度   - 平均分配任务
    least_loaded,   // 最少负载   - 分配给负载最轻的线程
    adaptive,       // 自适应调度 - 根据性能动态调整
    priority_based, // 优先级调度 - 基于任务优先级
};
//扩缩容策略枚举
enum class expansion_strategy
{
    conservative, // 保守策略 - 缓慢调整
    aggressive,   // 激进策略 - 快速调整
    reactive,     // 响应策略 - 基于当前负载
    hybrid        // 混合策略 - 结合多种策略
};

enum class pool_state
{
    stopped,  // 已停止
    starting, // 启动中
    running,  // 运行中
    pausing,  // 暂停中
    paused,   // 已暂停
    stopping, // 停止中
    error     // 错误状态
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
    std::string message_; // 异常消息
    std::uint64_t identifier_; // 任务ID

public:
    /**
      * @brief 构造函数
      * @param message 异常消息
      * @param task_id 任务`ID`，默认为`0`
      */
    explicit execution_exception(std::string message, std::uint64_t task_id = 0) noexcept
        : message_(std::move(message)), identifier_(task_id)  {}
    
    execution_exception(const execution_exception& other) noexcept
        : message_(other.message_), identifier_(other.identifier_) {}

    execution_exception(execution_exception&& other) noexcept
        : message_(std::move(other.message_)), identifier_(other.identifier_) {}
    
    execution_exception& operator=(const execution_exception& other) noexcept
    {
        if (this != &other)
        {
            message_ = other.message_;
            identifier_ = other.identifier_;
        }
        return *this;
    }
    
    execution_exception& operator=(execution_exception&& other) noexcept
    {
        if (this != &other)
        {
            message_ = std::move(other.message_);
            identifier_ = other.identifier_;
        }
        return *this;
    }
    
    ~execution_exception() override = default;
    
    const char* what() const noexcept override
    {
        return message_.c_str();
    }
    
    std::uint64_t getidentifier_() const noexcept
    {
        return identifier_;
    }
    
    void setidentifier_(std::uint64_t id) noexcept
    {
        identifier_ = id;
    }
    
    void swap(execution_exception& other) noexcept
    {
        using std::swap;
        swap(message_, other.message_);
        swap(identifier_, other.identifier_);
    }
};

/**
  * @brief #### 操作期间异常类
  */
class operation_exception : public std::exception
{
private:
    std::string message_; // 异常消息
    std::chrono::system_clock::time_point time_; // 异常时间点

public:
    operation_exception(std::string message) noexcept
        : message_(std::move(message)), time_(std::chrono::system_clock::now()) {}

    operation_exception(const operation_exception& other) noexcept
        : message_(other.message_), time_(other.time_) {}

    operation_exception(operation_exception&& other) noexcept
        : message_(std::move(other.message_)), time_(other.time_) {}

    operation_exception& operator=(const operation_exception& other) noexcept
    {
        if (this != &other)
        {
            message_ = other.message_;
            time_ = other.time_;
        }
        return *this;
    }

    operation_exception& operator=(operation_exception&& other) noexcept
    {
        if (this != &other)
        {
            message_ = std::move(other.message_);
            time_ = other.time_;
        }
        return *this;
    }

    ~operation_exception() override = default;

    const char* what() const noexcept override
    {
        return message_.c_str();
    }

    std::chrono::system_clock::time_point get_time() const noexcept
    {
        return time_;
    }

    void set_time(std::chrono::system_clock::time_point time) noexcept
    {
        time_ = time;
    }

    std::string get_time_string(std::string_view fmt = "%Y-%m-%d %H:%M:%S") const noexcept
    {
        return convert_time::to_local_string(time_, fmt);
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

    std::any data_;
    bool void_;

public:
    derivation() : void_(true) {}

    template<typename convert_t>
    derivation(convert_t&& value) 
    : data_(std::forward<convert_t>(value)), void_(false) {}

    derivation(derivation&& other) noexcept 
    : data_(std::move(other.data_)), void_(std::move(other.void_)) {}

    derivation& operator= (derivation&& other) noexcept
    {
        if(this != &other)
        {
            data_ = std::move(other.data_);
            void_ = other.void_;
        }
        return *this;
    }

    derivation(const derivation& ) = delete;

    derivation& operator= (const derivation&) = delete;

    template<typename implicit_type>
    operator implicit_type() const
    {
        static_assert(!std::is_void_v<implicit_type>, "Cannot convert to void type");
        if(void_)
        {
            throw execution_exception("The void type cannot be converted.",0);
        }
        try
        {
            return std::any_cast<implicit_type>(data_);
        }
        catch(const std::bad_any_cast& conversion_e)
        {
            throw std::runtime_error(std::string("Type conversion failed: ") + conversion_e.what());
        }
    }

    // #### 检查是否为 void 类型
    bool isvoid_() const noexcept
    {
        return void_;
    }

    // 检查是否有值
    bool has_value() const noexcept
    {
        return !void_ && data_.has_value();
    }

    // #### 显式获取任务返回值
    template<typename convert_t>
    auto get() const
    {
        if constexpr (std::is_void_v<convert_t>)
        {
            if(!void_)
            {
                throw execution_exception("The task has a return value and cannot be obtained as void", 0);
            }
            return;
        } 
        else
        {
            if(void_)
            {
                throw execution_exception("Cannot get value from void derivation", 0);
            }
            try
            {
                return std::any_cast<convert_t>(data_);
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
        return void_ ? typeid(void) : data_.type();
    }

    // 就地构造新值
    template<typename convert_t, typename... Args>
    void emplace(Args&&... args)
    { // void 类型不允许存储值
        static_assert(!std::is_void_v<convert_t>, "Cannot emplace void type");
        data_.emplace<convert_t>(std::forward<Args>(args)...);
        void_ = false;
    }
};
/**
  * @brief 工作线程统计信息
  *
  * 记录工作线程的性能统计数据，用于监控和优化
  */
class worker_statistics
{
public:
    std::atomic<std::uint64_t> tasks_failed{0};           // 执行失败任务数量
    std::atomic<std::uint64_t> tasks_executed{0};         // 已执行任务数量
    std::atomic<std::uint64_t> total_idle_time_{0};        // 总空闲时间(微秒)
    std::atomic<std::uint64_t> total_execution_time_{0};   // 总执行时间(微秒)

    std::chrono::steady_clock::time_point start_time_;     // 线程启动时间
    std::chrono::steady_clock::time_point last_task_time_; // 最后任务执行时间

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
        total_idle_time_.store(0, std::memory_order_relaxed);
        total_execution_time_.store(0, std::memory_order_relaxed);
        start_time_ = std::chrono::steady_clock::now();
        last_task_time_ = start_time_;
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

        auto total_time = total_execution_time_.load(std::memory_order_relaxed);
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
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();

        if (total_time == 0)
            return 0.0;

        auto execution_time = total_execution_time_.load(std::memory_order_relaxed);
        return static_cast<double>(execution_time) / total_time;
    }
};
class load_metrics
{
public:
    std::atomic<double> throughput{0.0};               // 吞吐量(任务/秒)
    std::atomic<double> memory_usage{0.0};             // 内存使用率
    std::atomic<double> cpu_utilization{0.0};          // CPU利用率
    std::atomic<double> average_task_time_{0.0};        // 平均任务执行时间

    std::atomic<std::size_t> queue_length{0};          // 队列长度
    std::atomic<std::size_t> active_threads{0};        // 活跃线程数
    std::atomic<std::size_t> total_threads{0};         // 总线程数
    std::atomic<std::size_t> queue_capacity{0};        // 队列容量（支持动态调整）

    std::chrono::steady_clock::time_point last_update; // 最后更新时间

    /**
      * @brief 重置指标
      */
    void reset()
    {
        throughput.store(0.0, std::memory_order_relaxed);
        memory_usage.store(0.0, std::memory_order_relaxed);
        cpu_utilization.store(0.0, std::memory_order_relaxed);
        average_task_time_.store(0.0, std::memory_order_relaxed);

        queue_length.store(0, std::memory_order_relaxed);
        active_threads.store(0, std::memory_order_relaxed);
        total_threads.store(0, std::memory_order_relaxed);
        queue_capacity.store(0, std::memory_order_relaxed);

        last_update = std::chrono::steady_clock::now();
    }

    /**
      * @brief 计算综合负载分数
      * @return 负载分数(0.0-1.0)
      */
    double calculate_load_score() const
    {
        // 使用线程利用率与队列使用率作为主要负载信号，适配动态队列容量
        auto qlen = queue_length.load(std::memory_order_relaxed);
        auto qcap = queue_capacity.load(std::memory_order_relaxed);
        auto act  = active_threads.load(std::memory_order_relaxed);
        auto tot  = total_threads.load(std::memory_order_relaxed);

        if (tot == 0) tot = 1;
        if (qcap == 0) qcap = std::max<std::size_t>(qlen, 1);

        double utilization_threads = static_cast<double>(act) / static_cast<double>(tot);
        double utilization_queue   = std::min(static_cast<double>(qlen) / static_cast<double>(qcap), 1.0);

        // 基础负载分数：线程繁忙度与队列占用度各占一半，范围 [0,1]
        double base_score = 0.5 * utilization_threads + 0.5 * utilization_queue;
        return std::clamp(base_score, 0.0, 1.0);
    }
};
class scaling_config
{
public:
    std::size_t min_threads = 1;                      // 最小线程数
    std::size_t max_threads = 32;                     // 最大线程数
    std::size_t core_threads = 4;                     // 核心线程数
    double scale_up_threshold = 0.8;                  // 扩容阈值
    double scale_down_threshold = 0.4;                // 缩容阈值

    std::size_t scale_up_step = 1;                    // 扩容步长
    std::size_t scale_down_step = 1;                  // 缩容步长
    bool enable_predictive_scaling = true;            // 启用预测性扩缩容

    std::chrono::milliseconds scale_up_delay{1000};   // 扩容延迟
    std::chrono::milliseconds scale_down_delay{5000}; // 缩容延迟
};
class pool_config
{
public:
    // 基础配置
    std::string pool_name_ = "default_pool"; // 线程池名称标识
    std::size_t min_threads_ = 1; // 最小线程数量
    std::size_t max_threads_ = 8; // 最大线程数量
    std::size_t core_threads_ = 4; // 核心线程数量
    std::size_t initial_threads_ = 4; // 初始线程数量

    // 队列配置
    std::size_t max_queue_size_ = 0; // 最大队列容量
    rank_strategy queue_policy_ = rank_strategy::fifo; // 队列调度策略

    // 调度配置
    expansion_strategy expansion_strategy_ = expansion_strategy::hybrid; // 扩缩容策略
    scheduling_tactics scheduling_tactics_ = scheduling_tactics::adaptive; // 调度策略
    
    // 超时配置
    std::chrono::milliseconds task_timeout_{300}; // 任务执行超时时间
    std::chrono::milliseconds idle_timeout_{600}; // 线程空闲超时时间
    std::chrono::milliseconds shutdown_timeout_{1000}; // 线程池关闭超时时间

    // 监控配置
    bool enable_monitoring_ = true; // 是否启用性能监控
    bool enable_performance_profiling_ = false; // 是否启用性能分析
    std::chrono::milliseconds monitoring_interval_{1000}; // 性能监控采样间隔

    // 日志配置
    std::string log_file_path_; // 日志文件存储路径
    bool enable_event_logging_ = false; // 是否启用事件日志记录

    /**
      * @brief 验证配置有效性
      * @return true 配置有效，false 配置无效
      */
    bool validate() const
    {
        return min_threads_ > 0 && max_threads_ >= min_threads_ && initial_threads_ >= min_threads_ &&
        initial_threads_ <= max_threads_ && core_threads_ >= min_threads_ && core_threads_ <= max_threads_;
    }
};
struct pool_statistics
{
    // 基础统计
    std::atomic<std::uint64_t> total_tasks_failed_{0}; // 累计失败任务数量
    std::atomic<std::uint64_t> total_tasks_timeout_{0}; // 累计超时任务数量
    std::atomic<std::uint64_t> total_tasks_cancelled_{0}; // 累计取消任务数量
    std::atomic<std::uint64_t> total_tasks_submitted_{0}; // 累计提交任务数量
    std::atomic<std::uint64_t> total_tasks_completed_{0}; // 累计完成任务数量

    // 性能统计
    std::atomic<double> current_throughput_{0.0}; // 当前任务吞吐量
    std::atomic<double> peak_throughput_{0.0}; // 历史峰值吞吐量
    
    // 吞吐量计算辅助变量
    std::atomic<std::int64_t> last_throughput_time_{0}; // 上次吞吐量计算时间戳
    std::atomic<std::uint64_t> last_completed_count_{0}; // 上次完成任务计数

    // 线程统计
    std::atomic<std::size_t> active_thread_count_{0}; // 当前活跃线程数量
    std::atomic<std::size_t> current_thread_count_{0}; // 当前总线程数量
    std::atomic<std::size_t> peak_thread_count_{0}; // 历史峰值线程数量

    // 队列统计
    std::atomic<std::size_t> current_queue_size_{0}; // 当前队列任务数量
    std::atomic<std::size_t> peak_queue_size_{0}; // 历史峰值队列大小

    // 时间统计
    std::chrono::steady_clock::time_point start_time_; // 线程池启动时间
    std::chrono::steady_clock::time_point last_task_time_; // 最后任务执行时间

    /**
      * @brief 重置统计信息
      */
    void reset()
    {
        total_tasks_failed_.store(0, std::memory_order_relaxed);
        total_tasks_timeout_.store(0, std::memory_order_relaxed);
        total_tasks_cancelled_.store(0, std::memory_order_relaxed);
        total_tasks_submitted_.store(0, std::memory_order_relaxed);
        total_tasks_completed_.store(0, std::memory_order_relaxed);

        peak_throughput_.store(0.0, std::memory_order_relaxed);
        current_throughput_.store(0.0, std::memory_order_relaxed);
        
        last_throughput_time_.store(0, std::memory_order_relaxed);
        last_completed_count_.store(0, std::memory_order_relaxed);

        peak_thread_count_.store(0, std::memory_order_relaxed);
        active_thread_count_.store(0, std::memory_order_relaxed);
        current_thread_count_.store(0, std::memory_order_relaxed);

        peak_queue_size_.store(0, std::memory_order_relaxed);
        current_queue_size_.store(0, std::memory_order_relaxed);

        last_task_time_ = start_time_;
        start_time_ = std::chrono::steady_clock::now();
    }

    /**
      * @brief 计算成功率
      * @return 成功率(0.0-1.0)
      */
    double calculate_success_rate() const
    {
        auto total = total_tasks_submitted_.load(std::memory_order_relaxed);
        if (total == 0)
            return 1.0;

        auto completed = total_tasks_completed_.load(std::memory_order_relaxed);
        return static_cast<double>(completed) / total;
    }

    /**
      * @brief 计算运行时间
      * @return 运行时间(秒)
      */
    double calculate_uptime() const
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        return duration.count();
    }
};