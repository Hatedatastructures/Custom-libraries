#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <vector>
/**
* @brief #### 单生产单消费无锁队列类(SPSC)
* @tparam  producer_consumer_type
**/
template<typename producer_consumer_type>
class producer_consumer_queue
{
private:
  std::atomic<size_t> _producer;
  std::atomic<size_t> _consumer;
  const size_t _current_capacity;
  static constexpr size_t _default_capacity = 10;
  std::vector<producer_consumer_type> _shared_circular_queue;
  size_t compute_position(size_t index) const
  {
    return index % _current_capacity;
  }
public:
  explicit producer_consumer_queue(const size_t new_capacity = _default_capacity):_producer(0),_consumer(0),
  _current_capacity(std::max(new_capacity,static_cast<size_t>(1))),_shared_circular_queue(_current_capacity){}
  producer_consumer_queue(const producer_consumer_queue& another_queue) = delete;
  producer_consumer_queue(producer_consumer_queue&& another_queue) noexcept = delete;
  producer_consumer_queue& operator=(const producer_consumer_queue& another_queue) = delete;
  producer_consumer_queue& operator=(producer_consumer_queue&& another_queue) noexcept = delete;
  template<typename push_type>
  bool push(push_type&& produce_data)
  {
    const size_t current_producer = _producer.fetch_add(1,std::memory_order_relaxed);
    const size_t position = compute_position(current_producer);
    if(current_producer - _consumer.load(std::memory_order_acquire) >= _current_capacity)
    {
      _producer.fetch_sub(1,std::memory_order_release);
      return false;
    }
    _shared_circular_queue[position] = std::forward<push_type>(produce_data);
    return true;
  }
  template<typename consume_type>
  bool pop(producer_consumer_type& consume_data)
  {
    const size_t current_consumer = _consumer.fetch_add(1,std::memory_order_relaxed);
    const size_t position = compute_position(current_consumer);
    if(current_consumer >= _producer.load(std::memory_order_acquire))
    {
      _consumer.fetch_sub(1,std::memory_order_release);
      return false;
    }
    consume_data = _shared_circular_queue[position];
    return true;
  }
  bool empty() const
  {
    return _consumer.load(std::memory_order_acquire) == _producer.load(std::memory_order_acquire);
  }
  bool full() const
  {
    return _producer.load(std::memory_order_acquire) - _consumer.load(std::memory_order_acquire) == _current_capacity;
  }
  size_t size() const
  {
    return _producer.load(std::memory_order_acquire) - _consumer.load(std::memory_order_acquire);
  }
};
/**
 * @brief #### 多生产多消费有锁队列类(MPSC)
 * @tparam producers_consumers_type
 */
template<typename producers_consumers_type>
class producers_consumers_queue
{
private:
  mutable std::mutex _access_lock;
  size_t _current_capacity;
  static constexpr size_t _default_capacity = 10;
  std::queue<producers_consumers_type> _shared_queue;
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
  producers_consumers_queue(const size_t new_capacity = _default_capacity)
  :_current_capacity(std::max(new_capacity,static_cast<size_t>(1))){}
  producers_consumers_queue(const producers_consumers_queue& another_queue) = delete;
  producers_consumers_queue(producers_consumers_queue&& another_queue) noexcept = delete;
  producers_consumers_queue& operator=(const producers_consumers_queue& another_queue) = delete;
  producers_consumers_queue& operator=(producers_consumers_queue&& another_queue) noexcept = delete;
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
  bool pop(producers_consumers_type& consume_data)
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
  bool try_pop(producers_consumers_type& consume_data)
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
  bool pop_for(producers_consumers_type& consume_data,const std::chrono::duration<precision, period> time_out)
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
  bool whether_close() const
  {
    return _close_identifier.load(std::memory_order_acquire);
  }
};
template<typename producers_consumers_semaphore_type>
class producers_consumers_semaphore_queue
{
private:
  std::vector<producers_consumers_semaphore_type> _circular_queue;
};
template<typename producers_consumers_type>
class producer_consumer_queues
{
  static constexpr size_t _default_capacity = 10;
  private: 
    size_t _current_capacity;
    std::atomic<bool> _close_identifier;
    std::mutex _produce_mutex,_consume_mtutex;
    std::atomic<bool> _switchover_identifier;
    std::queue<producers_consumers_type> _produce_pipe;
    std::queue<producers_consumers_type> _consume_pipe;
    std::condition_variable _produce_thread_condition;
    std::condition_variable _consume_thread_condition;
    std::atomic<std::queue<producers_consumers_type>*> _produce;
    std::atomic<std::queue<producers_consumers_type>*> _consume;
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
// void read(producers_consumers_queue<task>& task_queue)
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
// void write(producers_consumers_queue<task>& task_queue,const std::string& task_name)
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
//   producers_consumers_queue<task> task_queue;
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
