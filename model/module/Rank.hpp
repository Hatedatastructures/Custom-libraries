#pragma once
#include "Unit.hpp"
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

#define parameter_discard(parameter) (void)(parameter)
#define macro_statement throw operation_exception("The current derived class has not overridden the function.")

namespace internals
{
  namespace structure_r {}
}
namespace internals::structure_r
{
  using namespace internals::structure_u;
  using safety_unit_pointer = std::shared_ptr<unit_ordinary>;

  using internals_time_t = std::chrono::system_clock::time_point;
  using internals_time = std::shared_ptr<internals_time_t>;

  class rank_ordinary
  {
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

  protected:
    // 内部推送任务接口
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
    internals_time deadline  = nullptr)
    {
      parameter_discard(pointer);  parameter_discard(mode);
      parameter_discard(deadline); macro_statement;
      return false;
    }
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode)
    {
      parameter_discard(pointer); parameter_discard(mode); macro_statement;
      return false;
    }
    // 内部批量推送任务接口
    virtual std::size_t internal_push_batch(std::vector<safety_unit_pointer>&& pointers,
       backpressure mode)
    {
      parameter_discard(pointers); parameter_discard(mode); macro_statement;
      return std::size_t(0);
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
      parameter_discard(count); macro_statement;
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
      parameter_discard(timeout); macro_statement;
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
    // virtual std::size_t internal_add_sub_cohort(std::unique_ptr<rank_ordinary>&& rank)
    // {
    //   parameter_discard(rank); macro_statement;
    //   return 0;
    // }
    // // 内部移除子队列接口
    // virtual std::size_t internal_remove_sub_cohort(std::unique_ptr<rank_ordinary> rank)
    // {
    //   macro_statement;
    //   return 0;
    // }
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

    std::size_t push_batch(std::vector<safety_unit_pointer> pointers, backpressure mode = backpressure::block)
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
      return true;
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

    // std::size_t get_ready_uint_count() const 
    // { 
    //   return internal_get_delay_uint_count(); 
    // }

    // std::size_t add_sub_cohort(std::unique_ptr<rank_ordinary>&& cohort) 
    // {
    //   return internal_add_sub_cohort(std::move(cohort));
    // }

    // std::size_t remove_sub_cohort(std::unique_ptr<rank_ordinary> cohort)
    // {
    //   return internal_remove_sub_cohort(std::move(cohort));
    // }

    rank_strategy strategy() const 
    { 
      return internal_strategy(); 
    }

  };

  class rank_standard : public rank_ordinary
  {
  protected:

    std::deque<safety_unit_pointer> _rank_uint;

    std::condition_variable_any _judge_full_cv;
    std::condition_variable_any _judge_empty_cv;

    mutable std::shared_mutex _rank_standard_mutex;

  public:
    explicit rank_standard(std::size_t max_size = 0) : rank_ordinary(max_size) {}

    virtual ~rank_standard() = default;

  private:
    bool enqueue_with_backpressure(safety_unit_pointer pointer, backpressure mode)
    {
      if((_max_storage_capacity != 0 && _rank_uint.size() >= _max_storage_capacity) == false)
      {
        std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);
        _rank_uint.push_back(std::move(pointer));
        _judge_empty_cv.notify_one();
        return true;
      }
      else
      switch(mode)
      {
        case backpressure::block:
        {
          std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
          auto block_func = [this]()
          {
            return this->_rank_uint.size() < this->_max_storage_capacity
            || this->_closed.load(std::memory_order_acquire);
          };
          _judge_full_cv.wait(lock, block_func);
          if(_closed.load(std::memory_order_acquire)) return false;
          _rank_uint.push_back(std::move(pointer));
          lock.unlock();
          _judge_empty_cv.notify_one();
          return true;
        }
        case backpressure::overwrite:
        {
          std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
          _rank_uint.pop_back();
          _rank_uint.push_back(std::move(pointer));
          lock.unlock();
          _judge_empty_cv.notify_one();
          return true;
        }
        case backpressure::exception:
          throw operation_exception("The queue is full, please check the overflow policy.");
        case backpressure::drop:
          return false;
        default:
          throw operation_exception("Unknown backpressure mode.");
      }
    }
  protected:
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode) override
    {
      if(_closed.load(std::memory_order_acquire)) return false;
      if(pointer == nullptr) return false;
      return enqueue_with_backpressure(std::move(pointer), mode);
    }
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
    internals_time timeout_pointer) override
    {
      internals_time_t now_time = std::chrono::system_clock::now();
      if(!timeout_pointer || now_time < *timeout_pointer)
      {
        return internal_push(std::move(pointer), mode);
      }
      return false;
    }
    virtual std::size_t internal_push_batch(std::vector<safety_unit_pointer>&& pointers, 
      backpressure mode) override
    {
      if(_closed.load(std::memory_order_acquire)) return false;
      if(pointers.empty()) throw operation_exception("The vector pointers is empty.");
      std::size_t complete_push_unit_counter = 0;
      for(auto& unit_pointers : pointers)
      {
        if (internal_push(std::move(unit_pointers), mode))
        {
          complete_push_unit_counter++;
        }
      }
      return complete_push_unit_counter;
    }
    virtual safety_unit_pointer internal_pop() override
    {
      std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
      auto  check_units_func = [this]()
      {
        return !this->_rank_uint.empty() || this->_closed.load(std::memory_order_acquire);
      };
      _judge_empty_cv.wait(lock, check_units_func);
      if(_closed.load(std::memory_order_acquire) && this->_rank_uint.empty()) return nullptr;
      auto pointer = std::move(_rank_uint.front());
      _rank_uint.pop_front();
      lock.unlock();
      _judge_full_cv.notify_one();
      return pointer;
    }
    virtual std::vector<safety_unit_pointer> internal_pop_batch(std::size_t count) override
    {
      std::vector<safety_unit_pointer> pointers;

      std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
      pointers.reserve(count);
      auto  popup_func = [this]()
      {
        return !this->_rank_uint.empty();
      };
      _judge_empty_cv.wait(lock, popup_func);
      if(_closed.load(std::memory_order_acquire) && this->_rank_uint.empty()) return pointers;
      count = std::min(count, _rank_uint.size());
      auto first_iterator = std::make_move_iterator(_rank_uint.begin());
      auto last_iterator  = std::make_move_iterator(_rank_uint.begin() + count);
      pointers.assign(first_iterator,last_iterator);
      _rank_uint.erase(_rank_uint.begin(), _rank_uint.begin() + count);

      lock.unlock();
      if(pointers.size() != count)
      {
        // throw operation_exception("The current number of popups does not match the verification logic.");
        // 可写日志记录信息
      }
      _judge_full_cv.notify_one();
      return pointers;
    }
    virtual safety_unit_pointer internal_try_pop() override
    {
      std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);

      if(_rank_uint.empty()) return nullptr;
      auto pointer = std::move(_rank_uint.front());
      _rank_uint.pop_front();

      _judge_full_cv.notify_one();
      return pointer;
    }
    virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout) override
    {
      std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
      auto  popup_func = [this]()
      {
        return !this->_rank_uint.empty();
      };
      if(_judge_empty_cv.wait_for(lock, timeout, popup_func))
      {
        auto pointer = std::move(_rank_uint.front());
        _rank_uint.pop_front();
        lock.unlock();
        _judge_full_cv.notify_one();
        return pointer;
      }
      return nullptr;
    }
    virtual std::size_t internal_size()const override
    {
      std::shared_lock<std::shared_mutex> lock(_rank_standard_mutex);
      return _rank_uint.size();
    }
    virtual bool internal_empty()const override
    {
      std::shared_lock<std::shared_mutex> lock(_rank_standard_mutex);
      return _rank_uint.empty();
    }
    virtual void internal_clear() override
    {
      std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);
      _rank_uint.clear();
    }
    virtual void internal_close() override
    {
      _closed.store(true, std::memory_order_release);
      _judge_empty_cv.notify_all();
      _judge_full_cv.notify_all();
    }
    virtual rank_strategy internal_strategy()const override
    {
      return rank_strategy::fifo;
    }
    virtual std::size_t internal_get_sub_queue_count()const override
    {
      return 0;
    }
    virtual std::size_t internal_get_delay_uint_count()const override
    {
      return 0;
    }
  };
}