#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
template<typename prod_cons_type>
class prod_cons_queue
{
private:
  mutable std::mutex _produce,_consume;
  constexpr size_t _default_capacity = 10;
  std::queue<prod_cons_type> _shared_queue;
  std::atomic<bool> _close_identifier = false;
  std::condition_variable _produce_thread_condition;
  std::condition_variable _consume_thread_condition;

public:
  prod_cons_queue(const size_t& new_capacity = _default_capacity)
  :_default_capacity(new_capacity){}
  prod_cons_queue(const prod_cons_queue& another_queue)
  {
    _shared_queue = another_queue._shared_queue;
    _default_capacity = another_queue._default_capacity;
  }
  prod_cons_queue(prod_cons_queue&& another_queue) noexcept
  {
    _shared_queue = std::move(another_queue._shared_queue);
    _default_capacity = another_queue._default_capacity;
  }
  prod_cons_queue& operator=(const prod_cons_queue& another_queue)
  {
    if(this != &another_queue)
    {
      _shared_queue = another_queue._shared_queue;
      _default_capacity = another_queue._default_capacity;
    }
    return *this;
  }
  prod_cons_queue& operator=(prod_cons_queue&& another_queue) noexcept
  {
    if(this != &another_queue)
    {
      _shared_queue = std::move(another_queue._shared_queue);
      _default_capacity = another_queue._default_capacity;
    }
    return *this;
  }
  bool push(const prod_cons_type& produce_data)
  {
    std::unique_lock<std::mutex> produce_lock(_produce);
    while(full() && !_close_identifier)
    {
      _produce_thread_condition.wait(produce_lock);
    }
    if(_close_identifier) return false;
    _shared_queue.push(produce_data);
    produce_lock.unlock();
    _consume_thread_condition.notify_one();
    return true;
  }
  bool try_push(const prod_cons_type& produce_data)
  {
    std::unique_lock<std::mutex> produce_lock(_produce,std::try_to_lock);
    if(!produce_lock.owns_lock()) return false;
    if(_close_identifier.load(std::memory_order_relaxed) || full()) return false;
    _shared_queue.push(produce_data);
    produce_lock.unlock();
    _consume_thread_condition.notify_one();
    return true;
  }
  bool pop(prod_cons_type& consume_data)
  {
    std::unique_lock<std::mutex> consume_lock(_consume);
    while(empty() && !_close_identifier)
    {
      _consume_thread_condition.wait(consume_lock);
    }
    if(_close_identifier && empty()) return false;
    consume_data = _shared_queue.front();
    _shared_queue.pop();
    _produce_thread_condition.notify_one();
    return true;
  }
  bool try_pop(prod_cons_type& consume_data)
  {
    
  }
  void close() 
  {
    if(_close_identifier.load(std::memory_order_acquire)) return;
    std::lock(_produce,_consume);
    std::unique_lock<std::mutex> produce_lock(_produce,std::adopt_lock);
    std::unique_lock<std::mutex> consume_lock(_consume,std::adopt_lock);  
    if(_close_identifier) return;
    _close_identifier.store(true,std::memory_order_release);
    produce_lock.unlock();
    consume_lock.unlock();
    _produce_thread_condition.notify_all();
    _consume_thread_condition.notify_all();
  }
  bool full() const
  {
    if(_shared_queue.size() == _default_capacity)
    {
      return true;
    }
    return false;
  }
  bool empty() const
  {
    return _shared_queue.empty();
  }
};
class conc_prod_cons_queue
{

};
int main()
{

}