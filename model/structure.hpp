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
#include "./standard_concurrent/Concurrent_container.hpp" //并发容器
using internal_time = std::chrono::milliseconds;
using internal_time_point = std::chrono::steady_clock::time_point;
using async_task = std::function<void()>;
/**
 * @enum thread_status
 * @brief 线程状态 ：`异常`，`创建`，`运行`，`暂停`，`关闭`
 */
enum class thread_status : uint8_t
{
  abnormal,create,operation, paused, shutdown,
};
/**
 * @enum priolevel
 * @brief 任务优先级 ：`低`，`正常`，`高`
 */
enum class priolevel : uint8_t
{
  low, normal, high
};
class metrics  
{
  template <typename data_information>
  void internal_assignment(data_information&& data)
  {
    _total_tasks_submitted = data._total_tasks_submitted.load();
    _total_tasks_completed = data._total_tasks_completed.load();
    _total_tasks_unsuccess = data._total_tasks_unsuccess.load();
    _total_tasks_time      = data._total_tasks_time.load();
    _level_tasks_time      = data._level_tasks_time.load();
    _total_thread_create   = data._total_thread_create.load();
    _total_thread_scorch   = data._total_thread_scorch.load();
    _current_queue_size    = data._current_queue_size.load();
    _current_tasks_size    = data._current_tasks_size.load();
    _execute_threads       = data._execute_threads.load();
    _closure_threads       = data._closure_threads.load();
  }
public:
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
  metrics()
  {
    _total_tasks_submitted = 0;
    _total_tasks_completed = 0;
    _total_tasks_unsuccess = 0;
    _total_tasks_time      = 0;
    _level_tasks_time      = 0;
    _total_thread_create   = 0;
    _total_thread_scorch   = 0;
    _current_queue_size    = 0;
    _current_tasks_size    = 0;
    _execute_threads       = 0;
    _closure_threads       = 0;
  }
  metrics(const metrics& other)
  {
    internal_assignment(other);
  }
  metrics(metrics&& other)
  {
    internal_assignment(std::move(other));
  }
  metrics& operator=(const metrics& other)
  {
    if(this != &other)
    {
      internal_assignment(other);
    }
    return *this;
  }
  metrics& operator=(metrics&& other)
  {
    if(this != &other)
    {
      internal_assignment(std::move(other));
    }
    return *this;
  }
};
struct task_deps
{
public:
  std::vector<uint64_t> _deps_task;       // 依赖任务id表
  std::atomic<uint16_t> _pending_dep;     // 依赖任务是否完成
  task_deps()
  {
    _pending_dep = 0;
  }
  task_deps(std::vector<uint64_t> deps_task)
  {
    _deps_task = deps_task;
    _pending_dep = 0;
  }
  task_deps& operator=(const task_deps& other)
  {
    _deps_task = other._deps_task;
    _pending_dep = other._pending_dep.load();
    return *this;
  }
};
class task_wrapper
{
public:
  task_deps _task_dependence;                 // 任务依赖关系
  priolevel _task_property;               // 任务优先级
  uint64_t _unique_identification;            // 任务ID
  async_task _task_function_object;           // 任务函数
  task_wrapper() = default;
  task_wrapper(async_task task, priolevel property, task_deps dependence)
  {
    _unique_identification = 0;
    _task_function_object = task;
    _task_property = property;
    _task_dependence = dependence;
  }
};
class thread_status_info
{
public:
  std::string name;             // 线程名称

  int exit_code;                // 线程退出码
  std::string last_exception;   // 上次异常
  int restart_count;            // 重启次数

  uint64_t task_count;          // 任务数量
  bool high_priority_task;      // 是否有高优先级任务

  uint64_t sole_logotype;       // 唯一标识
  std::jthread::id thread_id;   // 线程id 系统标识

  thread_status current_status; // 当前状态
  internal_time_point last_active;    // 上次活动时间
  thread_status_info() = default;
  thread_status_info(const std::string& thread_name)
  {
    name = thread_name;
  }
};
class internal_thread_info
{
public:
  std::string _name;                      // 线程名称
  std::jthread _thread;                   // 线程对象

  std::atomic<int> _exit_code;            // 线程退出码
  std::string _last_exception;            // 上次异常
  std::atomic<int> _restart_count;        // 重启次数

  std::stop_source _stop_src;             // 停止令牌
  thread_status _current_status;          // 当前状态
  internal_time_point _last_active;       // 上次活动时间

  std::jthread::id _thread_id;            // 线程id
  std::atomic<uint64_t> _sole_logotype;   // 唯一标识

  std::atomic<uint64_t> _task_count;      // 执行过的任务数量
  std::atomic<bool> _high_priority_task;  // 是否有高优先级任务
  internal_thread_info() = default;
  internal_thread_info(const std::string& name)
  {
    _name = name;
    _task_count = 0;
    _current_status = thread_status::create;
    _last_active = std::chrono::steady_clock::now();
    _sole_logotype = 0;
    _high_priority_task = false;
  }
  thread_status_info to_thread_status_info() const
  {
    thread_status_info info;
    info.name               = _name;
    info.exit_code          = _exit_code.load();
    info.last_exception     = _last_exception;
    info.restart_count      = _restart_count.load();
    info.high_priority_task = _high_priority_task.load();
    info.sole_logotype      = _sole_logotype.load();
    info.thread_id          = _thread_id;
    info.last_active        = _last_active;
    info.current_status     = _current_status;
    return info;
  }
};