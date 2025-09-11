#pragma once
#include "Uint.hpp"
#include "Integration.hpp"
#include <set>
#include <deque>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <mutex>
#include <memory>
#include <typeinfo>
#include <shared_mutex>
#include <unordered_map>

#define macro_statement throw operation_exception("The current derived class has not overridden the function.")

namespace internals
{
  namespace structure_c {}
}
namespace internals::structure_c
{
  using namespace internals::structure_u;
  using safety_unit_pointer = std::shared_ptr<uint_ordinary>;

  class rank_ordinary
  {
    using internals_time_t = std::chrono::system_clock::time_point;
    using internals_time = std::shared_ptr<internals_time_t>;
  protected:

  // 计算执行单元默认超时时间点
  internals_time internal_calculation_deadline()
  {
    if(!_unit_time_limit)
    {
      return nullptr;
    }
    internals_time_t now_time = std::chrono::system_clock::now() + _default_function_timeout;
    return std::shared_ptr<internals_time_t>(new internals_time_t(now_time));
  }

  protected:

    std::atomic<bool> _closed{false}; //关闭标识
    std::atomic<bool> _unit_time_limit{false}; //执行单元时间限制
    std::atomic<std::size_t> _max_storage_capacity{0}; //最大队列大小
    std::chrono::milliseconds _default_function_timeout{1000}; //默认等待时间 
    std::atomic<backpressure> _backpressure{backpressure::block}; //背压策略

