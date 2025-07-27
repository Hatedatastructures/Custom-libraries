#include <list>
#include <ctime>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <condition_variable>
#include <boost/circular_buffer.hpp>

#define console std::make_unique<controller::console_controller>()
#define file(file_name) std::make_unique<controller::file_controller>(file_name)
#define file_mode(file,mode) std::make_unique<controller::file_controller>(file,mode)


using custom_string = std::string;
namespace instrument
{
  enum class situation_level
  {
    info,
    warning,
    error,
    fatal
  };
  enum class open_mode
  {
    append,
    overwrite
  };
  class chronix
  { 
  private:
    std::atomic<uint64_t> microseconds_value;
    static std::tm localtime_thread_safe(std::time_t t) 
    {
      std::tm struct_tm;
  #ifndef _WIN32
      localtime_r(&t, &struct_tm); 
  #else
      localtime_s(&struct_tm, &t);  
  #endif
      return struct_tm;
    }
  public:
    chronix()
    {
      auto nowadays = std::chrono::high_resolution_clock::now();
      auto nowadays_epoch = nowadays.time_since_epoch();
      uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(nowadays_epoch).count();
      microseconds_value.store(us, std::memory_order_relaxed); 
    }
    explicit chronix(const std::chrono::high_resolution_clock::time_point& tp) 
    {
      auto epoch = tp.time_since_epoch();
      uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
      microseconds_value.store(us, std::memory_order_relaxed);
    }
    explicit chronix(const uint64_t us) 
    : microseconds_value(us) {}
    uint64_t get_microseconds() const
    {
      return microseconds_value.load(std::memory_order_relaxed);
    }
    uint64_t to_seconds() const 
    {
      return get_microseconds() / 1000000;
    }
    uint64_t to_milliseconds() const 
    {
      return get_microseconds() / 1000;
    }
    custom_string to_string() const 
    {
      uint64_t us = microseconds_value.load(std::memory_order_relaxed);
      std::time_t t = static_cast<std::time_t>(us / 1000000);
      // 生成年月日时分秒
      std::tm tm = localtime_thread_safe(t);
      char buf[32];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
      // 补充微秒部分（秒的小数部分）
      uint64_t tail_us = us % 1000000; // 取微秒部分（0-999999）
      return custom_string(buf) + "." + std::to_string(tail_us);
    }
    chronix(const chronix& other) 
    : microseconds_value(other.microseconds_value.load(std::memory_order_relaxed)) {}

    chronix& operator=(const chronix& other) 
    {
      if (this != &other) 
      {
        microseconds_value.store(other.microseconds_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
      }
      return *this;
    }

    chronix operator-(const chronix& other) const 
    {
      uint64_t this_us = microseconds_value.load(std::memory_order_relaxed);
      uint64_t other_us = other.microseconds_value.load(std::memory_order_relaxed);
      return chronix(this_us - other_us);
    }
    friend std::ostream& operator<<(std::ostream& time_os, const chronix& tp);
  };
  std::ostream& operator<<(std::ostream& time_os, const chronix& tp)
  {
    return time_os << tp.to_string();
  }
  static std::unordered_map<situation_level, std::string> level_string = 
  {
    {situation_level::info, "INFO"},
    {situation_level::warning, "WARNING"},
    {situation_level::error, "ERROR"},
    {situation_level::fatal, "FATAL"}
  };
  struct synthesis
  {
    instrument::chronix time;
    situation_level level;
    custom_string message;
    synthesis(instrument::chronix time, situation_level level, custom_string message)
    : time(time), level(level), message(message) {}
    synthesis()
    {time = instrument::chronix();level = situation_level::info;message = "";}
    synthesis& operator=(const synthesis& other)
    {
      time = other.time;level = other.level;message = other.message;
      return *this;
    }
    synthesis(const synthesis& other)
    : time(other.time), level(other.level), message(other.message) {}
    synthesis(synthesis&& other) noexcept
    : time(std::move(other.time)), level(std::move(other.level)), 
    message(std::move(other.message)) {}
    synthesis& operator=(synthesis&& other) noexcept
    {
      time = std::move(other.time);level = std::move(other.level);
      message = std::move(other.message);return *this;
    }
    custom_string to_string() const
    {
      return time.to_string() +  custom_string(" [") + level_string[level] + custom_string("] ") + message;
    }
  };
}
using callback_function = std::function<void(const custom_string )>;
namespace cushioning
{
  static constexpr size_t CACHE_LINE = 64; // 使用预定义的值

