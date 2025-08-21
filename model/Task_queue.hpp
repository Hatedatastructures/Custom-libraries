#pragma once

// 无锁任务队列实现（接口与 queue.hpp 完全一致）
// 说明：
// - 使用原子操作与计数信号量(std::counting_semaphore)实现阻塞/非阻塞获取
// - 不使用任何互斥锁或条件变量
// - FIFO 队列采用 Michael-Scott 无锁队列思想，结合 shared_ptr 原子以简化内存回收
// - 优先级队列采用多桶(Bucket)分级的无锁队列组合实现，近似实现“高优先级优先 + 同级 FIFO”
// - 延迟队列采用无锁有序单链表保存延迟任务 + 后台计时线程搬运到就绪 FIFO 队列
// - 多级队列采用无锁单链表管理子队列，并使用全局信号量协调阻塞弹出

#include "Task.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <queue>
#include <array>
#include <chrono>
#include <thread>
#include <semaphore>
#include <limits>
#include <algorithm>
#include <functional>

namespace task_structure
{
  // 与 queue.hpp 中的定义保持一致
  enum class queue_policy
  {
    fifo,
    priority,
    delay,
    round_robin
  };

  // 适配 task 类型名以与 queue.hpp 的接口保持一致

  class base_task_queue
  {
  public:
    virtual ~base_task_queue() = default;

    virtual bool push(std::shared_ptr<task_base> task) = 0;
    virtual std::shared_ptr<task_base> pop() = 0;
    virtual std::shared_ptr<task_base> try_pop() = 0;

    template <typename Rep, typename Period>
    std::shared_ptr<task_base> try_pop_for(const std::chrono::duration<Rep, Period> &timeout)

    virtual std::size_t size() const = 0;
    virtual bool empty() const = 0;
    virtual void clear() = 0;
    virtual void close() = 0;
    virtual bool is_closed() const = 0;
    virtual queue_policy get_policy() const = 0;
  };
  // ================ 无锁 FIFO 任务队列 =================
  class fifo_task_queue : public base_task_queue
  {
  private:
    struct node
    {
      std::shared_ptr<task_base> value;
      std::atomic<std::shared_ptr<node>> next{nullptr};
      explicit node(std::shared_ptr<task_base> v) : value(std::move(v)) {}
    };

    std::atomic<std::shared_ptr<node>> _head;  // 带哨兵节点
    std::atomic<std::shared_ptr<node>> _tail;

    std::counting_semaphore<std::numeric_limits<int>::max()> _items{0};
    std::atomic<std::size_t> _size{0};
    std::atomic<bool> _closed{false};

    // 记录最大容量（与 queue.hpp 语义一致：0 表示不限制）
    std::atomic<std::size_t> _max_size{0};

    // 正在等待 pop 的线程数，用于 close() 唤醒
    std::atomic<int> _waiters{0};

  private:
    static std::shared_ptr<node> make_dummy()
    {
      return std::make_shared<node>(nullptr);
    }

    bool enqueue_node(std::shared_ptr<node> n)
    {
      while (true)
      {
        auto tail = _tail.load(std::memory_order_acquire);
        auto next = tail->next.load(std::memory_order_acquire);
        if (tail == _tail.load(std::memory_order_acquire))
        {
          if (!next)
          {
            if (tail->next.compare_exchange_weak(next, n, std::memory_order_release, std::memory_order_relaxed))
            {
              _tail.compare_exchange_strong(tail, n, std::memory_order_release, std::memory_order_relaxed);
              return true;
            }
          }
          else
          {
            // 协助推进尾指针
            _tail.compare_exchange_strong(tail, next, std::memory_order_release, std::memory_order_relaxed);
          }
        }
      }
    }

