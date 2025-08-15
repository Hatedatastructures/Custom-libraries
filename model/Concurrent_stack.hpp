/**
 * @file Concurrent_stack.hpp
 * @brief 线程安全的 LIFO 栈（多生产者多消费者）
 * @author wang
 * @version 1.0
 * @date 2025-08-15
 *
 * 内部使用 std::stack 作为底层容器，
 * 通过 std::mutex + std::condition_variable 实现线程安全：
 *   - push：若栈已满则阻塞等待；
 *   - pop：若栈为空则阻塞等待；
 *   - 支持任意数量的生产者和消费者并发访问。
 */

#pragma once
#include <stack>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <utility>

namespace con
{
  /**
   * @class concurrent_stack
   * @brief 线程安全的后进先出栈
   * @tparam T         元素类型
   * @tparam Container 底层容器，默认 std::vector<T>
   *
   * 使用示例：
   *   concurrent_stack<int> st(10); // 最大容量 10
   *   st.push(42);
   *   int val;
   *   st.pop(val); // 取出栈顶元素
   */
  template <typename T,
            typename Container = std::vector<T>>
  class concurrent_stack
  {
    using base_stack = std::stack<T, Container>;

  private:
    base_stack _st;                        /**< 底层栈容器 */
    mutable std::mutex _mtx;               /**< 互斥锁 */
    std::condition_variable _cv_not_full;  /**< 等待栈未满 */
    std::condition_variable _cv_not_empty; /**< 等待栈非空 */
    std::size_t _max_cap;                  /**< 最大容量（0 表示无界） */

  public:
    /**
     * @brief 构造可指定最大容量的栈
     * @param max_capacity 最大元素个数，0 表示无界
     */
    explicit concurrent_stack(std::size_t max_capacity = 0)
        : _max_cap(max_capacity) {}

    /** 禁止拷贝，允许移动 */
    concurrent_stack(const concurrent_stack &) = delete;
    concurrent_stack &operator=(const concurrent_stack &) = delete;
    concurrent_stack(concurrent_stack &&) = default;
    concurrent_stack &operator=(concurrent_stack &&) = default;

    /**
     * @brief 获取当前栈元素个数
     * @return 元素数量
     */
    std::size_t size() const
    {
      std::unique_lock<std::mutex> lock(_mtx);
      return _st.size();
    }

    /** @brief 判断栈是否为空 */
    bool empty() const
    {
      std::unique_lock<std::mutex> lock(_mtx);
      return _st.empty();
    }

    /**
     * @brief 获取最大容量
     * @return 最大元素个数；0 表示无界
     */
    std::size_t max_capacity() const
    {
      return _max_cap;
    }

    /**
     * @brief 判断栈是否已满
     * @return true 已满；false 未满或无界
     */
    bool full() const
    {
      std::unique_lock<std::mutex> lock(_mtx);
      return _max_cap != 0 && _st.size() >= _max_cap;
    }

    /**
     * @brief 入栈（拷贝）
     * @param value 待入栈元素
     * @note 若栈已满将阻塞等待；入栈后唤醒一个等待 pop 的线程
     */
    void push(const T &value)
    {
      std::unique_lock<std::mutex> lock(_mtx);
      if (_max_cap != 0)
        _cv_not_full.wait(lock, [this]
                          { return _st.size() < _max_cap; });
      _st.push(value);
      _cv_not_empty.notify_one(); // 通知等待 pop 的线程
    }

    /** @brief 入栈（移动） */
    void push(T &&value)
    {
      std::unique_lock<std::mutex> lock(_mtx);
      if (_max_cap != 0)
        _cv_not_full.wait(lock, [this]
                          { return _st.size() < _max_cap; });
      _st.push(std::move(value));
      _cv_not_empty.notify_one();
    }

    /**
     * @brief 就地构造入栈
     * @param args 构造元素所需参数
     */
    template <typename... Args>
    void emplace(Args &&...args)
    {
      std::unique_lock<std::mutex> lock(_mtx);
      if (_max_cap != 0)
        _cv_not_full.wait(lock, [this]
                          { return _st.size() < _max_cap; });
      _st.emplace(std::forward<Args>(args)...);
      _cv_not_empty.notify_one();
    }

    /**
     * @brief 出栈（阻塞等待）
     * @param out 接收栈顶元素的引用
     * @note 若栈为空将阻塞等待；出栈后唤醒一个等待 push 的线程
     */
    void pop(T &out)
    {
      std::unique_lock<std::mutex> lock(_mtx);
      _cv_not_empty.wait(lock, [this]
                        { return !_st.empty(); });
      out = std::move(_st.top());
      _st.pop();
      if (_max_cap != 0)
        _cv_not_full.notify_one(); // 栈空出一位，可唤醒 push
    }

    /**
     * @brief 尝试出栈（非阻塞）
     * @param out 接收栈顶元素的引用
     * @return true 成功出栈；false 栈为空
     */
    bool try_pop(T &out)
    {
      std::unique_lock<std::mutex> lock(_mtx);
      if (_st.empty())
        return false;
      out = std::move(_st.top());
      _st.pop();
      if (_max_cap != 0)
        _cv_not_full.notify_one();
      return true;
    }

    /**
     * @brief 清空栈
     * @note  会唤醒所有等待 pop 的线程，但此时栈已空，它们将继续等待
     */
    void clear()
    {
      std::unique_lock<std::mutex> lock(_mtx);
      while (!_st.empty())
        _st.pop();
      _cv_not_full.notify_all(); // 唤醒所有等待 push 的线程
    }

    /**
     * @brief 与另一线程安全栈交换内容
     * @param other 另一个实例
     * @note  采用双锁避免死锁
     */
    void swap(concurrent_stack &other)
    {
      if (this == &other)
        return;
      std::unique_lock<std::mutex> lhs_lock(_mtx);
      std::unique_lock<std::mutex> rhs_lock(other._mtx);
      _st.swap(other._st);
      std::swap(_max_cap, other._max_cap);
      lhs_lock.unlock();
      rhs_lock.unlock();
      _cv_not_empty.notify_all();
      _cv_not_full.notify_all();
      other._cv_not_empty.notify_all();
      other._cv_not_full.notify_all();
    }

    /**
     * @brief 获取当前栈快照（栈顶在前）
     * @return std::vector<T> 元素副本，顺序与出栈顺序一致
     * @note  内部加锁，拷贝后立即释放，外部可安全遍历
     */
    std::vector<T> snapshot() const
    {
      std::unique_lock<std::mutex> lock(_mtx);
      std::vector<T> vec;
      vec.reserve(_st.size());
      base_stack tmp = _st; // 拷贝一份
      while (!tmp.empty())
      {
        vec.emplace_back(std::move(tmp.top()));
        tmp.pop();
      }
      return vec;
    }
  };
}