  class underlying_cache 
  {
  private:
    std::mutex produce_mutex,consume_mutex;                                                     // 生产消费锁
    size_t single_container_capacity;                                                           // 单个容器容量
    std::thread background_consumption;                                                         // 后台输出线程
    static constexpr size_t default_capacity = 10;                                              // 默认容量
    std::condition_variable conditional_variables;                                              // 条件变量
    alignas(CACHE_LINE) std::atomic<bool> running_identifier,consume_identifier;                // 运行消费标识
    alignas(CACHE_LINE) boost::circular_buffer<custom_string> primary,secondary;                // 队列
    alignas(CACHE_LINE) std::atomic<boost::circular_buffer<custom_string>*> produce,consume;    // 生产消费
    std::unordered_map<custom_string,callback_function> function_map;                           // 回调函数映射表
    void container_exchange()
    {
      boost::circular_buffer<custom_string> *tmp = produce.load(std::memory_order_acquire);
      produce.store(consume.load(std::memory_order_acquire),std::memory_order_release);
      consume.store(tmp,std::memory_order_release);
    }
    void consume_value()
    {
      for (auto &value : *consume.load())
      {
        for (auto &function_value : function_map)
        {
          function_value.second(value);
        }
      }
      consume.load()->clear();
      consume_identifier = true;
      conditional_variables.notify_one();
    }
    void background_functions()
    {
      std::unique_lock<std::mutex> lock(consume_mutex);
      while (running_identifier)
      {
        auto tmp_func = [&](){ return !running_identifier || !consume.load()->empty(); };
        conditional_variables.wait(lock, tmp_func);
        if (running_identifier == false)
        {
          break;
        }
        consume_value();
      }
      if(!consume.load()->empty())
      {
        consume_value();
      }
    }
  public:
    underlying_cache(const size_t &container_capacity = default_capacity)
    :single_container_capacity(container_capacity),running_identifier(true),consume_identifier(true),
    produce(&primary),consume(&secondary)
    {
      primary.set_capacity(container_capacity);
      secondary.set_capacity(container_capacity);
      background_consumption = std::thread(&underlying_cache::background_functions, this);
    }
    void push(custom_string &&string_value)
    {
      std::unique_lock<std::mutex>  produce_lock(produce_mutex);
      if(produce.load()->full())
      {
        std::unique_lock<std::mutex> consume_lock(consume_mutex);
        conditional_variables.wait(consume_lock,[&]{return consume_identifier.load();});
        consume_identifier = false;
        container_exchange();
        conditional_variables.notify_one();
      }
      produce.load()->push_back(string_value);
    }
    void push(const custom_string &string_value) = delete;
    void push_batch(std::vector<custom_string>&& vector_string_value)
    {
      std::unique_lock<std::mutex> produce_lock(produce_mutex);
    for (const auto& string_value : vector_string_value) 
    {
        if (produce.load()->full()) 
        {
          std::unique_lock<std::mutex> consume_lock(consume_mutex);
          conditional_variables.wait(consume_lock, [&]() { return consume_identifier.load(); });
          consume_identifier = false;
          container_exchange();
          conditional_variables.notify_one();
        }
        produce.load()->push_back(std::move(string_value));
      }
    }
    void flush()
    {
      std::lock_guard<std::mutex> lock(produce_mutex);
      // 如果生产缓冲区非空，交换并通知消费
      if (!produce.load()->empty())
      {
        {
          std::lock_guard<std::mutex> consume_lock(consume_mutex);
          container_exchange();
        }
        conditional_variables.notify_one();
        // 等待当前数据处理完成
        std::unique_lock<std::mutex> consume_lock(consume_mutex);
        conditional_variables.wait(consume_lock, [&]() { return consume_identifier.load();});
      }
    }
    double usage_rate()
    {
      std::lock_guard<std::mutex> lock(produce_mutex);
      size_t using_size = produce.load()->size() + consume.load()->size();
      return static_cast<float>(using_size) / (2 * single_container_capacity);
    }
    bool adjust_capacity(const size_t &new_container_capacity)
    { // 调整双队列大小
      if (new_container_capacity > produce.load()->size() && new_container_capacity > consume.load()->size())
      {
        single_container_capacity = new_container_capacity;
        primary.set_capacity(new_container_capacity);
        secondary.set_capacity(new_container_capacity);
        return true;
      }
      return false;
    }
    inline void insert_callback(const custom_string &controller_id, const callback_function &function_value)
    {
      function_map[controller_id] = function_value;
    }
    inline void remove_callback(const custom_string &controller_id)
    {
      function_map.erase(controller_id);
    }
    inline bool lookup_callback(const custom_string &controller_id)const
    {
      return function_map.find(controller_id) != function_map.end();
    }
    ~underlying_cache()
    {
      flush();
      running_identifier = false;
      {
        conditional_variables.notify_one(); // 唤醒线程
      }
      if(background_consumption.joinable())
      {
        background_consumption.join();
      }
    }
  };
}
namespace controller
{
  class abstract_controller
  {
  public:
    virtual custom_string identifier() const = 0;
    virtual void write(const custom_string string_value) = 0;
    virtual void flush() = 0;
    virtual ~abstract_controller() = default;
  };
  class file_controller : public abstract_controller
  {
  private:
    std::ofstream file_stream;
    instrument::open_mode mode;
    custom_string file_name;
    std::mutex file_mutex;
    std::ios::openmode mode_to_flag(instrument::open_mode tmp_mode)
    {
      switch (tmp_mode)
      {
      case instrument::open_mode::overwrite:
        return std::ios::out;
      case instrument::open_mode::append:
        return std::ios::app;
      default:
        return std::ios::out;
      }
    }
    static custom_string mode_to_string(instrument::open_mode tmp_mode) 
    {
      switch (tmp_mode) 
      {
        case instrument::open_mode::append :
          return "append";
        case instrument::open_mode::overwrite :
          return "overwrite";
        default:
          return "unknown";
      }
      return "unknown";
    }
    void check_stream_error(const custom_string& action) const 
    {
      if (file_stream.fail()) 
      {
        throw std::runtime_error(action + ":" + file_name);
      }
    }
  public:
    virtual custom_string identifier() const override
    {
      return custom_string("file");
    }
    file_controller(const custom_string &tmp_file_name,const instrument::open_mode& tmp_mode = instrument::open_mode::overwrite)
    :mode(tmp_mode),file_name(tmp_file_name)
    {
      std::ios::openmode flag = mode_to_flag(tmp_mode);
      file_stream.open(tmp_file_name,flag);
      if (!file_stream.is_open()) 
      {
        throw std::runtime_error("无法打开文件: " + file_name + "(模式：" + mode_to_string(tmp_mode) + ")");
      }
    }
    file_controller(const custom_string& tmp_file_name, std::ios::openmode custom_flags)
    : file_name(tmp_file_name) 
    {
      file_stream.open(tmp_file_name, custom_flags);
      if (!file_stream.is_open()) 
      {
        throw std::runtime_error("无法打开文件: " + file_name + "（自定义模式）");
      }
    }
    virtual void flush() override
    {
      if (file_stream.is_open()) 
      {
        std::lock_guard<std::mutex> lock(file_mutex);
        file_stream.flush();
        check_stream_error("刷新失败");
      }
    }
    virtual void write(const custom_string string_value) override
    {
      file_stream << string_value ;
      file_stream.put('\n');
    }
    ~file_controller()
    {
      
      if (file_stream.is_open()) 
      {
        file_stream.flush();
        file_stream.close();
      }
    }
    file_controller(const file_controller&) = delete;
    file_controller& operator=(const file_controller&) = delete;
    file_controller(file_controller&&) = default;
    file_controller& operator=(file_controller&&) = default;
  };
  class console_controller : public abstract_controller
  {
  private:
    std::ostream &stream;
  public:
    virtual custom_string identifier() const override
    {
      return custom_string("console");
    }
    console_controller()
    : stream(std::cout) {}
    virtual void write(const custom_string string_value) override
    {
      stream << string_value ;
      stream.put('\n');
    }
    virtual void flush() override
    {
      stream.flush();
    }
    ~console_controller()
    {
      stream.flush();
    }
  };
}
namespace resource_manager
{
  class workflow_coordinator
  {
  private:
    std::unordered_map<custom_string, std::unique_ptr<controller::abstract_controller>> stream_map; 
    cushioning::underlying_cache cushioning_object;