    std::shared_ptr<task_base> dequeue_node()
    {
      while (true)
      {
        auto head = _head.load(std::memory_order_acquire);
        auto tail = _tail.load(std::memory_order_acquire);
        auto next = head->next.load(std::memory_order_acquire);
        if (head == _head.load(std::memory_order_acquire))
        {
          if (!next)
          {
            return nullptr; // 空
          }
          if (head == tail)
          {
            // 协助推进尾指针
            _tail.compare_exchange_strong(tail, next, std::memory_order_release, std::memory_order_relaxed);
            continue;
          }
          auto value = std::move(next->value);
          if (_head.compare_exchange_strong(head, next, std::memory_order_release, std::memory_order_relaxed))
          {
            return value;
          }
        }
      }
    }

  public:
    explicit fifo_task_queue(std::size_t max_size = 0)
      : _head(make_dummy()), _tail(_head.load()), _max_size(max_size)
    {
    }

    ~fifo_task_queue() override
    {
      close();
      clear();
    }

    bool push(std::shared_ptr<task_base> task) override
    {
      if (!task)
      {
        return false;
      }
      if (_closed.load(std::memory_order_acquire))
      {
        return false;
      }
      auto max_cap = _max_size.load(std::memory_order_relaxed);
      if (max_cap != 0)
      {
        // 近似检查容量
        std::size_t current = _size.load(std::memory_order_acquire);
        while (current >= max_cap)
        {
          if (_closed.load(std::memory_order_acquire))
          {
            return false;
          }
          return false; // 与 queue.hpp 语义保持：满则返回 false
        }
      }

      auto n = std::make_shared<node>(std::move(task));
      if (!enqueue_node(n))
      {
        return false;
      }
      _size.fetch_add(1, std::memory_order_acq_rel);
      _items.release();
      return true;
    }

    std::shared_ptr<task_base> pop() override
    {
      while (true)
      {
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
        _waiters.fetch_add(1, std::memory_order_acq_rel);
        _items.acquire();
        _waiters.fetch_sub(1, std::memory_order_acq_rel);

        auto task = dequeue_node();
        if (task)
        {
          _size.fetch_sub(1, std::memory_order_acq_rel);
          return task;
        }
        // 若为 close() 释放的“唤醒票据”，继续循环并检查关闭与空
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
      }
    }

    std::shared_ptr<task_base> try_pop() override
    {
      if (_items.try_acquire())
      {
        auto task = dequeue_node();
        if (task)
        {
          _size.fetch_sub(1, std::memory_order_acq_rel);
          return task;
        }
        // 可能是 close() 的“唤醒票据”，直接返回 nullptr
      }
      return nullptr;
    }

    template <typename Rep, typename Period>
    std::shared_ptr<task_base> try_pop_for(const std::chrono::duration<Rep, Period> &timeout) 
    {
      if (_items.try_acquire_for(timeout))
      {
        auto task = dequeue_node();
        if (task)
        {
          _size.fetch_sub(1, std::memory_order_acq_rel);
          return task;
        }
      }
      return nullptr;
    }

    std::size_t size() const override
    {
      return _size.load(std::memory_order_acquire);
    }

    bool empty() const override
    {
      return size() == 0;
    }

    void clear() override
    {
      while (true)
      {
        auto t = try_pop();
        if (!t)
        {
          break;
        }
      }
    }

    void close() override
    {
      bool expected = false;
      if (_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      {
        // 唤醒所有等待者
        int waiters = _waiters.load(std::memory_order_acquire);
        if (waiters > 0)
        {
          _items.release(waiters);
        }
      }
    }

    bool is_closed() const override
    {
      return _closed.load(std::memory_order_acquire);
    }

    queue_policy get_policy() const override
    {
      return queue_policy::fifo;
    }

    void set_max_size(std::size_t max_size)
    {
      _max_size.store(max_size, std::memory_order_release);
    }

    std::size_t get_max_size() const
    {
      return _max_size.load(std::memory_order_acquire);
    }
  };

  // ================ 无锁 优先级 任务队列 =================
  class priority_task_queue : public base_task_queue
  {
  private:
    // 每个桶一个无锁 FIFO
    struct bucket
    {
      fifo_task_queue queue;
      explicit bucket() : queue(0) {}
    };

