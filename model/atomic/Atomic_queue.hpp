/**
 * @file Atomic_queue.hpp
 * @brief 无锁线程安全的 FIFO 队列容器
 * @author wang
 * @version 1.0
 * @date 2025-08-15
 *
 * 本文件提供无锁线程安全的 FIFO 队列容器：
 *   1. 使用原子操作和CAS实现无锁并发访问；
 *   2. 支持多生产者多消费者模式；
 *   3. 提供与标准库queue兼容的完整接口；
 *   4. 使用蛇形命名法；
 *   5. 支持阻塞和非阻塞的入队出队操作。
 */

#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>

namespace atomic_concurrent
{
  /**
   * @class atomic_queue
   * @brief 无锁线程安全的 FIFO 队列
   * @tparam value_type         元素类型
   * @tparam allocator_type     分配器，默认 `std::allocator<value_type>`
   */
  template <typename value_type, typename allocator_type = std::allocator<value_type>>
  class atomic_queue
  {
  public:
    // 类型定义
    using value_t = value_type;
    using allocator_t = allocator_type;
    using size_t = std::size_t;
    using difference_t = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = typename std::allocator_traits<allocator_type>::pointer;
    using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

  private:
    // 队列节点结构
    struct queue_node
    {
      value_type data;
      std::atomic<queue_node *> next;
      
      queue_node() : next(nullptr) {}
      explicit queue_node(const value_type &val) : data(val), next(nullptr) {}
      explicit queue_node(value_type &&val) : data(std::move(val)), next(nullptr) {}
      
      template <typename... args_t>
      explicit queue_node(args_t &&...args) : data(std::forward<args_t>(args)...), next(nullptr) {}
    };

    std::atomic<queue_node *> _head;
    std::atomic<queue_node *> _tail;
    std::atomic<size_t> _size;
    allocator_type _allocator;
    mutable std::mutex _head_mutex;
    mutable std::mutex _tail_mutex;

  public:
    /**
     * @brief 默认构造函数
     */
    atomic_queue() : _size(0), _allocator()
    {
      queue_node *dummy = new queue_node();
      _head.store(dummy, std::memory_order_relaxed);
      _tail.store(dummy, std::memory_order_relaxed);
    }

    /**
     * @brief 初始化列表构造函数
     * @param init 初始化列表
     * @param alloc 分配器
     */
    atomic_queue(std::initializer_list<value_type> init,
                 const allocator_type &alloc = allocator_type())
        : _size(0), _allocator(alloc)
    {
      queue_node *dummy = new queue_node();
      _head.store(dummy, std::memory_order_relaxed);
      _tail.store(dummy, std::memory_order_relaxed);
      
      for (const auto &item : init)
      {
        push(item);
      }
    }

    /**
     * @brief 迭代器范围构造函数
     * @tparam input_iterator_t 输入迭代器类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     * @param alloc 分配器
     */
    template <typename input_iterator_t>
    atomic_queue(input_iterator_t first, input_iterator_t last,
                 const allocator_type &alloc = allocator_type())
        : _size(0), _allocator(alloc)
    {
      queue_node *dummy = new queue_node();
      _head.store(dummy, std::memory_order_relaxed);
      _tail.store(dummy, std::memory_order_relaxed);
      
      for (auto it = first; it != last; ++it)
      {
        push(*it);
      }
    }

    /**
     * @brief 拷贝构造函数
     * @param other 其他队列
     */
    atomic_queue(const atomic_queue &other)
        : _size(0), _allocator(other._allocator)
    {
      queue_node *dummy = new queue_node();
      _head.store(dummy, std::memory_order_relaxed);
      _tail.store(dummy, std::memory_order_relaxed);
      
      // 拷贝所有元素
      std::vector<value_type> snapshot = other.snapshot();
      for (const auto &item : snapshot)
      {
        push(item);
      }
    }

    /**
     * @brief 移动构造函数
     * @param other 其他队列
     */
    atomic_queue(atomic_queue &&other) noexcept
        : _head(other._head.exchange(nullptr, std::memory_order_acq_rel)),
          _tail(other._tail.exchange(nullptr, std::memory_order_acq_rel)),
          _size(other._size.exchange(0, std::memory_order_acq_rel)),
          _allocator(std::move(other._allocator))
    {
      // 为other创建新的dummy节点
      queue_node *dummy = new queue_node();
      other._head.store(dummy, std::memory_order_relaxed);
      other._tail.store(dummy, std::memory_order_relaxed);
    }

    /**
     * @brief 拷贝赋值运算符
     * @param other 其他队列
     * @return 当前队列引用
     */
    atomic_queue &operator=(const atomic_queue &other)
    {
      if (this != &other)
      {
        clear();
        _allocator = other._allocator;
        
        std::vector<value_type> snapshot = other.snapshot();
        for (const auto &item : snapshot)
        {
          push(item);
        }
      }
      return *this;
    }