  public:
    workflow_coordinator() = default;
    workflow_coordinator(const size_t& capacity)
    : cushioning_object(capacity){}
    workflow_coordinator(const workflow_coordinator&) = delete;
    workflow_coordinator& operator=(const workflow_coordinator&) = delete;
    workflow_coordinator(workflow_coordinator&&) = default;
    workflow_coordinator& operator=(workflow_coordinator&&) = default;
    double usage_rate()
    {
      return cushioning_object.usage_rate();
    }
    void push_batch(std::vector<custom_string>&& vector_string_value)
    {
      cushioning_object.push_batch(std::forward<std::vector<custom_string>>(vector_string_value));
    }
    bool insert_controller(std::unique_ptr<controller::abstract_controller>&& smart_pointer_value)
    {
      if (!smart_pointer_value) return false; 
      const custom_string controller_id = smart_pointer_value->identifier();
      stream_map.emplace(controller_id, std::move(smart_pointer_value));
      auto function_value = [this, controller_id](const custom_string string_value)
      {
        auto it = stream_map.find(controller_id);
        if (it != stream_map.end()) 
        { 
          it->second->write(string_value);
        }
      };
      cushioning_object.insert_callback(controller_id, function_value);
      return lookup_controller(controller_id);
    }
    bool insert_controller(std::unique_ptr<controller::abstract_controller>&& smart_pointer_value,
    std::unique_ptr<controller::abstract_controller>&& smart_pointer_value_two) 
    {
      bool all_ok = insert_controller(std::move(smart_pointer_value));
      all_ok = all_ok && insert_controller(std::move(smart_pointer_value_two));
      return all_ok; 
    }
    bool remove_controller(std::unique_ptr<controller::abstract_controller>&& smart_pointer_value)
    {
      custom_string controller_id = smart_pointer_value->identifier();
      stream_map.erase(controller_id);
      cushioning_object.remove_callback(controller_id);
      if(!cushioning_object.lookup_callback(controller_id) && !stream_map.contains(controller_id))
      {
        return true;
      }
      return false;
    }
    bool lookup_controller(const custom_string &controller_id)const
    {
      if(cushioning_object.lookup_callback(controller_id) && stream_map.contains(controller_id))
      {
        return true;
      }
      return false;
    }
    void push(custom_string &&string_value)
    {
      cushioning_object.push(std::move(string_value));
    }
    void flush()
    {
      cushioning_object.flush();
      for(auto& smart_pointer_value : stream_map)
      {
        smart_pointer_value.second->flush();
      }
    }
  };
  class staging_area
  {
  private:
    std::list<std::vector<custom_string>> primary_staging_area; //主缓冲区
    std::vector<custom_string> secondary_staging_area; //次缓冲区
    std::atomic<size_t> staging_area_threshold; //缓冲区阈值
    void clear()
    {
      primary_staging_area.clear();
      secondary_staging_area.clear();
    }
  public:
    staging_area()
    : staging_area_threshold(1000){}
    staging_area(const size_t& staging_area_threshold)
    : staging_area_threshold(staging_area_threshold){}
    void push(custom_string && string_value)
    {
      secondary_staging_area.push_back(std::move(string_value));
      if(secondary_staging_area.size() >= staging_area_threshold)
      {
        primary_staging_area.push_back(std::move(secondary_staging_area));
        secondary_staging_area.clear();
      }
    }
    void push_batch(std::vector<custom_string>&& vector_string_value)
    {
      if(secondary_staging_area.empty())
      {
        primary_staging_area.push_back(std::move(vector_string_value));
      }
      {
        secondary_staging_area.insert(secondary_staging_area.end(), vector_string_value.begin(), vector_string_value.end());
        if(secondary_staging_area.size() >= staging_area_threshold)
        {
          primary_staging_area.push_back(std::move(secondary_staging_area));
          secondary_staging_area.clear();
        }
      }
    }
    void flush()
    {
      primary_staging_area.push_back(std::move(secondary_staging_area));
      secondary_staging_area.clear();
    }
    std::vector<custom_string> recycling_resources()
    {
      flush();
      std::vector<custom_string> result;
      for(auto& vector_string_value : primary_staging_area)
      {
        result.insert(result.end(), vector_string_value.begin(), vector_string_value.end());
      }
      primary_staging_area.clear();
      return result;
    }
    ~staging_area()
    {
      flush();
      clear();
    }
  };
}
namespace recorders
{
  /*
  * #### 日志中控
  *
  * - 支持多种输出流(可自定义)

  * - 支持日志过滤功能

  * - 支持日志暂存
  */
  class recorder
  {
  private:
    resource_manager::workflow_coordinator processor;
    resource_manager::staging_area staging_area;
    std::vector<bool> situation_object;
    void filtration(instrument::situation_level level)
    {
      size_t idx = static_cast<size_t>(level);
      if (idx < situation_object.size())
      {
        situation_object[idx] = true;
      }
    }
  public:
    recorder():situation_object(4,false){}
    recorder(const size_t& processor_capacity):processor(processor_capacity),situation_object(4, false){}
    recorder(const size_t& processor_capacity,const size_t& secondary_staging_capacity)
    :processor(processor_capacity),staging_area(secondary_staging_capacity),situation_object(4, false){}
    /*
    * #### 向已添加的输出流输出数据 
    *
    *  支持的参数类型：
    *    - `synthesis`
    * 
    *    - `custom_string`
    * 
    *    - `std::vector<custom_string>`
    * 
    *    - `std::vector<synthesis>`
    */ 
    void log(const instrument::synthesis& message_set)
    {
      if(situation_object[static_cast<int>(message_set.level)] == false)
      {
        processor.push(message_set.time.to_string() + "  " + message_set.message);
      }
    }
    void log(custom_string&& message)
    {processor.push(std::move(message));}
    void log(std::vector<custom_string>&& vector_message)
    {processor.push_batch(std::move(vector_message));}
    void log(std::vector<instrument::synthesis>&& synthesis_message)
    {
      for(auto& message_set : synthesis_message)
      {
        log(message_set);
      }
    }
    void log_staging_area(custom_string&& message)
    {staging_area.push(std::move(message));}
    void log_staging_area(std::vector<custom_string>&& message)
    {staging_area.push_batch(std::move(message));}
    void purge_staging_area()
    {log(staging_area.recycling_resources());}
    void flush_staging_area(){staging_area.flush();}
    void log(const custom_string& message) = delete;
    template<typename... Args>
    void filtration(instrument::situation_level first, Args&&... second)
    {
      filtration(first); 
      filtration(std::forward<Args>(second)...); 
    }
    template<typename... Args>
    void install_controller(Args&&... args)
    {
      if(!processor.insert_controller(std::forward<Args>(args)...))
      {
        throw std::runtime_error("流配置器获取失败");
      }
    }
    double usage_rate()
    {return processor.usage_rate();}
    void remove_controller(std::unique_ptr<controller::abstract_controller>&& smart_pointer_value)
    {processor.remove_controller(std::move(smart_pointer_value));}
    void lookup_controller(const custom_string &controller_id)const
    {processor.lookup_controller(controller_id);}
    void flush()
    {processor.flush();}
  };
}
namespace rec
{
  using instrument::chronix;
  using instrument::open_mode;
  using instrument::situation_level;
  using instrument::synthesis;
  namespace configurator
  {
    using controller::console_controller;
    using controller::file_controller;
  }
  namespace underlying_implementation
  {
    using cushioning::underlying_cache;
    using resource_manager::workflow_coordinator;
    using resource_manager::staging_area;
  }
  using recorders::recorder;
}
