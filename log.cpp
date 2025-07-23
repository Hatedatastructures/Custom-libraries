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
#include <iostream>
#include <functional>
#include <unordered_map>
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
    std::mutex swap_mutex;                                       //交换锁
    std::mutex consume_mutex;                                    //输出锁
    std::thread consume_thread;                                  //后台输出线程
    size_t single_container_capacity;                            //单个容器容量
    static constexpr size_t default_container_capacity = 10;     //默认容量
    std::function<void(const custom_string&)> write_function;    //输出函数
    boost::circular_buffer<custom_string> loop_buffer_primary;   //主队列
    boost::circular_buffer<custom_string> loop_buffer_secondary; //副队列
    std::atomic<boost::circular_buffer<custom_string>*> produce; //生产
    std::atomic<boost::circular_buffer<custom_string>*> consume; //消费
    std::unordered_map<custom_string,std::function<void(const custom_string&)>> write_function_map;
    // boost::unordered_flat_map<custom_string,situation_level> situation_level_map;
    void container_exchange()
    {
      // std::cout << "队列交换" << std::endl;
      std::lock_guard<std::mutex> lock(swap_mutex);
      boost::circular_buffer<custom_string>* tmp = produce.load();
      produce.store(consume.load());
      consume.store(tmp);
    }
    void consume_value()
    {
      std::lock_guard<std::mutex> lock(consume_mutex);
      for(auto& value : *(consume.load()))
      {
        // write_function(value);
        for(auto& function_value : write_function_map)
        {
          function_value.second(value);
        }
      }
      consume.load()->clear();
    }
    void export_value()
    {
      std::thread consume_thread = std::thread([this](){this->consume_value();});
      consume_thread.join();
      // consume_value();
    }
  public:
    alternating_ring_buffer(const size_t& container_capacity = default_container_capacity)
    :single_container_capacity(container_capacity),
    produce(&loop_buffer_primary),consume(&loop_buffer_secondary)
    {
      loop_buffer_primary. set_capacity(container_capacity);
      loop_buffer_secondary.set_capacity(container_capacity);
    }
    void push(const custom_string& string_value)
    {
      if(produce.load()->full())
      {
        container_exchange();
        if(!consume.load()->empty())
        {
          export_value();
        }
      }
      produce.load()->push_back(string_value);
    }
    void fllush()
    {
      export_value();
    }
    bool set_capacity(const size_t& new_container_capacity)
    { //调整双队列大小
      if(new_container_capacity > produce.load()->size() && new_container_capacity > consume.load()->size())
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
      if(consume.load()->size() > 0)
      {
        export_value();
      }
      else
      {
        if(produce.load()->size() > 0)
        {
          container_exchange();
          export_value();
        }
      }
      produce.load()->clear();
      consume.load()->clear();
    }
};
class abstract_file_console
{
  public:
    static constexpr custom_string identifier_characters = "abstract";
    virtual void write(const custom_string& string_value) = 0;
    virtual void flush() = 0;
    virtual ~abstract_file_console() = default;
};
class file_configurator : public abstract_file_console
{
  private:
    std::ofstream file_stream;
  public:
    static constexpr custom_string identifier_characters = "file";
    file_configurator(const custom_string& file_name)
    {
      file_stream.open(file_name);
    }
    virtual void write(const custom_string& string_value) override
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
    std::ostream& stream;
  public:
    static constexpr custom_string identifier_characters = "console";
    console_configurator()
    :stream(std::cout){}
    virtual void write(const custom_string& string_value) override
    {
      stream << string_value  << std::endl;
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
class data_processor
{
  private:
    std::unordered_map<custom_string,std::unique_ptr<abstract_file_console>> configurator_map;
    alternating_ring_buffer ringbuffer;
    std::function<void(const custom_string&)> write_function;
  public:
    data_processor() = default;
    void add_configurator(std::unique_ptr<abstract_file_console> ptr_value)
    {
      custom_string temporary_identifiers = ptr_value->identifier_characters;
      if(configurator_map.find(ptr_value->identifier_characters) == configurator_map.end())
      {
        configurator_map.insert({ptr_value->identifier_characters,std::move(ptr_value)});
      }
      if(ringbuffer.write_function_map.find(temporary_identifiers) == ringbuffer.write_function_map.end())
      {
        auto temporary_function = [this,temporary_identifiers](const custom_string& string_value)
        {
          (this->configurator_map[temporary_identifiers])->write(string_value);
        };
        ringbuffer.write_function_map.insert({std::move(temporary_identifiers),std::move(temporary_function)});
      }
    }
    void push(const custom_string& string_value)
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
    data_processor processor;
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
  //std::cout << "Hello World!" << std::endl;
  data_processor log;
  log.add_configurator(console);
  for(int i = 0;i < 10;++i)
  {
    custom_string tmp = "Hello World! " + std::to_string(i);
    log.push(tmp);
  }
  return 0;
}