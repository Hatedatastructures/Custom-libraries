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

  /**
   * @brief 任务队列基类
   * @details 任务队列基类，定义了任务队列的基本接口，以及任务队列的基本属性。
   * @warning 该类需重载内部函数版本来消除运行时异常
   */
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
    virtual std::vector<safety_unit_pointer> internal_pop_batch(const std::size_t count)
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

    std::vector<safety_unit_pointer> pop_batch(const std::size_t count)
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
  /**
   * @brief 标准任务队列
   * @details 线程安全的标准任务队列，支持阻塞、覆盖、异常三种背压策略
   */
  class rank_standard : public rank_ordinary
  {
  protected:

    std::deque<safety_unit_pointer> _rank_unit_standard;

    std::condition_variable_any _judge_full_cv;
    std::condition_variable_any _judge_empty_cv;

    mutable std::shared_mutex _rank_standard_mutex;

  public:
    explicit rank_standard(std::size_t max_size = 0) : rank_ordinary(max_size) {}

    virtual ~rank_standard() = default;

  private:
    bool enqueue_with_backpressure(safety_unit_pointer pointer, backpressure mode)
    {
      std::size_t current_size = 0;
      {
        std::shared_lock<std::shared_mutex> lock(_rank_standard_mutex);
        current_size = _rank_unit_standard.size();
      }
      if((_max_storage_capacity != 0 && current_size >= _max_storage_capacity) == false)
      {
        std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);
        _rank_unit_standard.push_back(std::move(pointer));
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
            return this->_rank_unit_standard.size() < this->_max_storage_capacity
            || this->_closed.load(std::memory_order_acquire);
          };
          _judge_full_cv.wait(lock, block_func);
          if(_closed.load(std::memory_order_acquire)) return false;
          _rank_unit_standard.push_back(std::move(pointer));
          lock.unlock();
          _judge_empty_cv.notify_one();
          return true;
        }
        case backpressure::overwrite:
        {
          std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
          if(_rank_unit_standard.empty()) return false;
          _rank_unit_standard.pop_back();
          _rank_unit_standard.push_back(std::move(pointer));
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
        return !this->_rank_unit_standard.empty() || this->_closed.load(std::memory_order_acquire);
      };
      _judge_empty_cv.wait(lock, check_units_func);
      if(_closed.load(std::memory_order_acquire) && _rank_unit_standard.empty()) return nullptr;
      safety_unit_pointer pointer = std::move(_rank_unit_standard.front());
      _rank_unit_standard.pop_front();
      lock.unlock();
      _judge_full_cv.notify_one();
      return pointer;
    }
    virtual std::vector<safety_unit_pointer> internal_pop_batch(const std::size_t count) override
    {
      std::vector<safety_unit_pointer> pointers;

      std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
      pointers.reserve(count);
      auto  popup_func = [this]()
      {
        return !this->_rank_unit_standard.empty();
      };
      _judge_empty_cv.wait(lock, popup_func);
      if(_closed.load(std::memory_order_acquire) && this->_rank_unit_standard.empty()) return pointers;
      std::size_t safety_count = std::min(count, _rank_unit_standard.size());
      auto first = std::make_move_iterator(_rank_unit_standard.begin());
      auto last  = std::make_move_iterator(std::next(_rank_unit_standard.begin(), safety_count));
      pointers.assign(first, last);
      _rank_unit_standard.erase(_rank_unit_standard.begin(),std::next(_rank_unit_standard.begin(), safety_count));
      lock.unlock();
      if(count < safety_count)
      {
        //log funtion
      }
      if (safety_count > 0) _judge_full_cv.notify_all();
      return pointers;
    }
    virtual safety_unit_pointer internal_try_pop() override
    {
      std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);

      if(_rank_unit_standard.empty()) return nullptr;
      auto pointer = std::move(_rank_unit_standard.front());
      _rank_unit_standard.pop_front();

      _judge_full_cv.notify_one();
      return pointer;
    }
    virtual safety_unit_pointer internal_try_pop_for(const std::chrono::milliseconds& timeout) override
    {
      std::unique_lock<std::shared_mutex> lock(_rank_standard_mutex);
      auto  popup_func = [this]()
      {
        return !this->_rank_unit_standard.empty() || this->_closed.load(std::memory_order_acquire);
      };
      if(_judge_empty_cv.wait_for(lock, timeout, popup_func))
      {
        auto pointer = std::move(_rank_unit_standard.front());
        _rank_unit_standard.pop_front();
        lock.unlock();
        _judge_full_cv.notify_one();
        return pointer;
      }
      return nullptr;
    }
    virtual std::size_t internal_size()const override
    {
      std::shared_lock<std::shared_mutex> lock(_rank_standard_mutex);
      return _rank_unit_standard.size();
    }
    virtual bool internal_empty()const override
    {
      std::shared_lock<std::shared_mutex> lock(_rank_standard_mutex);
      return _rank_unit_standard.empty();
    }
    virtual void internal_clear() override
    {
      std::lock_guard<std::shared_mutex> lock(_rank_standard_mutex);
      _closed.store(false, std::memory_order_release);
      _max_storage_capacity.store(0, std::memory_order_release);
      _rank_unit_standard.clear();
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
  class rank_priority : public rank_ordinary
  {
  protected:
    class comparator
    {
    public:
      bool operator()(const safety_unit_pointer& first, const safety_unit_pointer& second)
      {
        return first->get_priority() < second->get_priority();
      }
    };
  protected: 
    std::multiset<safety_unit_pointer,comparator> _rank_unit_priority;

    std::condition_variable_any _judge_empty_cv;
    std::condition_variable_any _judge_full_cv;

    mutable std::shared_mutex _rank_priority_mutex;

  private:
    bool enqueue_with_backpressure(safety_unit_pointer pointer, backpressure mode)
    {
      std::size_t current_size = 0;
      {
        std::shared_lock<std::shared_mutex> lock(_rank_priority_mutex);
        current_size = _rank_unit_priority.size();
      }
      if((_max_storage_capacity != 0 && current_size >= _max_storage_capacity) == false)
      {
        std::lock_guard<std::shared_mutex> lock(_rank_priority_mutex);
        _rank_unit_priority.insert(std::move(pointer));
        _judge_empty_cv.notify_one();
        return true;
      }
      else
      switch(mode)
      {
        case backpressure::block:
        {
          std::unique_lock<std::shared_mutex> lock(_rank_priority_mutex);
          auto block_func = [this]()
          {
            return this->_rank_unit_priority.size() < this->_max_storage_capacity
            || this->_closed.load(std::memory_order_acquire);
          };
          _judge_full_cv.wait(lock, block_func);
          if(_closed.load(std::memory_order_acquire)) return false;
          _rank_unit_priority.insert(std::move(pointer));
          lock.unlock();
          _judge_empty_cv.notify_one();
          return true;
        }
        case backpressure::overwrite:
        { 
          std::unique_lock<std::shared_mutex> lock(_rank_priority_mutex);
          if(_rank_unit_priority.empty()) return false; //安全覆盖
          auto replace_iterator = std::prev(_rank_unit_priority.end());
          _rank_unit_priority.erase(replace_iterator);
          _rank_unit_priority.insert(std::move(pointer));
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
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode)
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
      std::unique_lock<std::shared_mutex> lock(_rank_priority_mutex);
      auto check_units_func = [this]()
      {
        return !this->_rank_unit_priority.empty() || this->_closed.load(std::memory_order_acquire);
      }; 
      _judge_empty_cv.wait(lock, check_units_func);
      if(_closed.load(std::memory_order_acquire) && _rank_unit_priority.empty()) return nullptr;
      safety_unit_pointer high_level_value = *_rank_unit_priority.begin();
      safety_unit_pointer pointer = std::move(const_cast<safety_unit_pointer&>(high_level_value));
      _rank_unit_priority.erase(_rank_unit_priority.begin());
      lock.unlock();
      _judge_full_cv.notify_one();
      return pointer;
    }
    virtual std::vector<safety_unit_pointer> internal_pop_batch(const std::size_t count) override
    {
      std::vector<safety_unit_pointer> pointers;
      std::unique_lock<std::shared_mutex> lock(_rank_priority_mutex);
      auto popup_func = [this]()
      {
        return !this->_rank_unit_priority.empty() || this->_closed.load(std::memory_order_acquire);
      };
      _judge_empty_cv.wait(lock, popup_func);
      if(_closed.load(std::memory_order_acquire) && _rank_unit_priority.empty()) return pointers;
      std::size_t safety_count = std::min(count, _rank_unit_priority.size());
      auto first = std::make_move_iterator(_rank_unit_priority.begin());
      auto last  = std::make_move_iterator(std::next(_rank_unit_priority.begin(), safety_count));
      pointers.assign(first, last);
      _rank_unit_priority.erase(_rank_unit_priority.begin(),std::next(_rank_unit_priority.begin(), safety_count));
      lock.unlock();
      if(count < safety_count)
      {
        //log funtion
      }
      if (safety_count > 0) _judge_full_cv.notify_all();
      return pointers;
    }
  };
}