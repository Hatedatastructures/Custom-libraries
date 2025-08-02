#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <vector>
/**
* @brief #### 单生产单消费无锁队列类`(SPSC)`
* @tparam  producer_consumer_type 数据类型
* @warning 在严格 `SPSC` 场景下，容器保证线程安全
**/
template<typename producer_consumer_type>
class producer_consumer_queue
{
private:
  std::atomic<uint64_t> _producer;
  std::atomic<uint64_t> _consumer;
  const uint64_t _current_capacity;
  static constexpr uint64_t _default_capacity = 10;
  std::vector<producer_consumer_type> _shared_circular_queue;
  uint64_t compute_position(uint64_t index) const
  {
    return index % _current_capacity;
  }
public:
  explicit producer_consumer_queue(const uint64_t new_capacity = _default_capacity):_producer(0),_consumer(0),
  _current_capacity(std::max(new_capacity,static_cast<uint64_t>(1))),_shared_circular_queue(_current_capacity){}
  producer_consumer_queue(const producer_consumer_queue& another_queue) = delete;
  producer_consumer_queue(producer_consumer_queue&& another_queue) noexcept = delete;
  producer_consumer_queue& operator=(const producer_consumer_queue& another_queue) = delete;
  producer_consumer_queue& operator=(producer_consumer_queue&& another_queue) noexcept = delete;
  bool push(producer_consumer_type&& produce_data)
  {
    const uint64_t current_producer = _producer.fetch_add(1,std::memory_order_relaxed);
    const uint64_t position = compute_position(current_producer);
    if(current_producer - _consumer.load(std::memory_order_acquire) >= _current_capacity)
    {
      _producer.fetch_sub(1,std::memory_order_release);
      return false;
    }
    _shared_circular_queue[position] = std::forward<producer_consumer_type>(produce_data);
    std::atomic_thread_fence(std::memory_order_release);
    return true;
  }
  bool pop(producer_consumer_type& consume_data)
  {
    const uint64_t current_consumer = _consumer.fetch_add(1,std::memory_order_relaxed);
    const uint64_t position = compute_position(current_consumer);
    if(current_consumer >= _producer.load(std::memory_order_acquire))
    {
      _consumer.fetch_sub(1,std::memory_order_release);
      return false;
    }
    std::atomic_thread_fence(std::memory_order_acquire);
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
  uint64_t size() const
  {
    return _producer.load(std::memory_order_acquire) - _consumer.load(std::memory_order_acquire);
  }
};
/**
 * @brief #### 多生产多消费有锁队列类(MPSC)
 * @tparam producers_consumers_type 数据类型
 */
template<typename producers_consumers_type>
class producers_consumers_queue
{
private:
  mutable std::mutex _access_lock;
  uint64_t _current_capacity;
  static constexpr uint64_t _default_capacity = 10;
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
  producers_consumers_queue(const uint64_t new_capacity = _default_capacity)
  :_current_capacity(std::max(new_capacity,static_cast<uint64_t>(1))){}
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
/**
 * @brief #### 生产者消费者有锁信号量队列
 * @tparam producers_consumers_semaphore_type  数据类型
 * @tparam largest_semaphore 最大信号量 , 默认`10`
 * @warning - 模板参数最大信号量尽量和队列长度保持一致
 * @warning - 更改信号量就是更改队列长度
 */
// template<typename producers_consumers_semaphore_type,uint64_t largest_semaphore = 10ULL>
// class producers_consumers_semaphore_queue
// {
// private:
//   static constexpr uint64_t _default_queue_capacity = largest_semaphore;
//   std::vector<producers_consumers_semaphore_type> _circular_shared_queue;
//   std::counting_semaphore<largest_semaphore> _produce_semaphore;
//   std::counting_semaphore<largest_semaphore> _consume_semaphore;
//   std::atomic<uint64_t> _current_queue_size;
//   std::atomic<bool> _close_identifier;
// public:
// };
template<typename producers_consumers_type>
class producer_consumer_queues
{
  static constexpr uint64_t _default_capacity = 10;
private: 
  uint64_t _current_capacity;
  std::atomic<bool> _close_identifier;
  std::mutex _produce_mutex,_consume_mutex;
  std::atomic<bool> _switchover_identifier;
  std::queue<producers_consumers_type> _produce_pipe;
  std::queue<producers_consumers_type> _consume_pipe;
  std::condition_variable _produce_thread_condition;
  std::condition_variable _consume_thread_condition;
  std::atomic<std::queue<producers_consumers_type>*> _produce;
  std::atomic<std::queue<producers_consumers_type>*> _consume;
  void queue_switchover()
  {
    auto* tmp_produce = _produce.load(std::memory_order_relaxed);
    _produce.store(_consume.load(std::memory_order_relaxed),std::memory_order_relaxed);
    _consume.store(tmp_produce,std::memory_order_relaxed);
  }
public:
  producer_consumer_queues(uint64_t capacity = _default_capacity)
  :_current_capacity(capacity),_close_identifier(false),_switchover_identifier(false),
  _produce(&_produce_pipe),_consume(&_consume_pipe){}
  producer_consumer_queues(const producer_consumer_queues& other) = delete;
  producer_consumer_queues& operator=(const producer_consumer_queues& other) = delete;
  producer_consumer_queues(producer_consumer_queues&& other) = default;
  producer_consumer_queues& operator=(producer_consumer_queues&& other) = default;
  bool push(const producers_consumers_type& produce_data)
  {
    if(_close_identifier.load(std::memory_order_relaxed)) return false;
    std::unique_lock<std::mutex> produce_lock(_produce_mutex);
    if(_produce.load(std::memory_order_relaxed)->size() >= _current_capacity)
    {
      std::unique_lock<std::mutex> consume_lock(_consume_mutex);
      auto status = [this](){return _switchover_identifier.load(std::memory_order_acquire);};
      _produce_thread_condition.wait(consume_lock,status);
      queue_switchover();
      _switchover_identifier.store(false,std::memory_order_release);
      consume_lock.unlock();
      _consume_thread_condition.notify_one();
    }
    _produce.load(std::memory_order_relaxed)->push(produce_data);
    return true;
  }
  bool pop(producers_consumers_type& consume_data)
  {
    std::unique_lock<std::mutex> consume_lock(_consume_mutex);
    if(_consume.load(std::memory_order_relaxed)->empty())
    {
      _switchover_identifier.store(true,std::memory_order_release);
      auto status = [this]() {return !_switchover_identifier.load(std::memory_order_acquire);};
      _consume_thread_condition.wait(consume_lock,status);
    }
    if(_consume.load(std::memory_order_relaxed)->empty()) return false;
    consume_data = _consume.load(std::memory_order_relaxed)->front();
    _consume.load(std::memory_order_relaxed)->pop();
    _produce_thread_condition.notify_one();
    return true;
  }
  void flush()
  {

  }
};
