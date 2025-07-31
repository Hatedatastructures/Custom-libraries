#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <semaphore>
// #include <iostream>
// #include <ctime>
// #include <windows.h>
/*
* #### 生产消费者队列模板类
* - 线程安全的 `FIFO` 缓冲区
* - 生产者线程和消费者线程可以并发执行
* - 提供三种入队（`push` 阻塞式、`try_push` 非阻塞式、`push_for` 限时式）
* - 提供三种出队（`pop` 阻塞式、`try_pop` 非阻塞式、`pop_for` 限时式）操作，适配不同场景的线程同步需求。
*/
template<typename prod_cons_type>
class prod_cons_queue
{
private:
  mutable std::mutex _access_lock;
  size_t _current_capacity;
  static constexpr size_t _default_capacity = 10;
  std::queue<prod_cons_type> _shared_queue;
  std::atomic<bool> _close_identifier = false;
  std::condition_variable _produce_thread_condition;
  std::condition_variable _consume_thread_condition;
  bool full_internal() const
  {
    return _shared_queue.size() == _current_capacity;
  }
  bool empty_internal() const
  {
    return _shared_queue.empty();
  }
  template<typename enqueue_type>
  void enqueue(enqueue_type && produce_data)
  {
    _shared_queue.push(std::forward<enqueue_type>(produce_data));
  }
public:
  prod_cons_queue(const size_t new_capacity = _default_capacity)
  :_current_capacity(std::max(new_capacity,static_cast<size_t>(1))){}
  prod_cons_queue(const prod_cons_queue& another_queue) = delete;
  prod_cons_queue(prod_cons_queue&& another_queue) noexcept = delete;
  prod_cons_queue& operator=(const prod_cons_queue& another_queue) = delete;
  prod_cons_queue& operator=(prod_cons_queue&& another_queue) noexcept = delete;
  /*
  * #### 阻塞式入队，队列满时等待
  * - 成功返回 `true`，队列关闭已关闭返回 `false`
  */
  template<typename push_type>
  bool push(push_type&& produce_data)
  {
    std::unique_lock<std::mutex> access_lock(_access_lock);
    while(full_internal() && !_close_identifier)
    {
      _produce_thread_condition.wait(access_lock);
    }
    if(_close_identifier) return false;
    enqueue(std::forward<push_type>(produce_data));
    access_lock.unlock();
    _consume_thread_condition.notify_one();
    return true;
  }
  /*
  * #### 非阻塞式入队，不等待
  * - 成功返回 `true`，失败（锁竞争 / 满 / 关闭）返回 `false`
  */
  template<typename try_push_type>
  bool try_push(try_push_type&& produce_data)
  {
    std::unique_lock<std::mutex> access_lock(_access_lock,std::try_to_lock);
    if(!access_lock.owns_lock()) return false;
    if(_close_identifier.load(std::memory_order_relaxed) || full_internal()) return false;
    enqueue(std::forward<try_push_type>(produce_data));
    access_lock.unlock();
    _consume_thread_condition.notify_one();
    return true;
  }
  /*
  * #### 限时入队，超时返回
  * - 成功返回 `true`，超时 / 关闭返回 `false`
  */
  template<typename push_for_type, typename precision, typename period>
  bool push_for(push_for_type&& produce_data,const std::chrono::duration<precision, period> time_out)
  {
    std::unique_lock<std::mutex> access_lock(_access_lock);
    auto status = _produce_thread_condition.wait_for(access_lock,time_out,[this](){return !full_internal() || _close_identifier;});
    if(_close_identifier || status == std::cv_status::timeout) return false;
    enqueue(std::forward<push_for_type>(produce_data));
    access_lock.unlock();
    _consume_thread_condition.notify_one();
    return true;
  }
  /*
  * #### 阻塞式出队，队列空时等待
  * - 成功返回 `true`，队列关闭已关闭返回 `false`
  */
  bool pop(prod_cons_type& consume_data)
  {
    std::unique_lock<std::mutex> access_lock(_access_lock);
    while(empty_internal() && !_close_identifier)
    {
      _consume_thread_condition.wait(access_lock);
    }
    if(_close_identifier && empty_internal()) return false;
    consume_data = _shared_queue.front();
    _shared_queue.pop();
    access_lock.unlock();
    _produce_thread_condition.notify_one();
    return true;
  }
  /*
  * #### 非阻塞式出队，不等待
  * - 成功返回 `true`，失败（锁竞争 / 空 / 关闭）返回 `false`
  */
  bool try_pop(prod_cons_type& consume_data)
  {
    std::unique_lock<std::mutex> access_lock(_access_lock,std::try_to_lock);
    if(!access_lock.owns_lock()) return false;
    if(_close_identifier.load(std::memory_order_relaxed) || empty_internal()) return false;
    consume_data = _shared_queue.front();
    _shared_queue.pop();
    access_lock.unlock();
    _produce_thread_condition.notify_one();
    return true;
  }
  /*
  * #### 限时出队，超时返回
  * - 成功返回 `true`，超时 / 关闭返回 `false`
  */
  template<typename precision, typename period>
  bool pop_for(prod_cons_type& consume_data,const std::chrono::duration<precision, period> time_out)
  {
    std::unique_lock<std::mutex> access_lock(_access_lock);
    auto status = _consume_thread_condition.wait_for(access_lock,time_out,[this](){return !empty_internal() || _close_identifier;});
    if((_close_identifier && empty_internal()) || status == std::cv_status::timeout) return false;
    consume_data = _shared_queue.front();
    _shared_queue.pop();
    access_lock.unlock();
    _produce_thread_condition.notify_one();
    return true;
  }
  /*
  * #### 关闭队列
  */
  void close() 
  {
    if(_close_identifier.load(std::memory_order_acquire)) return;
    std::unique_lock<std::mutex> access_lock(_access_lock);
    if(_close_identifier) return;
    _close_identifier.store(true,std::memory_order_release);
    access_lock.unlock();
    _produce_thread_condition.notify_all();
    _consume_thread_condition.notify_all();
  }
  bool full() const
  {
    std::lock_guard<std::mutex> access_lock(_access_lock);
    return full_internal();
  }
  bool empty() const
  {
    std::lock_guard<std::mutex> access_lock(_access_lock);
    return empty_internal();
  }
};
template<typename conc_prod_cons_type>
class conc_prod_cons_queue
{
  static constexpr size_t _default_capacity = 10;
  private: 
    size_t _current_capacity;
    std::mutex _produce,_consume;
    std::atomic<bool> _close_identifier;
    std::atomic<bool> _switchover_identifier;
    std::queue<conc_prod_cons_type> _produce_pipe;
    std::queue<conc_prod_cons_type> _consume_pipe;
    std::condition_variable _produce_thread_condition;
    std::condition_variable _consume_thread_condition;
    std::atomic<std::queue<conc_prod_cons_type>*> _produce;
    std::atomic<std::queue<conc_prod_cons_type>*> _consume;
  public:
};
// class task
// {
//   using _func_t = std::function<int(int,int)>;
// public:
//   std::string _task_name;
//   _func_t _func;
//   task(){}
//   task(const std::string& task_name, const _func_t& func)
//   :_task_name(task_name),_func(func){}
//   int operator()(int first,int second) const
//   {
//     return _func(first,second);
//   }
//   ~task(){}
// };
// std::mutex _cout_mutex;
// std::atomic<size_t> _size = {0};
// void read(prod_cons_queue<task>& task_queue)
// {
//   while(true)
//   {
//     task read_task;
//     if(task_queue.pop(read_task))
//     { 
//       std::lock_guard<std::mutex> cout_lock(_cout_mutex);
//       _size++;
//       std::cout << read_task._task_name << " " << read_task(rand()%10000,rand()%10000) << std::endl;
//     }
//     else
//     {
//       break;
//     }
//   }
// }
// void write(prod_cons_queue<task>& task_queue,const std::string& task_name)
// {
//   while(true)
//   {
//     if(!task_queue.push(task(task_name,[](int a,int b){return a*b;})))
//     {
//       break;
//     }
//   }
// }
// int main()
// {
//   srand(time(nullptr));
//   std::vector<std::thread> read_threads;
//   std::vector<std::thread> write_threads;
//   prod_cons_queue<task> task_queue;
//   for(int i = 0; i < 3; ++i)
//   {
//     write_threads.emplace_back(write,std::ref(task_queue),"write_tread" + std::to_string(i));
//   }
//   for(int i = 0; i < 3; ++i)
//   {
//     read_threads.emplace_back(read,std::ref(task_queue));
//   }
//   Sleep(11000);
//   task_queue.close();
//   for(auto& thread : write_threads)
//   {
//     thread.join();
//   }
//   for(auto& thread : read_threads)
//   {
//     thread.join();
//   }
//   std::cout << _size << std::endl;
//   return 0;
// }