    static constexpr int k_bucket_count = 16; // 分 16 桶近似覆盖优先级空间

    std::array<bucket, k_bucket_count> _buckets; // 从高优先到低优先扫描
    std::counting_semaphore<std::numeric_limits<int>::max()> _items{0};
    std::atomic<std::size_t> _size{0};
    std::atomic<bool> _closed{false};
    std::atomic<std::size_t> _max_size{0};
    std::atomic<int> _waiters{0};

    static int clamp_priority(std::int32_t pri)
    {
      // 将任意优先级映射到 [0, k_bucket_count-1]，高数值映射到低索引（优先级高）
      // 选取简单线性分段：[-100, 200] -> [0, k_bucket_count-1]
      long long p = pri;
      long long minp = -100;
      long long maxp = 200;
      if (p < minp) p = minp;
      if (p > maxp) p = maxp;
      long long span = maxp - minp + 1;
      long long idx = (maxp - p) * k_bucket_count / span; // 倒序：高优先级 -> 小索引
      if (idx < 0) idx = 0;
      if (idx >= k_bucket_count) idx = k_bucket_count - 1;
      return static_cast<int>(idx);
    }

    bool try_dequeue_from_highest(std::shared_ptr<task_base> &out)
    {
      for (int i = 0; i < k_bucket_count; ++i)
      {
        auto t = _buckets[i].queue.try_pop();
        if (t)
        {
          out = std::move(t);
          return true;
        }
      }
      return false;
    }

  public:
    explicit priority_task_queue(std::size_t max_size = 0) : _max_size(max_size)
    {
    }

    ~priority_task_queue() override
    {
      close();
      clear();
    }

    bool push(std::shared_ptr<task_base> task) override
    {
      if (!task) return false;
      if (_closed.load(std::memory_order_acquire)) return false;

      auto max_cap = _max_size.load(std::memory_order_relaxed);
      if (max_cap != 0)
      {
        auto cur = _size.load(std::memory_order_acquire);
        if (cur >= max_cap)
        {
          return false;
        }
      }

      int idx = clamp_priority(task->get_priority());
      if (!_buckets[idx].queue.push(task))
      {
        return false;
      }
      _size.fetch_add(1, std::memory_order_acq_rel);
      _items.release();
      return true;
    }

    std::shared_ptr<task_base> pop() override
    {
      while (true)
      {
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
        _waiters.fetch_add(1, std::memory_order_acq_rel);
        _items.acquire();
        _waiters.fetch_sub(1, std::memory_order_acq_rel);

        std::shared_ptr<task_base> out;
        if (try_dequeue_from_highest(out))
        {
          _size.fetch_sub(1, std::memory_order_acq_rel);
          return out;
        }
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
      }
    }

    std::shared_ptr<task_base> try_pop() override
    {
      if (_items.try_acquire())
      {
        std::shared_ptr<task_base> out;
        if (try_dequeue_from_highest(out))
        {
          _size.fetch_sub(1, std::memory_order_acq_rel);
          return out;
        }
      }
      return nullptr;
    }

    template <typename Rep, typename Period>
    std::shared_ptr<task_base> try_pop_for(const std::chrono::duration<Rep, Period> &timeout) 
    {
      if (_items.try_acquire_for(timeout))
      {
        std::shared_ptr<task_base> out;
        if (try_dequeue_from_highest(out))
        {
          _size.fetch_sub(1, std::memory_order_acq_rel);
          return out;
        }
      }
      return nullptr;
    }

    std::size_t size() const override
    {
      return _size.load(std::memory_order_acquire);
    }

    bool empty() const override
    {
      return size() == 0;
    }

    void clear() override
    {
      while (true)
      {
        auto t = try_pop();
        if (!t) break;
      }
    }