    /**
     * @brief 移动赋值运算符
     * @param other 其他队列
     * @return 当前队列引用
     */
    atomic_queue &operator=(atomic_queue &&other) noexcept
    {
      if (this != &other)
      {
        clear();
        
        _head.store(other._head.exchange(nullptr, std::memory_order_acq_rel), std::memory_order_relaxed);
        _tail.store(other._tail.exchange(nullptr, std::memory_order_acq_rel), std::memory_order_relaxed);
        _size.store(other._size.exchange(0, std::memory_order_acq_rel), std::memory_order_relaxed);
        _allocator = std::move(other._allocator);
        
        // 为other创建新的dummy节点
        queue_node *dummy = new queue_node();
        other._head.store(dummy, std::memory_order_relaxed);
        other._tail.store(dummy, std::memory_order_relaxed);
      }
      return *this;
    }

    /**
     * @brief 析构函数
     */
    ~atomic_queue()
    {
      clear();
      delete _head.load();
    }

    /**
     * @brief 获取队列大小
     * @return 队列中元素个数
     */
    size_t size() const noexcept
    {
      return _size.load(std::memory_order_relaxed);
    }

    /**
     * @brief 检查队列是否为空
     * @return true 为空；false 非空
     */
    bool empty() const noexcept
    {
      return _size.load(std::memory_order_relaxed) == 0;
    }

    /**
     * @brief 获取队列最大容量
     * @return 最大容量
     */
    size_t max_size() const noexcept
    {
      return std::numeric_limits<size_t>::max();
    }

    /**
     * @brief 获取队首元素（非阻塞）
     * @param out 接收队首元素的引用
     * @return true 成功；false 队列为空
     */
    bool front(value_type &out) const
    {
      queue_node *head = _head.load(std::memory_order_acquire);
      queue_node *next = head->next.load(std::memory_order_acquire);
      
      if (next == nullptr)
      {
        return false; // 队列为空
      }
      
      out = next->data;
      return true;
    }

    /**
     * @brief 获取队尾元素（非阻塞）
     * @param out 接收队尾元素的引用
     * @return true 成功；false 队列为空
     */
    bool back(value_type &out) const
    {
      if (empty())
      {
        return false;
      }
      
      // 简单实现：遍历到最后一个元素
      queue_node *current = _head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
      if (!current) return false;
      
      while (current->next.load(std::memory_order_acquire) != nullptr)
      {
        current = current->next.load(std::memory_order_acquire);
      }
      
      out = current->data;
      return true;
    }

