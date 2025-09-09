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

namespace internals
{
  namespace structure_c {}
}
namespace internals::structure_c
{
  using namespace internals::structure_u;
  using safety_unit_pointer = std::shared_ptr<uint_ordinary>;
  enum class cohort_strategy
  {
    fifo, // 先进先出
    priority, // 优先级
    delay, // 延迟
    round_robin, // 轮询
  };
  class cohort_ordinary
  {
  public:
    virtual ~cohort_ordinary() = default;
    virtual bool push(safety_unit_pointer pointer) {return false;}
    virtual safety_unit_pointer pop()      {return nullptr;}
    virtual safety_unit_pointer try_pop()  {return nullptr;}
    virtual std::size_t size()const {return 0;}
    virtual bool empty()const{return true;}
    virtual void clear() {}
    virtual void close() {}
    virtual bool closed() const { return true; }
    virtual std::size_t set_max_size() const { return 0; }
    virtual std::size_t get_max_size(size_t max_size)const  {}
    virtual std::size_t get_sub_queue_count()  const { return 0; }
    virtual std::size_t get_delay_uint_count() const { return 0; }
    virtual std::size_t get_ready_uint_count() const { return 0; }
    virtual void add_sub_cohort(std::unique_ptr<cohort_ordinary> cohort) {}
    virtual void remove_sub_cohort(std::unique_ptr<cohort_ordinary> cohort) {}
    virtual safety_unit_pointer try_pop_for(const std::chrono::milliseconds& timeout)
    virtual void wait_empty() const {}
    virtual void wait_closed() const {}
    virtual void wait_for_empty(const std::chrono::milliseconds& timeout) const {}
    virtual void wait_for_closed(const std::chrono::milliseconds& timeout) const {}
    virtual cohort_strategy strategy() const { return cohort_strategy::fifo; }
    {
      return nullptr;
    }
    template<typename rep, typename period>
    safety_unit_pointer try_pop_for(const std::chrono::duration<rep, period>& timeout)
    {
      return try_pop_for(convert_time::to_milliseconds(timeout));
    }
  protected:
    std::atomic<bool> _closed{false}; //关闭标识
    std::atomic<std::size_t> _max_size{0}; //最大队列大小
  };
}