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
#define file std::make_unique<file_controller>()
enum class situation_level
{
  info,
  warning,
  error,
  fatal
};
using custom_string = std::string;
class underlying_cache 
{
private:
  std::mutex produce_mutex,consume_mutex;                                                     // 生产消费锁
  std::atomic<bool> running_identifier,consume_identifier;                                    // 运行消费标识
  size_t single_container_capacity;                                                           // 单个容器容量
  std::thread background_consumption;                                                         // 后台输出线程
  static constexpr size_t default_capacity = 10;                                              // 默认容量
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
class file_controller : public abstract_controller
{
private:
  std::ofstream file_stream;

public:
  static constexpr custom_string identifier_characters = "file";
  file_controller(const custom_string &file_name)
  {
    file_stream.open(file_name);
  }
  virtual void write(const custom_string &string_value) override
  {
    file_stream << string_value << "\n";
  }
  ~file_controller()
  {
    file_stream.close();
  }
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
  // std::cout << "Hello World!" << std::endl;
  workflow_coordinator log;
  log.insert_controller(console);
  //计算时间
   std::vector<custom_string> log_test;
  for (int i = 0; i < 100000; ++i)
  {
    custom_string tmp = "Hello World! " + std::to_string(i);
    log_test.push_back(tmp);
    // Sleep(1);
    // log.push(tmp);
    // std::cerr << log.usage_rate() << std::endl;
  }
  auto time_start = std::chrono::high_resolution_clock::now();
  log.push_batch(std::move(log_test));
  auto time_end = std::chrono::high_resolution_clock::now();
  Sleep(7000);
  std::cerr << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count() << "ms" << std::endl;
  return 0;
}