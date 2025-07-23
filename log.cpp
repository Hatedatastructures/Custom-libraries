#include <list>
#include <ctime>
#include <queue>
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
#define console std::make_unique<console_configurator>()
enum class situation_level
{
  info,
  warning,
  error,
  fatal
};
using custom_string = std::string;
class alternating_ring_buffer
{
public:
  std::mutex produce_mutex;                                                                   // 生产锁
  std::mutex consume_mutex;                                                                   // 输出锁
  std::mutex consume_flags_mutex;                                                             // 消费标识锁
  std::atomic<bool> run_flags{true};                                                          // 运行标志
  std::atomic<bool> consume_flags{true};                                                      // 消费标识
  std::thread consume_thread;                                                                 // 后台输出线程
  size_t single_container_capacity;                                                           // 单个容器容量
  static constexpr size_t default_container_capacity = 100;                                    // 默认容量
  std::condition_variable conditional_variables;                                              // 条件变量
  boost::circular_buffer<custom_string> loop_buffer_primary;                                  // 主队列
  boost::circular_buffer<custom_string> loop_buffer_secondary;                                // 副队列
  std::atomic<boost::circular_buffer<custom_string> *> produce;                               // 生产
  std::atomic<boost::circular_buffer<custom_string> *> consume;                               // 消费
  std::unordered_map<custom_string, std::function<void(const custom_string &)>> function_map; // 哈希表
  // boost::unordered_flat_map<custom_string,situation_level> situation_level_map;
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
    {
      std::lock_guard<std::mutex> lock(consume_flags_mutex);
      consume_flags = true;
    }
    conditional_variables.notify_one();
  }
  void background_functions()
  {
    std::unique_lock<std::mutex> lock(consume_mutex);
    while (run_flags)
    {
      auto tmp_func = [&]()
      { return !run_flags || !consume.load()->empty(); };
      conditional_variables.wait(lock, tmp_func);
      if (run_flags == false)
      {
        break;
      }
      consume_value();
    }
    consume_value();
  }

public:
  alternating_ring_buffer(const size_t &container_capacity = default_container_capacity)
      : single_container_capacity(container_capacity), produce(&loop_buffer_primary),
        consume(&loop_buffer_secondary)
  {
    loop_buffer_primary.set_capacity(container_capacity);
    loop_buffer_secondary.set_capacity(container_capacity);
    consume_thread = std::thread(&alternating_ring_buffer::background_functions, this);
  }
  void push(const custom_string &string_value)
  {
    std::unique_lock<std::mutex>  produce_lock(produce_mutex);
    if (produce.load()->full())
    {
      std::unique_lock<std::mutex> consume_flags_lock(consume_flags_mutex);
      conditional_variables.wait(consume_flags_lock,[&]{return consume_flags.load();});
      consume_flags = false;
      {
        std::lock_guard<std::mutex> consume_lock(consume_mutex);
        container_exchange();
        conditional_variables.notify_one();
      }
      conditional_variables.wait(consume_flags_lock,[&]{return consume_flags.load();});
    }
    produce.load()->push_back(string_value);
  }
  void flush()
  {
    std::lock_guard<std::mutex> lock(produce_mutex); // 交换缓冲区，强制消费线程处理剩余数据
    if (!produce.load()->empty())
    {
      container_exchange();
      conditional_variables.notify_one(); // 通知消费线程
    }
  }
  bool set_capacity(const size_t &new_container_capacity)
  { // 调整双队列大小
    if (new_container_capacity > produce.load()->size() && new_container_capacity > consume.load()->size())
    {
      single_container_capacity = new_container_capacity;
      loop_buffer_primary.set_capacity(new_container_capacity);
      loop_buffer_secondary.set_capacity(new_container_capacity);
      return true;
    }
    return false;
  }
  ~alternating_ring_buffer()
  {
    run_flags = false;
    {
      std::lock_guard<std::mutex> produce_lock(consume_mutex);
      conditional_variables.notify_one(); // 唤醒线程
    }
    if (consume_thread.joinable())
    {
      consume_thread.join();
    }
  }
};
class abstract_file_console
{
public:
  static constexpr custom_string identifier_characters = "abstract";
  virtual void write(const custom_string &string_value) = 0;
  virtual void flush() = 0;
  virtual ~abstract_file_console() = default;
};
class file_configurator : public abstract_file_console
{
private:
  std::ofstream file_stream;

public:
  static constexpr custom_string identifier_characters = "file";
  file_configurator(const custom_string &file_name)
  {
    file_stream.open(file_name);
  }
  virtual void write(const custom_string &string_value) override
  {
    file_stream << string_value << "\n";
  }
  ~file_configurator()
  {
    file_stream.close();
  }
};
class console_configurator : public abstract_file_console
{
private:
  std::ostream &stream;

public:
  static constexpr custom_string identifier_characters = "console";
  console_configurator()
      : stream(std::cout) {}
  virtual void write(const custom_string &string_value) override
  {
    stream << string_value << std::endl;
  }
  virtual void flush() override
  {
    stream.flush();
  }
  ~console_configurator()
  {
    stream.flush();
  }
};
class central_control_processing
{
private:
  std::unordered_map<custom_string, std::unique_ptr<abstract_file_console>> configurator_map;
  alternating_ring_buffer ringbuffer;
  std::function<void(const custom_string &)> write_function;

public:
  central_control_processing() = default;
  void add_configurator(std::unique_ptr<abstract_file_console> smart_pointer_value)
  {
    custom_string temporary_identifiers = smart_pointer_value->identifier_characters;
    if (configurator_map.find(smart_pointer_value->identifier_characters) == configurator_map.end())
    {
      configurator_map.insert({smart_pointer_value->identifier_characters, std::move(smart_pointer_value)});
    }
    if (ringbuffer.function_map.find(temporary_identifiers) == ringbuffer.function_map.end())
    {
      auto temporary_function = [this, temporary_identifiers](const custom_string &string_value)
      {
        (this->configurator_map[temporary_identifiers])->write(string_value);
      };
      ringbuffer.function_map.insert({std::move(temporary_identifiers), std::move(temporary_function)});
    }
  }
  void push(const custom_string &string_value)
  {
    ringbuffer.push(string_value);
    // configurator->flush();
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
  // auto ptr = std::make_unique<console_configurator>();
  central_control_processing processor;
  staging_area staging_area;

public:
  diary() = default;
  void add(std::unique_ptr<abstract_file_console> ptr)
  {
    processor.add_configurator(std::move(ptr));
  }
};
int main()
{
  // std::cout << "Hello World!" << std::endl;
  central_control_processing log;
  log.add_configurator(console);
  //计算时间
  auto time_start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 10000; ++i)
  {
    custom_string tmp = "Hello World! " + std::to_string(i);
    // Sleep(1);
    log.push(tmp);
  }
  auto time_end = std::chrono::high_resolution_clock::now();
  std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count() << "ms" << std::endl;
  return 0;
}