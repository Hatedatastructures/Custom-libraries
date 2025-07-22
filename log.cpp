#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <functional>
#include <list>
#include <boost/circular_buffer.hpp>
enum class situation_level
{
  info,
  warning,
  error,
  fatal
};
using custom_string = std::string;
class queue_buffer
{
  public:
    std::mutex swap_mutex; //锁
    std::mutex consume_mutex; //消费锁
    // std::thread consume_thread; //写线程
    size_t single_container_capacity; //单个容器容量
    boost::circular_buffer<custom_string> first;
    boost::circular_buffer<custom_string> second;
    static constexpr size_t default_container_capacity = 10;
    std::function<void(const custom_string&)> write_function; //写函数
    std::atomic<boost::circular_buffer<custom_string>*> produce; //生产者
    std::atomic<boost::circular_buffer<custom_string>*> consume; //消费者
    void container_exchange()
    {
      std::cout << "队列交换" << std::endl;
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
        write_function(value);
      }
      consume.load()->clear();
    }
    void export_value()
    {
      // consume_thread = std::thread(consume_value);
      // consume_thread.join();
      consume_value();
    }
  public:
    queue_buffer(const size_t& container_capacity = default_container_capacity)
    :single_container_capacity(container_capacity),
    produce(&first),consume(&second)
    {
      first. resize(container_capacity);
      second.resize(container_capacity);
    }
    void push(const custom_string& string_value)
    {
      if(produce.load()->full())
      {
        container_exchange();
        export_value();
      }
      produce.load()->push_back(string_value);
    }
    void fllush()
    {
      export_value();
    }
    // bool set_capacity(const size_t& new_container_capacity)
    // { //调整双队列大小
    //   if(first.empty() && second.empty())
    //   {
    //     first.resize(size);
    //     second.resize(size);
    //     return true;
    //   }
    //   return false;
    // }
    ~queue_buffer()
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
    virtual void write(const custom_string& string_value) = 0;
    virtual void flush() = 0;
    virtual void input_function(const custom_string& container_data) = 0;
    virtual ~abstract_file_console() = default;
};
class file_configurator : public abstract_file_console
{
  private:
    std::ofstream file_stream;
  public:
    file_configurator(const custom_string& file_name)
    {
      file_stream.open(file_name);
    }
    virtual void write(const custom_string& string_value) override
    {
      file_stream << string_value << std::endl;
    }
    virtual void input_function(const custom_string& container_data) override
    {
      file_stream << container_data << std::endl;
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
    virtual void input_function(const custom_string& container_data) override
    {
      stream << container_data << std::endl;
    }
    ~console_configurator()
    {
      stream.flush();
    }
};
class data_processor
{
  private:
    std::unique_ptr<abstract_file_console> configurator;
    queue_buffer buffer;
    std::function<void(const custom_string&)> write_function;
  public:
    data_processor(std::unique_ptr<abstract_file_console> ptr_value)
    {
      configurator = std::move(ptr_value);
      write_function = [this](const custom_string& string_value)
      {
        this->configurator->input_function(string_value);
      };
      buffer.write_function = std::move(write_function);
    }
    void push(const custom_string& string_value)
    {
      buffer.push(string_value);
      configurator->flush();
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
    diary()
    :processor(std::make_unique<console_configurator>())
    {}
};
int main()
{
  //std::cout << "Hello World!" << std::endl;
  data_processor log(std::make_unique<console_configurator>());
  for(int i = 0;i < 100;++i)
  {
    custom_string tmp = "Hello World! " + std::to_string(i);
    log.push(tmp);
  }
  return 0;
}