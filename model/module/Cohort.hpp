#pragma once
#include "Uint.hpp"
#include <queue>
#include <vector>
#include <memory>
#include <typeinfo>
#include <shared_mutex>
#include <unordered_map>

namespace internals
{
  namespace structure_c
  {
  }
}
namespace internals::structure_c
{
  using namespace internals::structure_u;

  enum class cohort_strategy
  {
    fifo, // 先进先出
    priority, // 优先级
    delay, // 延迟
    round_robin // 轮询
  };
}