  protected:
    // 内部推送任务接口
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
    internals_time deadline  = nullptr)
    {
      macro_statement;
      return false;
    }
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode)
    {
      macro_statement;
      return false;
    }
    // 内部批量推送任务接口
    virtual bool internal_push_batch(std::vector<safety_unit_pointer> pointers, backpressure mode)
    {
      macro_statement;
      return false;
    }
    // 内部弹出任务接口
    virtual safety_unit_pointer internal_pop()
    {
      macro_statement;
      return nullptr;
    }
    // 内部批量弹出任务接口
    virtual std::vector<safety_unit_pointer> internal_pop_batch(std::size_t count)
    {
      macro_statement;
      return {};
    }
    // 内部尝试弹出任务接口
    virtual safety_unit_pointer internal_try_pop()
    {
      macro_statement;
      return nullptr;
    }
    // 内部尝试弹出任务接口（带超时）
    virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout)
    {
      macro_statement;
      return nullptr;
    }
    // 内部获取队列大小接口
    virtual std::size_t internal_size() const
    {
      macro_statement;
      return 0;
    }
    // 内部判断队列是否为空接口
    virtual bool internal_empty() const
    {
      macro_statement;
      return true;
    }
    // 内部清空队列接口
    virtual void internal_clear()
    {
      macro_statement;
      return;
    }
    // 内部关闭接口
    virtual void internal_close()
    {
      macro_statement;
      return;
    }
    // 内部获取子队列数量接口
    virtual std::size_t internal_get_sub_queue_count() const
    {
      macro_statement;
      return 0;
    }
    // 内部获取延迟执行单元数量接口
    virtual std::size_t internal_get_delay_uint_count() const
    {
      macro_statement;
      return 0;
    }
    // 内部添加子队列接口
    virtual std::size_t internal_add_sub_cohort(std::unique_ptr<rank_ordinary>&& rank)
    {
      macro_statement;
      return 0;
    }
    // 内部移除子队列接口
    virtual std::size_t internal_remove_sub_cohort(std::unique_ptr<rank_ordinary> rank)
    {
      macro_statement;
      return 0;
    }
    // 内部获取调度策略接口
    virtual rank_strategy internal_strategy() const
    {
      macro_statement;
      return rank_strategy::fifo;
    }
  public:
    rank_ordinary(const std::size_t size) :_max_storage_capacity(size) {} 

    virtual ~rank_ordinary() = default;

    bool push(safety_unit_pointer pointer, backpressure mode = backpressure::block) 
    {
      return internal_push(std::move(pointer), mode, internal_calculation_deadline());
    }

    bool push(safety_unit_pointer pointer, std::chrono::system_clock::time_point deadline,
    backpressure mode = backpressure::block)
    {
      internals_time time_point = std::make_shared<std::chrono::system_clock::time_point>(deadline);
      return internal_push(std::move(pointer), mode, time_point);
    }

    bool push_batch(std::vector<safety_unit_pointer> pointers, backpressure mode = backpressure::block)
    {
      return internal_push_batch(std::move(pointers), mode);
    }

    safety_unit_pointer pop()
    {
      return internal_pop();
    }

    std::vector<safety_unit_pointer> pop_batch(std::size_t count)
    {
      return internal_pop_batch(count);
    }

    safety_unit_pointer try_pop()
    {
      return internal_try_pop();
    }
    template<typename rep, typename period>
    safety_unit_pointer try_pop_for(const std::chrono::duration<rep, period>& timeout)
    {
      return internal_try_pop_for(convert_time::to_milliseconds(timeout));
    }

    std::size_t size() const
    {
      return internal_size();
    }

    bool empty() const 
    { 
      return internal_empty(); 
    }

    void clear() 
    {
      internal_clear();
    }

    virtual void close() 
    {
      internal_close();
    }
    
    bool closed() const 
    { 
      return _closed.load(std::memory_order_acquire); 
    }
    bool set_max_size(const std::size_t max_size)
    {
      _max_storage_capacity.store(max_size, std::memory_order_relaxed);
      if( _max_storage_capacity.load() ==  max_size)
      {
        return true;
      }
      else
      {
        return false
      }
    }
    std::size_t get_max_size()const  
    {
      return _max_storage_capacity.load();
    }

    std::size_t get_sub_queue_count()  const 
    { 
      return internal_get_sub_queue_count(); 
    }

    std::size_t get_delay_uint_count() const 
    { 
      return internal_get_delay_uint_count(); 
    }

    std::size_t get_ready_uint_count() const 
    { 
      return internal_get_delay_uint_count(); 
    }

    void add_sub_cohort(std::unique_ptr<rank_ordinary> cohort) 
    {
      internal_add_sub_cohort(cohort);
    }

    void remove_sub_cohort(std::unique_ptr<rank_ordinary> cohort)
    {
      internal_remove_sub_cohort(cohort);
    }

    rank_strategy strategy() const 
    { 
      return internal_strategy(); 
    }

    void set_backpressure_mode(backpressure mode)
    {
      _backpressure.store(mode, std::memory_order_relaxed);
    }

    backpressure get_backpressure_mode() const
    {
      return _backpressure.load(std::memory_order_relaxed);
    }
  };

  class rank_standard : public rank_ordinary
  {
  protected:

    std::condition_variable _judge_empty_cv; // 队列空条件变量
    std::deque<safety_unit_pointer> _rank_uint; // 任务队列
    mutable std::shared_mutex _rank_standard_mutex; // 任务队列锁
  public:
    explicit rank_standard(std::size_t max_size = 0) : rank_ordinary(max_size) {}

    virtual ~rank_standard() = default;

  private:
    bool enqueue_with_backpressure(safety_unit_pointer pointer, backpressure mode)
    {
      if(_max_storage_capacity != 0 && _rank_uint.size() >= _max_storage_capacity)
      {
        switch(mode)
          case backpressure::block:
          {
            std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
            auto block_func = [this]()
            {
              return this->_rank_uint.size() < this->_max_storage_capacity
              || this->_closed.load(std::memory_order_acquire);
            };
            _judge_empty_cv.wait(lock, block_func);
            if(_closed.load(std::memory_order_acquire)) return false;
            _rank_uint.push_back(std::move(pointer));
            return true;
          }
          case backpressure::overwrite:
          {
            std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);
            _rank_uint.pop_back();
            _rank_uint.push_back(std::move(pointer));
            return true;
          }
          case backpressure::exception:
            throw operation_exception("The queue is full, please check the overflow policy.");
          case backpressure::drop:
            return false;
          default:
            throw operation_exception("Unknown backpressure mode.");
      }
      else
      {
        std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);
        _rank_uint.push_back(std::move(pointer));
        _judge_empty_cv.notify_one();
        return true;
      }
    }
  protected:
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode =
    _backpressure.load(std::memory_order_relaxed))
    {
      if(_closed.load(std::memory_order_acquire)) return false;
      if(pointer == nullptr) return false;
      return enqueue_with_backpressure(std::move(pointer), mode);
    }
  };
}