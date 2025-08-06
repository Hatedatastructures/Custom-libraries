#include <thread>
#include <functional>  //函数包装器
#include <vector>
#include <queue>
#include <future>      //异步函数容器
#include <stdexcept>   //异常
#include <concepts>    //概念/ 约束特定的函数传参类型
#include <memory>      //智能指针
#include <type_traits> //类型萃取
#include <chrono>      //时间
#include "Syncs.hpp"   //MPMC队列

using func_t = std::function<void()>;
//C++20
/**
 * @brief 线程池
 */
class thread_pool
{
  static constexpr uint64_t _thread_quantity = 5;
  static constexpr uint64_t _min_thread_quantity = 1;
  static constexpr uint64_t _max_thread_quantity = 32;
  struct thread_status
  {
    std::jthread _thread;
    std::chrono::steady_clock::time_point _last_active;
  };
  void expansion_capacity(uint64_t _threads)
  {
    for(uint64_t i = 0; i < _threads; ++i)
    {
      create_single_thread();
    }
  }
  /**
   * @brief 把线程设置成停止状态
   */
  void decrease_capacity(uint64_t _threads)
  { 
    std::lock_guard<std::mutex> lock(_mutex);
    uint64_t removed = 0;
    while(removed < _threads && !_workers_thread.empty())
    {
      std::stop_source stop_source;
      _tasks_close.pop(stop_source);
      stop_source.request_stop();
      ++removed;
    }
    _closure_threads -= removed;
  }
  /**
   * @brief 线程任务
   */
  void thread_task(std::stop_token st)
  {
    auto stop_token = std::stop_source(st);
    bool idle = false;
    while(!st.stop_requested())
    {
      func_t task;
      if(_tasks.pop(task))
      {
        if(idle)
        {
          _closure_threads -= 1;
          _execute_threads += 1;
          idle = false;
        }task();
      }else
      {
        if(!idle)
        {
          _execute_threads -= 1;
          _closure_threads += 1;
          idle = true;
        }std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
  }
  void create_single_thread()
  {
    thread_status initial;
    initial._thread = std::jthread([this]{this->thread_task();});
    initial._last_active = std::chrono::steady_clock::now();
    _workers_thread.emplace_back(initial);
  }
  void monitoring_thread_func(std::stop_token stop_token)
  {
    while(!stop_token.stop_requested())
    {
      std::this_thread::sleep_for(_frequency_interval);
      uint64_t current_threads = _execute_threads.load() + _closure_threads.load();
      uint64_t tasks_size = _tasks.size();
      if(tasks_size > _execute_threads * 2 && current_threads < _max_threads)
      {
        uint64_t increase_threads = std::min(_max_threads - current_threads, tasks_size - _execute_threads * 2);
        create_single_thread(increase_threads);
      }
      else if (tasks_size < _execute_threads && current_threads > _min_threads)
      {
        uint64_t decrease_threads = std::min(current_threads - _min_threads, _execute_threads - tasks_size);
        decrease_capacity(decrease_threads);
      }
    }
  }
private:
  uint64_t _max_threads;
  uint64_t _min_threads;
  std::mutex _mutex;
  std::jthread _monitoring_thread;
  std::condition_variable _condtion;
  std::atomic<uint64_t> _execute_threads;    // 正在执行任务的线程
  std::atomic<uint64_t> _closure_threads;    // 空闲线程
  producers_consumers_queue<func_t> _tasks;
  std::vector<thread_status> _workers_thread;
  std::chrono::milliseconds _frequency_interval{500};
  producers_consumers_queue<std::stop_source> _tasks_close;
public:
  thread_pool(uint64_t cap = _thread_quantity)
  :_max_threads(_max_thread_quantity),_min_threads(_min_thread_quantity),_execute_threads(0),_closure_threads(0),
  _tasks(cap*2),_workers_thread(cap),_tasks_close(cap)
  {
    create_single_thread(cap);
    _monitoring_thread = std::jthread([this]{this->monitoring_thread_func();});
  }
  template<class...Args,std::invocable<Args...> func>
  auto submit(func&& task_value,Args&&...args)
  -> std::future<std::invoke_result_t<func,Args...>>
  {
    using return_type = std::invoke_result_t<func,Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
    std::bind(std::forward<func>(task_value),std::forward<Args>(args)...));
    std::future<return_type> result = task->get_future();
    auto detection_thread = [](const std::jthread& thread_detection)
    {
      return thread_detection.get_stop_token().stop_requested();
    };
    if(std::any_of(_workers_thread.begin()->_thread,_workers_thread.end()->_thread,detection_thread))
    {
      throw std::runtime_error("无法向已关闭的线程池提交任务");
    }
    _tasks.push([task](){(*task)();});
    _condtion.notify_one();
    return result;
  }
  ~thread_pool()
  {
    _tasks.close();
    for(auto& thread : _workers_thread)
    {
      if(thread.joinable())
      {
        thread.join();
      }
    }
  }
};