    void close() override
    {
      bool expected = false;
      if (_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      {
        int waiters = _waiters.load(std::memory_order_acquire);
        if (waiters > 0)
        {
          _items.release(waiters);
        }
        // 关闭所有桶（非必须，但让其各自停止阻塞行为）
        for (int i = 0; i < k_bucket_count; ++i)
        {
          _buckets[i].queue.close();
        }
      }
    }

    bool is_closed() const override
    {
      return _closed.load(std::memory_order_acquire);
    }

    queue_policy get_policy() const override
    {
      return queue_policy::priority;
    }

    void set_max_size(std::size_t max_size)
    {
      _max_size.store(max_size, std::memory_order_release);
    }

    std::size_t get_max_size() const
    {
      return _max_size.load(std::memory_order_acquire);
    }
  };

  // ================ 无锁 延迟 任务队列 =================
  class delay_task_queue : public base_task_queue
  {
  private:
    struct delayed_node
    {
      std::shared_ptr<task_base> task;
      std::chrono::steady_clock::time_point execute_time;
      std::atomic<std::shared_ptr<delayed_node>> next{nullptr};
      delayed_node(std::shared_ptr<task_base> t, std::chrono::steady_clock::time_point et)
        : task(std::move(t)), execute_time(et) {}
    };

    // 就绪队列使用无锁 FIFO
    fifo_task_queue _ready_queue{0};

    // 延迟链表（按时间升序）无锁插入
    std::atomic<std::shared_ptr<delayed_node>> _delayed_head{nullptr};

    std::counting_semaphore<std::numeric_limits<int>::max()> _items{0};
    std::atomic<std::size_t> _ready_size{0};
    std::atomic<std::size_t> _delayed_size{0};
    std::atomic<bool> _closed{false};
    std::atomic<std::size_t> _max_size{0};
    std::atomic<int> _waiters{0};

    std::thread _timer_thread;
    std::atomic<bool> _timer_running{false};

