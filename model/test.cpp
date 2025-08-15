#include <thread>
#include <functional>  //函数包装器
#include <vector>
#include <string>
#include <shared_mutex>
#include <atomic>        //原子操作
#include <queue>         //优先级队列
#include <future>        //异步函数容器
#include <stdexcept>     //异常
#include <concepts>      //概念/ 约束特定的函数传参类型
#include <memory>        //智能指针
#include <type_traits>   //类型萃取
#include <chrono>        //时间
#include <unordered_set> //哈希表
#include "Syncs.hpp"     //MPMC队列

using async_task = std::function<void()>;

enum class status : uint8_t
{
  operation, paused, shutdown
}
enum class priority : uint8_t
{
  low, normal, high
}
enum class scope
{
  destructible,not_destructible
}
struct metrics  //线程池指标
{
  std::atomic<uint64_t> _total_tasks_submitted;  // 提交任务数
  std::atomic<uint64_t> _total_tasks_completed;  // 完成任务数
  std::atomic<uint64_t> _total_tasks_unsuccess;  // 失败任务数

  std::atomic<uint64_t> _total_tasks_time;       // 执行任务总时间
  std::atomic<uint64_t> _level_tasks_time;       // 执行任务平均时间

  std::atomic<uint64_t> _total_thread_create;    // 创建线程数
  std::atomic<uint64_t> _total_thread_scorch;    // 销毁线程数

  std::atomic<uint64_t> _current_queue_size;     // 当前队列大小
  std::atomic<uint64_t> _current_tasks_size;     // 当前任务数

  std::atomic<uint64_t> _execute_threads;        // 运行线程数
  std::atomic<uint64_t> _closure_threads;        // 空闲线程数
};
class thread_pool
{
  using internal_time = std::chrono::milliseconds;
  void initialize_metrics()
  {
    _metrics._total_tasks_submitted = 0;
    _metrics._total_tasks_completed = 0;
    _metrics._total_tasks_unsuccess = 0;
    _metrics._total_tasks_time = 0;
    _metrics._level_tasks_time = 0;
    _metrics._total_thread_create = 0;
    _metrics._total_thread_scorch = 0;
    _metrics._current_queue_size = 0;
    _metrics._current_tasks_size = 0;
    _metrics._execute_threads = 0;
    _metrics._closure_threads = 0;
  }
  void monitoring_thread_func(std::stop_token stop_tok)
  {
    while (!stop_tok.stop_requested() && !_shutdown)
    {
      std::this_thread::sleep_for(_readjust);
      uint64_t current_threads = _metrics._execute_threads.load() + _metrics._closure_threads.load();
      uint64_t tasks_count = _tasks.size();
      if (tasks_count > _metrics._execute_threads.load() * 2 && current_threads < _max_threads)
      {
        const uint64_t standard_number = (tasks_count + 1) / 2;
        const uint64_t increase_threads = std::min(standard_number, _max_threads - current_threads);
        expand_capacity(increase_threads);
      }
      else if (tasks_count < _metrics._execute_threads.load() && current_threads > _min_threads)
      {
        const uint64_t standard_number = current_threads - _min_threads;
        const uint64_t decrease_threads = std::min(standard_number, _closure_threads.load());
        shrink_capacity(decrease_threads);
      }
    }
  }
  void expand_capacity(uint64_t increase_threads)
  {

  }
  void shrink_capacity(uint64_t decrease_threads)
  {

  }
  struct thread_status
  {
    std::stop_source _stop_src; // 令牌
    status _status; // 运行状态
    std::chrono::steady_clock::time_point _last_active; // 上次活动时间
  };
  struct thread_information
  {
    scope _scope;
    std::jthread _thread;
    thread_status _status; // 线程状态
    std::string _thread_name; // 线程标识
    std::atomic<bool> _priority; // 优先级标识
  };
  struct task_dependence
  {
    std::atomic<bool> _is_dependence_completed; // 依赖任务是否完成
    std::vector<uint64_t> _dependence_task; // 依赖任务
  }
  struct task_information
  {
    uint64_t _task_id; // 任务ID
    async_task _task; // 任务函数
    priority _property; // 任务优先级
    task_dependence _dependence; // 任务依赖关系
  };

  std::string _name; //线程池标识

  static constexpr uint64_t _backup_threads = 5; // 初始化线程数
  static constexpr uint64_t _backup_min_threads = 1;  // 最小线程数
  static constexpr uint64_t _backup_max_threads = 32; // 最大线程数

  static constexpr internal_time _backup_inactive  = internal_time(5);    // 线程休眠时间
  static constexpr internal_time _backup_readjust  = internal_time(500);  // 调整频率
  static constexpr internal_time _backup_overtime  = internal_time(20);   // 线程超时时间

  mutable std::mutex _mutex; // 线程池锁
  mutable std::mutex _map_mutex; // 任务id映射表锁
  mutable std::mutex _name_mutex; // 线程池标识称锁
  mutable std::shared_mutex _exclusive_mutex;  // 回调函数锁

  uint64_t _max_threads;
  uint64_t _min_threads;

  internal_time _inactive;    
  internal_time _readjust;  
  internal_time _overtime;   
  
  std::condition_variable _condtion;  // 条件变量
  std::atomic<bool> _shutdown{false}; // 关闭标识

  std::atomic<uint64_t> _distributor{0}; // 分配器

  metrics _metrics; // 线程池指标

  std::jthread _monitoring_thread; // 后台监控线程

  con::mpmc_queue<async_task> _tasks_queue; // 任务队列
  std::priority_queue<async_task> _tasks_priority_queue; // 优先级任务队列

  std::unordered_set<uint64_t> _running_tasks; // 正在运行的任务id映射表
  std::unordered_map<std::string,thread_information> _thread_name; // 线程池标识映射表

  alignas(CACHE_ALIGNMENT) std::vector<thread_information> _workers_thread; // 线程池

  std::function<void(const std::exception &)> _exception_callback; // 异常回调函数

public:
  thread_pool(uint64_t threads = _backup_threads, uint64_t max_threads = _backup_max_threads, uint64_t min_threads = _backup_min_threads,
  internal_time inactive = _backup_inactive, internal_time readjust = _backup_readjust, internal_time overtime = _backup_overtime)
  :_max_threads(max_threads), _min_threads(min_threads), _inactive(inactive), _readjust(readjust), _overtime(overtime),_tasks_queue(threads*4),
  _tasks_priority_queue(threads*3), _workers_thread(threads*2)
  {
    initialize_metrics();
  }
  std::string get_thread_pool_name();
  metrics get_thread_pool_metrics();
  void set_exception_callback(const std::function<void(const std::exception &)> &callback);
  void set_thread_pool_name(const std::string &name);
  void set_thread_pool_inactive(internal_time inactive);
  void set_thread_pool_readjust(internal_time readjust);
  void set_thread_pool_overtime(internal_time overtime);
  void set_thread_pool_max_threads(uint64_t max_threads);
  void set_thread_pool_min_threads(uint64_t min_threads);
  void set_thread_priority(std::string thread_name, bool priority);
  void set_thread_scope(std::string thread_name, scope scope);
  // void get_thread_information(std::string thread_name);
  void add_priority_task(async_task task, priority property = priority::normal);
  void add_thread(const std::string &name,bool property = false);
  void add_task(async_task task);
};