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
#include <windows.h>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <condition_variable>
#include <boost/circular_buffer.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#define console std::make_unique<console_controller>()
#define file(file_name) std::make_unique<file_controller>(file_name)
#define file_mode(file,mode) std::make_unique<file_controller>(file,mode)
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
using custom_string = std::string;
class underlying_cache 
{
private:
  std::mutex produce_mutex,consume_mutex;                                                     // 生产消费锁
  std::atomic<bool> running_identifier,consume_identifier;                                    // 运行消费标识
  size_t single_container_capacity;                                                           // 单个容器容量
  std::thread background_consumption;                                                         // 后台输出线程
  static constexpr size_t default_capacity = 10;                                               // 默认容量
  std::condition_variable conditional_variables;                                              // 条件变量
  boost::circular_buffer<custom_string> primary,secondary;                                    // 队列
  std::atomic<boost::circular_buffer<custom_string> *> produce,consume;                       // 生产消费
  std::unordered_map<custom_string, std::function<void(const custom_string &)>> function_map; // 回调函数映射表
  void container_exchange()
  {
    boost::circular_buffer<custom_string> *tmp = produce.load();
    produce.store(consume.load());
    consume.store(tmp);
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
  :running_identifier(true),consume_identifier(true),
  single_container_capacity(container_capacity), produce(&primary),consume(&secondary)
  {
    primary.set_capacity(container_capacity);
    secondary.set_capacity(container_capacity);
    background_consumption = std::thread(&underlying_cache::background_functions, this);
  }
  void push(const custom_string &string_value)
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
      produce.load()->push_back(string_value);
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
  void insert_callback(const custom_string &controller_id, const std::function<void(const custom_string &)> &function_value)
  {
    function_map[controller_id] = function_value;
  }
  void remove_callback(const custom_string &controller_id)
  {
    function_map.erase(controller_id);
  }
  bool lookup_callback(const custom_string &controller_id)const
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
class abstract_controller
{
public:
  static constexpr custom_string identifier_characters = "abstract";
  virtual void write(const custom_string &string_value) = 0;
  virtual void flush() = 0;
  virtual ~abstract_controller() = default;
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
class file_controller : public abstract_controller
{
private:
  std::ofstream file_stream;
  open_mode mode;
  custom_string file_name;
  std::mutex file_mutex;
  std::ios::openmode mode_to_flag(open_mode tmp_mode)
  {
    switch (tmp_mode)
    {
    case open_mode::overwrite:
      return std::ios::out;
    case open_mode::append:
      return std::ios::app;
    default:
      return std::ios::out;
    }
  }
  static custom_string mode_to_string(open_mode tmp_mode) 
  {
    switch (tmp_mode) 
    {
      case open_mode::append :
        return "append";
      case open_mode::overwrite :
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
  static constexpr custom_string identifier_characters = "file";
  file_controller(const custom_string &tmp_file_name,const open_mode& tmp_mode = open_mode::overwrite)
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
  virtual void write(const custom_string &string_value) override
  {
    file_stream << string_value << "\n";
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
  static constexpr custom_string identifier_characters = "console";
  console_controller()
  : stream(std::cout) {}
  virtual void write(const custom_string &string_value) override
  {
    stream << string_value << "\n";
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
class workflow_coordinator
{
private:
  std::unordered_map<custom_string, std::unique_ptr<abstract_controller>> stream_map; //当前启用的输出流
  underlying_cache cushioning_object;

public:
  workflow_coordinator() = default;
  double usage_rate()
  {
    return cushioning_object.usage_rate();
  }
  void push_batch(std::vector<custom_string>&& vector_string_value)
  {
    cushioning_object.push_batch(std::forward<std::vector<custom_string>>(vector_string_value));
  }
  bool insert_controller(std::unique_ptr<abstract_controller>&& smart_pointer_value)
  {
    if (!smart_pointer_value) return false; 
    const custom_string controller_id = smart_pointer_value->identifier_characters;
    stream_map.insert({controller_id, std::move(smart_pointer_value)});
    auto function_value = [this, controller_id](const custom_string &string_value)
    {
      stream_map.at(controller_id)->write(string_value);
    };
    cushioning_object.insert_callback(controller_id, function_value);
    return lookup_controller(controller_id);
  }
  bool remove_controller(std::unique_ptr<abstract_controller>&& smart_pointer_value)
  {
    custom_string controller_id = smart_pointer_value->identifier_characters;
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
  void push(const custom_string &string_value)
  {
    cushioning_object.push(string_value);
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
  std::list<std::vector<custom_string>> primary_staging_area;
  std::vector<custom_string> secondary_staging_area;
  std::atomic<size_t> staging_area_threshold;

public:
};
class diary
{
private:
  // auto ptr = std::make_unique<console_controller>();
  workflow_coordinator processor;
  staging_area staging_area;

public:
  diary() = default;
  void add(std::unique_ptr<abstract_controller> ptr)
  {
    processor.insert_controller(std::move(ptr));
  }
};
int main()
{
  std::cout << "Hello World!" << std::endl;
  workflow_coordinator log;
  log.insert_controller(file("test.txt"));
  //计算时间
  std::vector<chronix> log_test;
  // for (int i = 0; i < 1000; ++i)
  // {
  //   custom_string tmp = chronix().to_string() + "  Hello World! " + std::to_string(i);
  //   log_test.push_back(tmp);
  //   // Sleep(1);
  //   // log.push(tmp);
  //   // std::cerr << log.usage_rate() << std::endl;
  // }
  // for(int i = 0; i < 1000; ++i)
  // {
  //   custom_string tmp = chronix().to_string() + "  Hello World! " + std::to_string(i);
  //   log.push(tmp);
  // }
  auto time_start = std::chrono::high_resolution_clock::now();
  for(int i = 0; i < 1000000; ++i)
  {
    chronix time_s;
    log_test.push_back(time_s);
    custom_string tmp = time_s.to_string() + "  Hello World! " + std::to_string(i);
    log.push(tmp);
  }
  // log.push_batch(std::move(log_test));
  auto time_end = std::chrono::high_resolution_clock::now();
  // for(auto& tmp : log_test)
  // {
  //   std::cout << tmp << std::endl;
  // }
  std::cout << log_test.begin()->to_string() << std::endl << log_test.back().to_string() << std::endl;
  // Sleep(7000);
  std::cerr << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count() << "ms" << std::endl;
  // std::cout << chronix() << std::endl;
  return 0;
}