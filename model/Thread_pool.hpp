#include <thread>
#include <functional>  //函数包装器
#include <vector>
#include <future>      //异步函数容器
#include <stdexcept>   //异常
#include <concepts>    //概念/ 约束特定的函数传参类型
#include <memory>      //智能指针
#include <type_traits> //类型萃取
#include "Syncs.hpp"   //MPMC队列
using func_t = std::function<void()>;
//C++20
class thread_pool
{
  static constexpr size_t _thread_quantity = 5;
private:
  std::vector<std::jthread> _workers_thread;
  producers_consumers_queue<func_t> _tasks;
  std::condition_variable _condtion;
  void thread_task()
  {
    while(!_tasks.whether_close())
    {
      func_t run_task;
      if(!_tasks.pop(run_task))
      {throw std::runtime_error("线程池任务队列异常,检查任务队列是否异常关闭");}
      run_task();
    }
  }
  void construct_task(size_t threads)
  {
    for(size_t i = 0;i < threads;++i)
    {
      _workers_thread.emplace_back([this]{this->thread_task();});
    }
  }
public:
  thread_pool(size_t cap = _thread_quantity):_workers_thread(cap), _tasks(cap){construct_task(cap);}
  template<class...Args,std::invocable<Args...> func>
  auto submit(func&& task_value,Args&&...args)
  -> std::future<std::invoke_result_t<func,Args...>>
  {
    using return_type = std::invoke_result_t<func,Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
    std::bind(std::forward<func>(task_value),std::forward<Args>(args)...));
    std::future<return_type> result = task->get_future();
    auto detection_thread = [](const std::jthread& thread_detection)
    {return thread_detection.get_stop_token().stop_requested();};
    if(std::any_of(_workers_thread.begin(),_workers_thread.end(),detection_thread))
    {
      throw std::runtime_error("在已经停止的线程上提交任务");
    }
    _tasks.push([task](){(*task)();});
    _condtion.notify_one();
    return result;
  }
};