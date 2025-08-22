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
      std::atomic<value_type *> data;
      std::atomic<queue_node *> next;
      std::atomic<bool> is_valid;

      queue_node() : data(nullptr), next(nullptr), is_valid(true) {}

      ~queue_node()
      {
        value_type *ptr = data.load();
        if (ptr)
        {
          allocator_type alloc;
          std::allocator_traits<allocator_type>::destroy(alloc, ptr);
          std::allocator_traits<allocator_type>::deallocate(alloc, ptr, 1);
        }
      }
    };

    std::atomic<queue_node *> _head;
    std::atomic<queue_node *> _tail;
    std::atomic<size_t> _size;
    allocator_type _allocator;

    // 内部辅助函数
    queue_node *create_dummy_node()
    {
      return new queue_node();
    }

    queue_node *create_data_node(const value_type &value_data)
    {
      queue_node *node = new queue_node();
      value_type *data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
      std::allocator_traits<allocator_type>::construct(_allocator, data, value_data);
      node->data.store(data);
      return node;
    }

    queue_node *create_data_node(value_type &&value_data)
    {
      queue_node *node = new queue_node();
      value_type *data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
      std::allocator_traits<allocator_type>::construct(_allocator, data, std::move(value_data));
      node->data.store(data);
      return node;
    }

    template <typename... args_t>
    queue_node *create_emplace_node(args_t &&...args)
    {
      queue_node *node = new queue_node();
      value_type *data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
      std::allocator_traits<allocator_type>::construct(_allocator, data, std::forward<args_t>(args)...);
      node->data.store(data);
      return node;
    }

    void cleanup_nodes()
    {
      queue_node *current = _head.load();
      while (current)
      {
        queue_node *next = current->next.load();
        delete current;
        current = next;
      }
    }

  public:
    /** @brief 默认构造空队列 */
    atomic_queue() : _size(0), _allocator()
    {
      queue_node *dummy = create_dummy_node();
      _head.store(dummy);
      _tail.store(dummy);
    }

    /**
     * @brief 初始化列表构造
     * @param init 形如 {1, 2, 3} 的列表
     * @param alloc 分配器
     */
    atomic_queue(std::initializer_list<value_type> init,
                 const allocator_type &alloc = allocator_type())
        : _size(0), _allocator(alloc)
    {
      queue_node *dummy = create_dummy_node();
      _head.store(dummy);
      _tail.store(dummy);

      for (const auto &item : init)
      {
        push(item);
      }
    }

    /**
     * @brief 范围构造
     * @tparam input_iterator_t 输入迭代器
     * @param first 起始
     * @param last  终止（不含）
     * @param alloc 分配器
     */
    template <typename input_iterator_t>
    atomic_queue(input_iterator_t first, input_iterator_t last,
                 const allocator_type &alloc = allocator_type())
        : _size(0), _allocator(alloc)
    {
      queue_node *dummy = create_dummy_node();
      _head.store(dummy);
      _tail.store(dummy);

      for (auto it = first; it != last; ++it)
      {
        push(*it);
      }
    }

    /** @brief 拷贝构造（线程安全） */
    atomic_queue(const atomic_queue &other)
        : _size(0), _allocator(other._allocator)
    {
      queue_node *dummy = create_dummy_node();
      _head.store(dummy);
      _tail.store(dummy);

      // 获取快照并逐个添加
      auto snapshot_data = other.snapshot();
      for (const auto &item : snapshot_data)
      {
        push(item);
      }
    }

    /** @brief 移动构造 */
    atomic_queue(atomic_queue &&other) noexcept
        : _head(other._head.exchange(nullptr)),
          _tail(other._tail.exchange(nullptr)),
          _size(other._size.exchange(0)),
          _allocator(std::move(other._allocator))
    {
      // 为 other 创建新的空队列
      queue_node *dummy = create_dummy_node();
      other._head.store(dummy);
      other._tail.store(dummy);
    }

    /** @brief 拷贝赋值（线程安全） */
    atomic_queue &operator=(const atomic_queue &other)
    {
      if (this != &other)
      {
        clear();
        _allocator = other._allocator;

        auto snapshot_data = other.snapshot();
        for (const auto &item : snapshot_data)
        {
          push(item);
        }
      }
      return *this;
    }

    /** @brief 移动赋值 */
    atomic_queue &operator=(atomic_queue &&other) noexcept
    {
      if (this != &other)
      {
        cleanup_nodes();

        _head.store(other._head.exchange(nullptr));
        _tail.store(other._tail.exchange(nullptr));
        _size.store(other._size.exchange(0));
        _allocator = std::move(other._allocator);

        // 为 other 创建新的空队列
        queue_node *dummy = create_dummy_node();
        other._head.store(dummy);
        other._tail.store(dummy);
      }
      return *this;
    }

    /** @brief 析构函数 */
    ~atomic_queue()
    {
      cleanup_nodes();
    }

    // 容量相关

    /** @brief 当前元素数量 */
    size_t size() const noexcept
    {
      return _size.load();
    }

    /** @brief 是否为空 */
    bool empty() const noexcept
    {
      return _size.load() == 0;
    }

    /** @brief 最大元素数（理论值） */
    size_t max_size() const noexcept
    {
      return std::allocator_traits<allocator_type>::max_size(_allocator);
    }

    // 元素访问

    /**
     * @brief 获取队首元素（不移除）
     * @param out 输出参数，接收元素值
     * @return true 成功；false 队列为空
     */
    bool front(value_type &out) const
    {
      queue_node *head = _head.load();
      queue_node *first = head->next.load();

      if (!first)
        return false;

      value_type *data = first->data.load();
      if (data)
      {
        out = *data;
        return true;
      }
      return false;
    }

    /**
     * @brief 获取队尾元素（不移除）
     * @param out 输出参数，接收元素值
     * @return true 成功；false 队列为空
     */
    bool back(value_type &out) const
    {
      queue_node *tail = _tail.load();
      value_type *data = tail->data.load();

      if (data)
      {
        out = *data;
        return true;
      }
      return false;
    }

    // 修改操作

    /**
     * @brief 入队（拷贝）
     * @param value_data 待入队元素
     */
    void push(const value_type &value_data)
    {
      queue_node *new_node = create_data_node(value_data);
      queue_node *prev_tail = _tail.exchange(new_node);
      prev_tail->next.store(new_node);
      _size.fetch_add(1);
    }

    /**
     * @brief 入队（移动）
     * @param value_data 待入队元素
     */
    void push(value_type &&value_data)
    {
      queue_node *new_node = create_data_node(std::move(value_data));
      queue_node *prev_tail = _tail.exchange(new_node);
      prev_tail->next.store(new_node);
      _size.fetch_add(1);
    }

    /**
     * @brief 就地构造入队
     * @param args 构造参数
     */
    template <typename... args_t>
    void emplace(args_t &&...args)
    {
      queue_node *new_node = create_emplace_node(std::forward<args_t>(args)...);
      queue_node *prev_tail = _tail.exchange(new_node);
      prev_tail->next.store(new_node);
      _size.fetch_add(1);
    }

    /**
     * @brief 出队（阻塞等待）
     * @param out 接收出队元素的引用
     * @return true 成功；false 失败（理论上不会发生）
     */
    bool pop(value_type &out)
    {
      while (true)
      {
        if (try_pop(out))
          return true;

        // 短暂等待后重试
        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
    }

    /**
     * @brief 尝试出队（非阻塞）
     * @param out 接收出队元素的引用
     * @return true 成功；false 队列为空
     */
    bool try_pop(value_type &out)
    {
      queue_node *head = _head.load();
      queue_node *first = head->next.load();

      if (!first)
        return false;

      value_type *data = first->data.load();
      if (!data)
        return false;

      out = std::move(*data);

      // 更新头节点
      if (_head.compare_exchange_weak(head, first))
      {
        first->data.store(nullptr); // 清空数据指针
        _size.fetch_sub(1);
        delete head; // 删除旧头节点
        return true;
      }

      return false;
    }

    /**
     * @brief 带超时的出队操作
     * @param out 接收出队元素的引用
     * @param timeout_ms 超时时间（毫秒）
     * @return true 成功；false 超时
     */
    bool pop_for(value_type &out, size_t timeout_ms)
    {
      auto start_time = std::chrono::steady_clock::now();
      auto timeout_duration = std::chrono::milliseconds(timeout_ms);

      while (true)
      {
        if (try_pop(out))
          return true;

        auto current_time = std::chrono::steady_clock::now();
        if (current_time - start_time >= timeout_duration)
          return false;

        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }

    /**
     * @brief 批量入队
     * @param values 待入队元素的容器
     */
    template <typename container_t>
    void push_range(const container_t &values)
    {
      for (const auto &value_data : values)
      {
        push(value_data);
      }
    }

    /**
     * @brief 批量出队
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
      value_type dummy;
      while (try_pop(dummy))
      {
        // 继续出队直到为空
      }
    }

    /**
     * @brief 与另一无锁队列交换内容
     * @param other 另一个实例
     */
    void swap(atomic_queue &other) noexcept
    {
      if (this == &other)
        return;

      queue_node *this_head = _head.exchange(other._head.load());
      queue_node *this_tail = _tail.exchange(other._tail.load());
      size_t this_size = _size.exchange(other._size.load());

      other._head.store(this_head);
      other._tail.store(this_tail);
      other._size.store(this_size);

      std::swap(_allocator, other._allocator);
    }

    // 查找和算法

    /**
     * @brief 判断元素是否存在
     * @param value_data 待查找值
     * @return true 存在；false 不存在
     */
    bool contains(const value_type &value_data) const
    {
      queue_node *current = _head.load()->next.load();

      while (current)
      {
        value_type *data = current->data.load();
        if (data && *data == value_data)
          return true;
        current = current->next.load();
      }
      return false;
    }

    /**
     * @brief 统计指定值的元素个数
     * @param value_data 待统计值
     * @return 元素个数
     */
    size_t count(const value_type &value_data) const
    {
      size_t result = 0;
      queue_node *current = _head.load()->next.load();

      while (current)
      {
        value_type *data = current->data.load();
        if (data && *data == value_data)
          ++result;
        current = current->next.load();
      }
      return result;
    }

    /**
     * @brief 对每个元素执行函数
     * @param func 函数对象
     */
    template <typename function_t>
    void for_each(function_t func) const
    {
      queue_node *current = _head.load()->next.load();

      while (current)
      {
        value_type *data = current->data.load();
        if (data)
          func(*data);
        current = current->next.load();
      }
    }

    /**
     * @brief 获取当前队列的只读快照
     * @return std::vector<value_type> 元素副本，按 FIFO 顺序
     * @note 返回的是拷贝，外部可安全遍历
     */
    std::vector<value_type> snapshot() const
    {
      std::vector<value_type> result;
      queue_node *current = _head.load()->next.load();

      while (current)
      {
        value_type *data = current->data.load();
        if (data)
          result.push_back(*data);
        current = current->next.load();
      }

      return result;
    }

    // 比较操作

    /**
     * @brief 相等比较
     * @param other 另一个 atomic_queue
     * @return true 相等；false 不相等
     */
    bool operator==(const atomic_queue &other) const
    {
      if (this == &other)
        return true;

      if (_size.load() != other._size.load())
        return false;

      auto this_snapshot = snapshot();
      auto other_snapshot = other.snapshot();

      return this_snapshot == other_snapshot;
    }

    /**
     * @brief 不等比较
     * @param other 另一个 atomic_queue
     * @return true 不相等；false 相等
     */
    bool operator!=(const atomic_queue &other) const
    {
      return !(*this == other);
    }
  };

  // 全局函数

  /**
   * @brief 交换两个 atomic_queue
   * @param lhs 第一个队列
   * @param rhs 第二个队列
   */
  template <typename value_type, typename allocator_type>
  void swap(atomic_queue<value_type, allocator_type> &lhs,
            atomic_queue<value_type, allocator_type> &rhs) noexcept
  {
    lhs.swap(rhs);
  }
}