#include "Structure.hpp"
#include "Cohort.hpp"
#include "Task.hpp"
#include <iostream>

class thread_pool
{
  static constexpr uint64_t CACHE_ALIGNMENT = 64;
  // void monitoring_thread_func(std::stop_token stop_tok)
  // {
  //   while (!stop_tok.stop_requested())
  //   {
  //     std::this_thread::sleep_for(_readjust);
  //     uint64_t current_threads = _metrics._execute_threads.load() + _metrics._closure_threads.load();
  //     uint64_t tasks_count = _tasks_priority_queue.size();
  //     if (tasks_count > _metrics._execute_threads.load() * 2 && current_threads < _max_threads)
  //     {
  //       const uint64_t standard_number = (tasks_count + 1) / 2;
  //       const uint64_t increase_threads = std::min(standard_number, _max_threads - current_threads);
  //       expand_capacity(increase_threads);
  //     }
  //     else if (tasks_count < _metrics._execute_threads.load() && current_threads > _min_threads)
  //     {
  //       const uint64_t standard_number = current_threads - _min_threads;
  //       const uint64_t decrease_threads = std::min(standard_number, _closure_threads.load());
  //       auto  ls = std::thread::get_id();
  //       shrink_capacity(decrease_threads);
  //     }
  //   }
  // }
  // void expand_capacity(uint64_t increase_threads)
  // {

  // }
  // void shrink_capacity(uint64_t decrease_threads)
  // {

  // }

  std::string _name; //线程池标识
  static constexpr uint64_t _backup_threads = 5; // 初始化线程数
  static constexpr uint64_t _backup_min_threads = 1;  // 最小线程数
  static constexpr uint64_t _backup_max_threads = 32; // 最大线程数

  static constexpr internal_time _backup_inactive  = internal_time(5);    // 线程休眠时间
  static constexpr internal_time _backup_readjust  = internal_time(500);  // 调整频率
  static constexpr internal_time _backup_overtime  = internal_time(20);   // 线程超时时间

  mutable std::shared_mutex _exclusive_mutex;  // 回调函数锁

  uint64_t _max_threads;
  uint64_t _min_threads;

  internal_time _inactive;    
  internal_time _readjust;  
  internal_time _overtime;   
  
  std::condition_variable _condtion;  // 条件变量

  std::atomic<uint64_t> _task_distributor{0}; // 任务分配器
  std::atomic<uint64_t> _thread__distributor{0}; // 线程分配器

  metrics _metrics; // 线程池指标

  std::jthread _monitoring_thread; // 后台监控线程
  
  con::mco::concurrent_priority_queue<std::shared_ptr<task_wrapper>> _tasks_priority_queue; // 优先级任务队列

  con::mco::concurrent_unordered_set<uint64_t> _running_tasks; // 正在运行的任务id映射表
  con::mco::concurrent_unordered_set<uint64_t> _waiting_tasks; // 等待运行的任务id映射表

  con::mco::concurrent_unordered_map<uint64_t,internal_thread_info> _thread_serial_number; // 线程映射表

  alignas(CACHE_ALIGNMENT) con::mco::concurrent_vector<internal_thread_info> _workers_thread; // 线程池

  std::function<void(const std::exception &)> _exception_callback; // 异常回调函数

public:
  // thread_pool(uint64_t threads = _backup_threads, uint64_t max_threads = _backup_max_threads, uint64_t min_threads = _backup_min_threads,
  // internal_time inactive = _backup_inactive, internal_time readjust = _backup_readjust, internal_time overtime = _backup_overtime)
  // :_max_threads(max_threads), _min_threads(min_threads), _inactive(inactive), _readjust(readjust), _overtime(overtime)
  // _tasks_priority_queue(threads*3), _workers_thread(threads*2)
  // {
  //   initialize_metrics();
  // }
  /**
   * @brief #### 包装任务函数
   * @param function 任务函数
   * @param priority_level 任务优先级
   * @param parameter 任务参数
   * @return 返回生成的任务和异步结果容器
   */
  template<class func_t, class... function_parameters>
  auto make_task(func_t&& function, priolevel priority_level, function_parameters&&... parameter)
  -> std::pair<std::shared_ptr<task_wrapper>,std::future<std::invoke_result_t<func_t,function_parameters...>>>
  {
    using return_t   = std::invoke_result_t<func_t,function_parameters...>;
    using packaged_t = std::packaged_task<return_t()>;
    //绑定任务参数
    auto fun_t = std::bind(std::forward<func_t>(function), std::forward<function_parameters>(parameter)...);
    //包装函数任务交由std::shared_ptr管理资源
    std::shared_ptr<packaged_t> packaged(new packaged_t(std::move(fun_t)));
    std::shared_ptr<task_wrapper> tmp_task(new task_wrapper);
    //初始化内部任务信息组件
    tmp_task->_unique_identification = _task_distributor.fetch_add(1);
    tmp_task->_task_property         = priority_level;
    tmp_task->_task_function_object  = [packaged](){(*packaged)();};

    return {tmp_task,packaged->get_future()};
  }
  template <class func_t,class ...function_parameters>
  auto submit(func_t && function, function_parameters &&... parameter)
  -> std::future<std::invoke_result_t<func_t, function_parameters...>>
  {
    auto [task,future] = make_task(std::forward<func_t>(function),
      priolevel::normal,std::forward<function_parameters>(parameter)...);
    _tasks_priority_queue.push(task);
    _condtion.notify_one();
    return future;
  }
  template <class func_t,class ...function_parameters>
  auto submit(func_t && function, priolevel priority_level, function_parameters &&... parameter)
  -> std::future<std::invoke_result_t<func_t, function_parameters...>>
  {
    auto [task,future] = make_task(std::forward<func_t>(function),
    priority_level,std::forward<function_parameters>(parameter)...);
    _tasks_priority_queue.push(task);
    _condtion.notify_one();
    return future;
  }
  template <class input_iterator>
  auto submit_batch(input_iterator first, input_iterator last,const std::vector<priolevel> &priority_levels)
  -> std::vector<std::future<void>>
  {
    std::vector<std::future<void>> futures;
    futures.reserve(std::distance(first,last));
    size_t index = 0;
    for(auto it = first;it != last;it++,index++)
    {
      futures.emplace_back(submit(*it,priority_levels[index]));
    }
    return futures;
  }
  // template <class func_t,class pri_t,class... rest>
  // auto submit_batch(func_t && function, pri_t && priority_level, rest &&... parameter)
  // -> std::vector<std::future<void>>
  // {
    
  // }
  // std::string get_thread_pool_name();
  // metrics get_thread_pool_metrics();
  // void set_exception_callback(const std::function<void(const std::exception &)> &callback);
  // void set_thread_pool_name(const std::string &name);
  // void set_thread_pool_inactive(internal_time inactive);
  // void set_thread_pool_readjust(internal_time readjust);
  // void set_thread_pool_overtime(internal_time overtime);
  // void set_thread_pool_max_threads(uint64_t max_threads);
  // void set_thread_pool_min_threads(uint64_t min_threads);
  // void set_thread_priority(std::string thread_name, bool priolevel);
  // void set_thread_scope(std::string thread_name, scope scope);
  // // void get_thread_information(std::string thread_name);
  // void add_priority_task(async_task task_information, priolevel property = priolevel::normal);
  // void add_thread(const std::string &name,bool property = false);
  // void add_task(async_task task_information);
};
int main()
{
  return 0;
}