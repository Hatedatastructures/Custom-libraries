#pragma once
#include "Uint.hpp"
#include "Integration.hpp"
#include <queue>
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
  protected:

  // 计算执行单元默认超时时间点
  std::chrono::system_clock::time_point internal_calculation_deadline()
  {
    return std::chrono::system_clock::now() + _default_function_timeout;
  }

  protected:

    std::atomic<bool> _closed{false}; //关闭标识
    std::atomic<std::size_t> _max_storage_capacity{0}; //最大队列大小
    std::chrono::milliseconds _default_function_timeout{1000}; //默认等待时间 
    std::atomic<backpressure> _backpressure{backpressure::block}; //背压策略
  protected:
    // 内部推送任务接口
    virtual bool internal_push(safety_unit_pointer pointer, backpressure mode, 
    std::chrono::system_clock::time_point deadline  = internal_calculation_deadline())
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
    virtual std::size_t internal_add_sub_cohort()
    {
      macro_statement;
      return 0;
    }
    // 内部移除子队列接口
    virtual std::size_t internal_remove_sub_cohort()
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
    virtual ~rank_ordinary() = default;

    bool push(safety_unit_pointer pointer, backpressure mode = backpressure::block) 
    {
      return internal_push(std::move(pointer), mode, internal_calculation_deadline());
    }

    bool push(safety_unit_pointer pointer, std::chrono::system_clock::time_point deadline,
    backpressure mode = backpressure::block)
    {
      return internal_push(std::move(pointer), mode, deadline);
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
    std::size_t set_max_size(const std::size_t max_size)
    {
      return _max_storage_capacity.store(max_size, std::memory_order_relaxed);
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
  };

  class rank_standard : public rank_ordinary
  {
  protected:
    std::queue<safety_unit_pointer> _rank_uint; // 任务队列
  };
}