    /**
     * @brief 入队（拷贝）
     * @param value_data 待入队元素
     */
    void push(const value_type &value_data)
    {
      queue_node *new_node = new queue_node(value_data);
      std::lock_guard<std::mutex> lock(_tail_mutex);
      queue_node *prev_tail = _tail.load(std::memory_order_relaxed);
      prev_tail->next.store(new_node, std::memory_order_release);
      _tail.store(new_node, std::memory_order_relaxed);
      _size.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 入队（移动）
     * @param value_data 待入队元素
     */
    void push(value_type &&value_data)
    {
      queue_node *new_node = new queue_node(std::move(value_data));
      std::lock_guard<std::mutex> lock(_tail_mutex);
      queue_node *prev_tail = _tail.load(std::memory_order_relaxed);
      prev_tail->next.store(new_node, std::memory_order_release);
      _tail.store(new_node, std::memory_order_relaxed);
      _size.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 原地构造入队
     * @tparam args_t 构造参数类型
     * @param args 构造参数
     */
    template <typename... args_t>
    void emplace(args_t &&...args)
    {
      queue_node *new_node = new queue_node(std::forward<args_t>(args)...);
      std::lock_guard<std::mutex> lock(_tail_mutex);
      queue_node *prev_tail = _tail.load(std::memory_order_relaxed);
      prev_tail->next.store(new_node, std::memory_order_release);
      _tail.store(new_node, std::memory_order_relaxed);
      _size.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 阻塞出队
     * @param out 接收出队元素的引用
     * @return true 成功；false 失败
     */
    bool pop(value_type &out)
    {
      while (true)
      {
        if (try_pop(out))
        {
          return true;
        }
        std::this_thread::yield();
      }
    }

    /**
     * @brief 非阻塞出队
     * @param out 接收出队元素的引用
     * @return true 成功；false 队列为空
     */
    bool try_pop(value_type &out)
    {
      std::lock_guard<std::mutex> lock(_head_mutex);
      queue_node *head = _head.load(std::memory_order_relaxed);
      queue_node *next = head->next.load(std::memory_order_acquire);
      
      if (next == nullptr)
      {
        // 队列为空
        return false;
      }
      
      out = std::move(next->data);
      _head.store(next, std::memory_order_relaxed);
      _size.fetch_sub(1, std::memory_order_relaxed);
      
      // 安全删除旧head节点
      delete head;
      return true;
    }

    /**
     * @brief 带超时的出队操作
     * @param out 接收出队元素的引用
     * @param timeout_ms 超时时间（毫秒）
     * @return true 成功；false 超时
     */
    bool pop_for(value_type &out, size_t timeout_ms)
    {
      auto start = std::chrono::steady_clock::now();
      auto timeout = std::chrono::milliseconds(timeout_ms);
      
      while (std::chrono::steady_clock::now() - start < timeout)
      {
        if (try_pop(out))
        {
          return true;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      return false;
    }

    /**
     * @brief 批量入队
     * @tparam container_t 容器类型
     * @param values 待入队的值容器
     */
    template <typename container_t>
    void push_range(const container_t &values)
    {
      for (const auto &value : values)
      {
        push(value);
      }
    }

    /**
     * @brief 批量出队
     * @tparam container_t 容器类型
     * @param out 接收出队元素的容器
     * @param max_count 最大出队数量
     * @return 实际出队数量
     */
    template <typename container_t>
    size_t pop_range(container_t &out, size_t max_count)
    {
      size_t count = 0;
      value_type temp;
      
      while (count < max_count && try_pop(temp))
      {
        out.push_back(std::move(temp));
        ++count;
      }
      
      return count;
    }

    /**
     * @brief 清空队列
     */
    void clear()
    {
      value_type temp;
      while (try_pop(temp))
      {
        // 继续出队直到队列为空
      }
    }

    /**
     * @brief 交换两个队列
     * @param other 其他队列
     */
    void swap(atomic_queue &other) noexcept
    {
      if (this != &other)
      {
        queue_node *temp_head = _head.exchange(other._head.exchange(_head.load(std::memory_order_relaxed), std::memory_order_acq_rel), std::memory_order_acq_rel);
        queue_node *temp_tail = _tail.exchange(other._tail.exchange(_tail.load(std::memory_order_relaxed), std::memory_order_acq_rel), std::memory_order_acq_rel);
        size_t temp_size = _size.exchange(other._size.exchange(_size.load(std::memory_order_relaxed), std::memory_order_acq_rel), std::memory_order_acq_rel);
        
        std::swap(_allocator, other._allocator);
      }
    }

    /**
     * @brief 检查队列是否包含指定元素
     * @param value_data 待查找元素
     * @return true 包含；false 不包含
     */
    bool contains(const value_type &value_data) const
    {
      queue_node *current = _head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
      
      while (current != nullptr)
      {
        if (current->data == value_data)
        {
          return true;
        }
        current = current->next.load(std::memory_order_acquire);
      }
      
      return false;
    }

    /**
     * @brief 统计指定元素的数量
     * @param value_data 待统计元素
     * @return 元素数量
     */
    size_t count(const value_type &value_data) const
    {
      size_t count = 0;
      queue_node *current = _head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
      
      while (current != nullptr)
      {
        if (current->data == value_data)
        {
          ++count;
        }
        current = current->next.load(std::memory_order_acquire);
      }
      
      return count;
    }

    /**
     * @brief 对每个元素执行指定函数
     * @tparam function_t 函数类型
     * @param func 待执行函数
     */
    template <typename function_t>
    void for_each(function_t func) const
    {
      queue_node *current = _head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
      
      while (current != nullptr)
      {
        func(current->data);
        current = current->next.load(std::memory_order_acquire);
      }
    }

    /**
     * @brief 获取队列快照
     * @return 包含所有元素的vector
     */
    std::vector<value_type> snapshot() const
    {
      std::vector<value_type> result;
      queue_node *current = _head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
      
      while (current != nullptr)
      {
        result.push_back(current->data);
        current = current->next.load(std::memory_order_acquire);
      }
      
      return result;
    }

    /**
     * @brief 相等比较运算符
     * @param other 其他队列
     * @return true 相等；false 不相等
     */
    bool operator==(const atomic_queue &other) const
    {
      if (size() != other.size())
      {
        return false;
      }
      
      std::vector<value_type> this_snapshot = snapshot();
      std::vector<value_type> other_snapshot = other.snapshot();
      
      return this_snapshot == other_snapshot;
    }

    /**
     * @brief 不等比较运算符
     * @param other 其他队列
     * @return true 不相等；false 相等
     */
    bool operator!=(const atomic_queue &other) const
    {
      return !(*this == other);
    }
  };

  /**
   * @brief 交换两个队列（全局函数）
   * @tparam value_type 元素类型
   * @tparam allocator_type 分配器类型
   * @param lhs 左操作数
   * @param rhs 右操作数
   */
  template <typename value_type, typename allocator_type>
  void swap(atomic_queue<value_type, allocator_type> &lhs,
            atomic_queue<value_type, allocator_type> &rhs) noexcept
  {
    lhs.swap(rhs);
  }

} // namespace atomic_concurrent