    void start_timer_thread()
    {
      bool expected = false;
      if (_timer_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      {
        _timer_thread = std::thread([this]()
        {
          while (_timer_running.load(std::memory_order_acquire))
          {
            auto now = std::chrono::steady_clock::now();
            // 将到期任务搬运到 ready
            while (true)
            {
              auto head = _delayed_head.load(std::memory_order_acquire);
              if (!head)
              {
                break;
              }
              if (head->execute_time <= now)
              {
                auto next = head->next.load(std::memory_order_acquire);
                if (_delayed_head.compare_exchange_strong(head, next, std::memory_order_acq_rel))
                {
                  _delayed_size.fetch_sub(1, std::memory_order_acq_rel);
                  _ready_queue.push(head->task);
                  _ready_size.fetch_add(1, std::memory_order_acq_rel);
                  _items.release();
                  continue;
                }
              }
              break;
            }
            // 睡眠一小段时间，避免忙轮询
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
        });
      }
    }

    void stop_timer_thread()
    {
      bool expected = true;
      if (_timer_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
      {
        if (_timer_thread.joinable())
        {
          _timer_thread.join();
        }
      }
    }

    void insert_delayed(std::shared_ptr<delayed_node> n)
    {
      while (true)
      {
        auto head = _delayed_head.load(std::memory_order_acquire);
        // 插入到有序单链表（头插或中插）
        if (!head || n->execute_time < head->execute_time)
        {
          n->next.store(head, std::memory_order_relaxed);
          if (_delayed_head.compare_exchange_weak(head, n, std::memory_order_release, std::memory_order_relaxed))
          {
            return;
          }
          continue;
        }
        // 在链表中寻找插入点（无锁：通过单步重试近似实现）
        auto prev = head;
        auto curr = head->next.load(std::memory_order_acquire);
        while (curr && curr->execute_time <= n->execute_time)
        {
          prev = curr;
          curr = curr->next.load(std::memory_order_acquire);
        }
        n->next.store(curr, std::memory_order_relaxed);
        if (prev->next.compare_exchange_weak(curr, n, std::memory_order_release, std::memory_order_relaxed))
        {
          return;
        }
        // 若失败则重试
      }
    }

  public:
    explicit delay_task_queue(std::size_t max_size = 0) : _max_size(max_size)
    {
    }

    ~delay_task_queue() override
    {
      close();
      clear();
      stop_timer_thread();
    }

    bool push(std::shared_ptr<task_base> task) override
    {
      if (!task) return false;
      if (_closed.load(std::memory_order_acquire)) return false;

      auto max_cap = _max_size.load(std::memory_order_relaxed);
      if (max_cap != 0)
      {
        auto cur = size();
        if (cur >= max_cap)
        {
          return false;
        }
      }

      bool ok = _ready_queue.push(task);
      if (ok)
      {
        _ready_size.fetch_add(1, std::memory_order_acq_rel);
        _items.release();
      }
      return ok;
    }

    template <typename Rep, typename Period>
    bool push_delayed(std::shared_ptr<task_base> task, const std::chrono::duration<Rep, Period> &delay)
    {
      if (!task) return false;
      if (_closed.load(std::memory_order_acquire)) return false;

      auto max_cap = _max_size.load(std::memory_order_relaxed);
      if (max_cap != 0)
      {
        auto cur = size();
        if (cur >= max_cap)
        {
          return false;
        }
      }

      auto execute_time = std::chrono::steady_clock::now() + delay;
      auto n = std::make_shared<delayed_node>(std::move(task), execute_time);
      insert_delayed(n);
      _delayed_size.fetch_add(1, std::memory_order_acq_rel);
      start_timer_thread();
      return true;
    }

    std::shared_ptr<task_base> pop() override
    {
      while (true)
      {
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
        _waiters.fetch_add(1, std::memory_order_acq_rel);
        _items.acquire();
        _waiters.fetch_sub(1, std::memory_order_acq_rel);

        auto t = _ready_queue.try_pop();
        if (t)
        {
          _ready_size.fetch_sub(1, std::memory_order_acq_rel);
          return t;
        }
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
      }
    }

    std::shared_ptr<task_base> try_pop() override
    {
      if (_items.try_acquire())
      {
        auto t = _ready_queue.try_pop();
        if (t)
        {
          _ready_size.fetch_sub(1, std::memory_order_acq_rel);
          return t;
        }
      }
      return nullptr;
    }

    template <typename Rep, typename Period>
    std::shared_ptr<task_base> try_pop_for(const std::chrono::duration<Rep, Period> &timeout)
    {
      if (_items.try_acquire_for(timeout))
      {
        auto t = _ready_queue.try_pop();
        if (t)
        {
          _ready_size.fetch_sub(1, std::memory_order_acq_rel);
          return t;
        }
      }
      return nullptr;
    }

    std::size_t size() const override
    {
      return _ready_size.load(std::memory_order_acquire) + _delayed_size.load(std::memory_order_acquire);
    }

    bool empty() const override
    {
      return size() == 0;
    }

    void clear() override
    {
      while (true)
      {
        auto t = try_pop();
        if (!t) break;
      }
      // 清空延迟链表（无锁逐个摘除）
      while (true)
      {
        auto head = _delayed_head.load(std::memory_order_acquire);
        if (!head) break;
        auto next = head->next.load(std::memory_order_acquire);
        if (_delayed_head.compare_exchange_strong(head, next, std::memory_order_acq_rel))
        {
          _delayed_size.fetch_sub(1, std::memory_order_acq_rel);
        }
      }
    }

    void close() override
    {
      bool expected = false;
      if (_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      {
        stop_timer_thread();
        int waiters = _waiters.load(std::memory_order_acquire);
        if (waiters > 0)
        {
          _items.release(waiters);
        }
        _ready_queue.close();
      }
    }

    bool is_closed() const override
    {
      return _closed.load(std::memory_order_acquire);
    }

    queue_policy get_policy() const override
    {
      return queue_policy::delay;
    }

    void set_max_size(std::size_t max_size)
    {
      _max_size.store(max_size, std::memory_order_release);
    }

    std::size_t get_max_size() const
    {
      return _max_size.load(std::memory_order_acquire);
    }

    std::size_t get_delayed_task_count() const
    {
      return _delayed_size.load(std::memory_order_acquire);
    }

    std::size_t get_ready_task_count() const
    {
      return _ready_size.load(std::memory_order_acquire);
    }
  };

  // ================ 无锁 多级 任务队列 =================
  class multi_level_task_queue : public base_task_queue
  {
  private:
    struct queue_node
    {
      std::unique_ptr<base_task_queue> queue;
      std::atomic<std::shared_ptr<queue_node>> next{nullptr};
      explicit queue_node(std::unique_ptr<base_task_queue> q)
        : queue(std::move(q)) {}
    };

    std::atomic<std::shared_ptr<queue_node>> _head{nullptr};
    std::counting_semaphore<std::numeric_limits<int>::max()> _items{0};

    std::atomic<std::size_t> _size{0};
    std::atomic<bool> _closed{false};
    std::atomic<int> _waiters{0};
    queue_policy _policy{queue_policy::round_robin};

    std::atomic<std::size_t> _rr_index{0};

    std::size_t collect_queues(std::vector<base_task_queue *> &out) const
    {
      out.clear();
      auto cur = _head.load(std::memory_order_acquire);
      while (cur)
      {
        out.push_back(cur->queue.get());
        cur = cur->next.load(std::memory_order_acquire);
      }
      return out.size();
    }

    std::size_t sub_queue_count() const
    {
      std::size_t count = 0;
      auto cur = _head.load(std::memory_order_acquire);
      while (cur)
      {
        ++count;
        cur = cur->next.load(std::memory_order_acquire);
      }
      return count;
    }

    base_task_queue *get_queue_by_index(std::size_t index) const
    {
      auto cur = _head.load(std::memory_order_acquire);
      while (cur && index > 0)
      {
        cur = cur->next.load(std::memory_order_acquire);
        --index;
      }
      return cur ? cur->queue.get() : nullptr;
    }

  public:
    explicit multi_level_task_queue(queue_policy policy = queue_policy::round_robin)
      : _policy(policy)
    {
    }

    ~multi_level_task_queue() override
    {
      close();
      clear();
    }

    void add_sub_queue(std::unique_ptr<base_task_queue> queue)
    {
      if (!queue) return;
      auto n = std::make_shared<queue_node>(std::move(queue));
      while (true)
      {
        auto head = _head.load(std::memory_order_acquire);
        n->next.store(head, std::memory_order_relaxed);
        if (_head.compare_exchange_weak(head, n, std::memory_order_release, std::memory_order_relaxed))
        {
          break;
        }
      }
    }

    bool push(std::shared_ptr<task_base> task) override
    {
      if (!task) return false;
      if (_closed.load(std::memory_order_acquire)) return false;

      // 选择队列
      std::vector<base_task_queue *> queues;
      collect_queues(queues);
      if (queues.empty())
      {
        return false;
      }

      base_task_queue *target = nullptr;
      switch (_policy)
      {
      case queue_policy::round_robin:
      case queue_policy::fifo:
      case queue_policy::delay:
      case queue_policy::priority:
      default:
      {
        auto idx = _rr_index.fetch_add(1, std::memory_order_acq_rel);
        target = queues[idx % queues.size()];
        break;
      }
      }

      if (target && target->push(task))
      {
        _size.fetch_add(1, std::memory_order_acq_rel);
        _items.release();
        return true;
      }
      return false;
    }

    std::shared_ptr<task_base> pop() override
    {
      while (true)
      {
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
        _waiters.fetch_add(1, std::memory_order_acq_rel);
        _items.acquire();
        _waiters.fetch_sub(1, std::memory_order_acq_rel);

        std::vector<base_task_queue *> queues;
        collect_queues(queues);
        if (queues.empty())
        {
          return nullptr;
        }
        // 轮询尝试
        auto start = _rr_index.fetch_add(1, std::memory_order_acq_rel);
        for (std::size_t i = 0; i < queues.size(); ++i)
        {
          auto idx = (start + i) % queues.size();
          auto t = queues[idx]->try_pop();
          if (t)
          {
            _size.fetch_sub(1, std::memory_order_acq_rel);
            return t;
          }
        }
        if (_closed.load(std::memory_order_acquire) && empty())
        {
          return nullptr;
        }
      }
    }

    std::shared_ptr<task_base> try_pop() override
    {
      if (_items.try_acquire())
      {
        std::vector<base_task_queue *> queues;
        collect_queues(queues);
        if (!queues.empty())
        {
          auto start = _rr_index.fetch_add(1, std::memory_order_acq_rel);
          for (std::size_t i = 0; i < queues.size(); ++i)
          {
            auto idx = (start + i) % queues.size();
            auto t = queues[idx]->try_pop();
            if (t)
            {
              _size.fetch_sub(1, std::memory_order_acq_rel);
              return t;
            }
          }
        }
      }
      return nullptr;
    }

    template <typename Rep, typename Period>
    std::shared_ptr<task_base> try_pop_for(const std::chrono::duration<Rep, Period> &timeout)
    {
      if (_items.try_acquire_for(timeout))
      {
        std::vector<base_task_queue *> queues;
        collect_queues(queues);
        if (!queues.empty())
        {
          auto start = _rr_index.fetch_add(1, std::memory_order_acq_rel);
          for (std::size_t i = 0; i < queues.size(); ++i)
          {
            auto idx = (start + i) % queues.size();
            auto t = queues[idx]->try_pop();
            if (t)
            {
              _size.fetch_sub(1, std::memory_order_acq_rel);
              return t;
            }
          }
        }
      }
      return nullptr;
    }

    std::size_t size() const override
    {
      return _size.load(std::memory_order_acquire);
    }

    bool empty() const override
    {
      return size() == 0;
    }

    void clear() override
    {
      while (true)
      {
        auto t = try_pop();
        if (!t) break;
      }
    }

    void close() override
    {
      bool expected = false;
      if (_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      {
        int waiters = _waiters.load(std::memory_order_acquire);
        if (waiters > 0)
        {
          _items.release(waiters);
        }
        // 关闭所有子队列
        auto cur = _head.load(std::memory_order_acquire);
        while (cur)
        {
          cur->queue->close();
          cur = cur->next.load(std::memory_order_acquire);
        }
      }
    }

    bool is_closed() const override
    {
      return _closed.load(std::memory_order_acquire);
    }

    queue_policy get_policy() const override
    {
      return _policy;
    }

    // 非虚接口：与 queue.hpp 保持一致
    std::size_t get_sub_queue_count() const
    {
      return sub_queue_count();
    }

    std::size_t get_sub_queue_size(std::size_t index) const
    {
      auto *q = get_queue_by_index(index);
      return q ? q->size() : 0;
    }
  };

  // 工厂方法（与 queue.hpp 一致的命名与签名）
  inline std::unique_ptr<base_task_queue> make_fifo_queue(std::size_t max_size = 0)
  {
    return std::make_unique<fifo_task_queue>(max_size);
  }

  inline std::unique_ptr<base_task_queue> make_priority_queue(std::size_t max_size = 0)
  {
    return std::make_unique<priority_task_queue>(max_size);
  }

  inline std::unique_ptr<base_task_queue> make_delay_queue(std::size_t max_size = 0)
  {
    return std::make_unique<delay_task_queue>(max_size);
  }

  inline std::unique_ptr<base_task_queue> make_multi_level_queue(queue_policy policy = queue_policy::round_robin)
  {
    return std::make_unique<multi_level_task_queue>(policy);
  }

  inline std::unique_ptr<base_task_queue> make_task_queue(queue_policy policy, std::size_t max_size = 0)
  {
    switch (policy)
    {
    case queue_policy::fifo:
      return make_fifo_queue(max_size);
    case queue_policy::priority:
      return make_priority_queue(max_size);
    case queue_policy::delay:
      return make_delay_queue(max_size);
    case queue_policy::round_robin:
    default:
      return make_multi_level_queue(queue_policy::round_robin);
    }
  }
} // namespace thread_pool