#pragma once
#include "Atomic_vector.hpp"
#include "Atomic_queue.hpp"
#include "Atomic_stack.hpp"
#include "Atomic_list.hpp"
#include "Atomic_map.hpp"
#include "Atomic_set.hpp"
#include "Atomic_array.hpp"
#include "Atomic_annular_queue.hpp"


namespace aco
{
  /**
 * @namespace atomic_concurrent
 * 
 * @brief 无锁并发容器命名空间
 * 
 * 该命名空间包含一系列基于原子操作的无锁并发容器实现，提供高性能的多生产者-多消费者场景下的安全访问接口
 * 
 * 包含的核心容器类型：
 * 
 *   - 序列容器：`atomic_vector`、`atomic_array`、`atomic_list`
 * 
 *   - 关联容器：`atomic_set`、`atomic_map`
 * 
 *   - 容器适配器：`atomic_queue`、`atomic_stack`、`atomic_annular_queue`
 * 
 * @warning 无锁容器虽然性能更高，但在某些极端并发场景下可能出现ABA问题，使用时需要注意内存管理
 * 
 * @note 所有容器均基于CAS（Compare-And-Swap）操作实现，避免了传统锁机制的开销；
 *       但在高竞争场景下可能出现自旋等待，建议根据实际场景选择合适的容器类型。
 */
  namespace aco = atomic_concurrent;
}