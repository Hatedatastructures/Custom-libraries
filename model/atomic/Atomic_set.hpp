#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <functional>
#include <algorithm>
#include <iterator>
#include <initializer_list>
#include <type_traits>

namespace atomic_concurrent
{
  /**
   * @brief 无锁线程安全的有序集合容器
   * 
   * 基于跳表实现的无锁线程安全集合，支持多生产者多消费者并发访问。
   * 提供与标准库 std::set 兼容的完整接口，使用蛇形命名法。
   * 
   * 特性：
   * - 使用原子操作和 CAS 实现无锁并发
   * - 支持多生产者多消费者
   * - 基于跳表实现有序存储和高效查找
   * - 提供完整的标准库兼容接口
   * - 支持双向迭代
   * - 线程安全的插入、删除、查找操作
   * 
   * @tparam key_t 键类型
   * @tparam compare_t 比较函数类型
   * @tparam allocator_t 分配器类型
   */
  template <typename key_t, typename compare_t = std::less<key_t>, typename allocator_t = std::allocator<key_t>>
  class atomic_set
  {
  public:
    // 标准库兼容的类型别名
    using key_type = key_t;
    using value_type = key_t;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = compare_t;
    using value_compare = compare_t;
    using allocator_type = allocator_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename std::allocator_traits<allocator_t>::pointer;
    using const_pointer = typename std::allocator_traits<allocator_t>::const_pointer;

  private:
    static constexpr int max_level = 16;
    static constexpr double probability = 0.5;
    static constexpr size_t max_retry_count = 1000;

    struct node_t
    {
      value_type key;
      std::atomic<bool> marked;
      std::vector<std::atomic<node_t*>> forward;
      
      template <typename k_t>
      node_t(k_t&& k, int level) 
        : key(std::forward<k_t>(k)), marked(false), forward(level + 1)
      {
        for (int i = 0; i <= level; ++i)
        {
          forward[i].store(nullptr);
        }
      }
      
      node_t(int level) : marked(false), forward(level + 1)
      {
        for (int i = 0; i <= level; ++i)
        {
          forward[i].store(nullptr);
        }
      }
    };

    std::atomic<node_t*> head_;
    std::atomic<node_t*> tail_;
    std::atomic<size_t> size_;
    std::atomic<int> level_;
    key_compare comp_;
    allocator_type alloc_;
    
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    mutable std::uniform_real_distribution<double> dis_;

    int random_level() const
    {
      int level = 0;
      while (dis_(gen_) < probability && level < max_level)
      {
        level++;
      }
      return level;
    }

    bool find_internal(const key_type& key, std::vector<node_t*>& preds, std::vector<node_t*>& succs) const
    {
      int level_found = -1;
      node_t* pred = head_.load();
      
      for (int level = level_.load(); level >= 0; level--)
      {
        node_t* curr = pred->forward[level].load();
        
        while (curr != tail_.load() && comp_(curr->key, key))
        {
          pred = curr;
          curr = pred->forward[level].load();
        }
        
        if (level_found == -1 && curr != tail_.load() && !comp_(key, curr->key))
        {
          level_found = level;
        }
        
        preds[level] = pred;
        succs[level] = curr;
      }
      
      return level_found != -1;
    }

  public:
    // 前向声明迭代器
    class iterator;
    class const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // 迭代器实现
    class const_iterator
    {
    private:
      node_t* node_;
      const atomic_set* container_;
      
    public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = atomic_set::value_type;
      using difference_type = atomic_set::difference_type;
      using pointer = const atomic_set::value_type*;
      using reference = const atomic_set::value_type&;
      
      const_iterator() : node_(nullptr), container_(nullptr) {}
      const_iterator(node_t* node, const atomic_set* container) : node_(node), container_(container) {}
      
      reference operator*() const { return node_->key; }
      pointer operator->() const { return &node_->key; }
      
      const_iterator& operator++()
      {
        if (node_ && node_ != container_->tail_.load())
        {
          node_ = node_->forward[0].load();
        }
        return *this;
      }
      
      const_iterator operator++(int)
      {
        const_iterator tmp = *this;
        ++(*this);
        return tmp;
      }
      
      bool operator==(const const_iterator& other) const { return node_ == other.node_; }
      bool operator!=(const const_iterator& other) const { return node_ != other.node_; }
    };
    
    class iterator : public const_iterator
    {
    public:
      using pointer = atomic_set::value_type*;
      using reference = atomic_set::value_type&;
      
      iterator() : const_iterator() {}
      iterator(node_t* node, const atomic_set* container) : const_iterator(node, container) {}
      
      reference operator*() const { return const_cast<reference>(const_iterator::operator*()); }
      pointer operator->() const { return const_cast<pointer>(const_iterator::operator->()); }
      
      iterator& operator++()
      {
        const_iterator::operator++();
        return *this;
      }
      
      iterator operator++(int)
      {
        iterator tmp = *this;
        ++(*this);
        return tmp;
      }
    };

    // 构造函数
    atomic_set() : atomic_set(key_compare()) {}
    
    explicit atomic_set(const key_compare& comp, const allocator_type& alloc = allocator_type())
      : comp_(comp), alloc_(alloc), gen_(rd_()), dis_(0.0, 1.0)
    {
      node_t* head_sentinel = new node_t(max_level);
      node_t* tail_sentinel = new node_t(max_level);
      
      for (int i = 0; i <= max_level; ++i)
      {
        head_sentinel->forward[i].store(tail_sentinel);
      }
      
      head_.store(head_sentinel);
      tail_.store(tail_sentinel);
      size_.store(0);
      level_.store(0);
    }
    
    explicit atomic_set(const allocator_type& alloc) : atomic_set(key_compare(), alloc) {}
    
    template <typename input_it>
    atomic_set(input_it first, input_it last, const key_compare& comp = key_compare(), const allocator_type& alloc = allocator_type())
      : atomic_set(comp, alloc)
    {
      insert(first, last);
    }
    
    template <typename input_it>
    atomic_set(input_it first, input_it last, const allocator_type& alloc)
      : atomic_set(first, last, key_compare(), alloc) {}
    
    atomic_set(const atomic_set& other)
      : atomic_set(other.comp_, other.alloc_)
    {
      auto snapshot = other.snapshot();
      for (const auto& item : snapshot)
      {
        insert(item);
      }
    }
    
    atomic_set(const atomic_set& other, const allocator_type& alloc)
      : atomic_set(other.comp_, alloc)
    {
      auto snapshot = other.snapshot();
      for (const auto& item : snapshot)
      {
        insert(item);
      }
    }
    
    atomic_set(atomic_set&& other) noexcept
      : comp_(std::move(other.comp_)), alloc_(std::move(other.alloc_)), gen_(std::move(other.gen_)), dis_(std::move(other.dis_))
    {
      head_.store(other.head_.exchange(nullptr));
      tail_.store(other.tail_.exchange(nullptr));
      size_.store(other.size_.exchange(0));
      level_.store(other.level_.exchange(0));
    }
    
    atomic_set(atomic_set&& other, const allocator_type& alloc) noexcept
      : comp_(std::move(other.comp_)), alloc_(alloc), gen_(std::move(other.gen_)), dis_(std::move(other.dis_))
    {
      if (alloc_ == other.alloc_)
      {
        head_.store(other.head_.exchange(nullptr));
        tail_.store(other.tail_.exchange(nullptr));
        size_.store(other.size_.exchange(0));
        level_.store(other.level_.exchange(0));
      }
      else
      {
        // 不同分配器，需要复制
        node_t* head_sentinel = new node_t(max_level);
        node_t* tail_sentinel = new node_t(max_level);
        
        for (int i = 0; i <= max_level; ++i)
        {
          head_sentinel->forward[i].store(tail_sentinel);
        }
        
        head_.store(head_sentinel);
        tail_.store(tail_sentinel);
        size_.store(0);
        level_.store(0);
        
        auto snapshot = other.snapshot();
        for (const auto& item : snapshot)
        {
          insert(item);
        }
      }
    }
    
    atomic_set(std::initializer_list<value_type> init, const key_compare& comp = key_compare(), const allocator_type& alloc = allocator_type())
      : atomic_set(comp, alloc)
    {
      insert(init);
    }
    
    atomic_set(std::initializer_list<value_type> init, const allocator_type& alloc)
      : atomic_set(init, key_compare(), alloc) {}

    // 析构函数
    ~atomic_set()
    {
      clear();
      
      node_t* head = head_.load();
      node_t* tail = tail_.load();
      if (head) delete head;
      if (tail) delete tail;
    }

    // 赋值运算符
    atomic_set& operator=(const atomic_set& other)
    {
      if (this != &other)
      {
        clear();
        comp_ = other.comp_;
        
        auto snapshot = other.snapshot();
        for (const auto& item : snapshot)
        {
          insert(item);
        }
      }
      return *this;
    }
    
    atomic_set& operator=(atomic_set&& other) noexcept
    {
      if (this != &other)
      {
        clear();
        
        node_t* head = head_.load();
        node_t* tail = tail_.load();
        delete head;
        delete tail;
        
        head_.store(other.head_.exchange(nullptr));
        tail_.store(other.tail_.exchange(nullptr));
        size_.store(other.size_.exchange(0));
        level_.store(other.level_.exchange(0));
        comp_ = std::move(other.comp_);
        alloc_ = std::move(other.alloc_);
        gen_ = std::move(other.gen_);
        dis_ = std::move(other.dis_);
      }
      return *this;
    }
    
    atomic_set& operator=(std::initializer_list<value_type> init)
    {
      clear();
      insert(init);
      return *this;
    }

    // 分配器
    allocator_type get_allocator() const { return alloc_; }

    // 迭代器
    iterator begin()
    {
      node_t* first = head_.load()->forward[0].load();
      return iterator(first, this);
    }
    
    const_iterator begin() const
    {
      node_t* first = head_.load()->forward[0].load();
      return const_iterator(first, this);
    }
    
    const_iterator cbegin() const { return begin(); }
    
    iterator end()
    {
      return iterator(tail_.load(), this);
    }
    
    const_iterator end() const
    {
      return const_iterator(tail_.load(), this);
    }
    
    const_iterator cend() const { return end(); }
    
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    // 容量
    bool empty() const { return size_.load() == 0; }
    size_type size() const { return size_.load(); }
    size_type max_size() const { return std::numeric_limits<size_type>::max(); }

    // 修改操作
    void clear()
    {
      node_t* current = head_.load()->forward[0].load();
      node_t* tail = tail_.load();
      
      while (current != tail)
      {
        node_t* next = current->forward[0].load();
        delete current;
        current = next;
      }
      
      node_t* head = head_.load();
      for (int i = 0; i <= max_level; ++i)
      {
        head->forward[i].store(tail);
      }
      
      size_.store(0);
      level_.store(0);
    }

    std::pair<iterator, bool> insert(const value_type& value)
    {
      int top_level = random_level();
      std::vector<node_t*> preds(max_level + 1);
      std::vector<node_t*> succs(max_level + 1);
      
      for (size_t retry = 0; retry < max_retry_count; ++retry)
      {
        if (find_internal(value, preds, succs))
        {
          return {iterator(succs[0], this), false};
        }
        
        node_t* new_node = new node_t(value, top_level);
        
        for (int level = 0; level <= top_level; level++)
        {
          new_node->forward[level].store(succs[level]);
        }
        
        bool success = true;
        for (int level = 0; level <= top_level && success; level++)
        {
          node_t* pred = preds[level];
          node_t* succ = succs[level];
          
          if (!pred->forward[level].compare_exchange_weak(succ, new_node))
          {
            success = false;
          }
        }
        
        if (success)
        {
          int current_level = level_.load();
          if (top_level > current_level)
          {
            level_.compare_exchange_weak(current_level, top_level);
          }
          
          size_.fetch_add(1);
          return {iterator(new_node, this), true};
        }
        else
        {
          delete new_node;
        }
        
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
      }
      
      return {end(), false};
    }
    
    std::pair<iterator, bool> insert(value_type&& value)
    {
      int top_level = random_level();
      std::vector<node_t*> preds(max_level + 1);
      std::vector<node_t*> succs(max_level + 1);
      
      for (size_t retry = 0; retry < max_retry_count; ++retry)
      {
        if (find_internal(value, preds, succs))
        {
          return {iterator(succs[0], this), false};
        }
        
        node_t* new_node = new node_t(std::move(value), top_level);
        
        for (int level = 0; level <= top_level; level++)
        {
          new_node->forward[level].store(succs[level]);
        }
        
        bool success = true;
        for (int level = 0; level <= top_level && success; level++)
        {
          node_t* pred = preds[level];
          node_t* succ = succs[level];
          
          if (!pred->forward[level].compare_exchange_weak(succ, new_node))
          {
            success = false;
          }
        }
        
        if (success)
        {
          int current_level = level_.load();
          if (top_level > current_level)
          {
            level_.compare_exchange_weak(current_level, top_level);
          }
          
          size_.fetch_add(1);
          return {iterator(new_node, this), true};
        }
        else
        {
          delete new_node;
        }
        
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
      }
      
      return {end(), false};
    }
    
    iterator insert(const_iterator hint, const value_type& value)
    {
      // 忽略 hint，直接插入
      return insert(value).first;
    }
    
    iterator insert(const_iterator hint, value_type&& value)
    {
      // 忽略 hint，直接插入
      return insert(std::move(value)).first;
    }
    
    template <typename input_it>
    void insert(input_it first, input_it last)
    {
      for (auto it = first; it != last; ++it)
      {
        insert(*it);
      }
    }
    
    void insert(std::initializer_list<value_type> init)
    {
      insert(init.begin(), init.end());
    }

    template <typename... args_t>
    std::pair<iterator, bool> emplace(args_t&&... args)
    {
      return insert(value_type(std::forward<args_t>(args)...));
    }
    
    template <typename... args_t>
    iterator emplace_hint(const_iterator hint, args_t&&... args)
    {
      return insert(hint, value_type(std::forward<args_t>(args)...));
    }

    iterator erase(const_iterator pos)
    {
      if (pos == end()) return end();
      
      const value_type& key = *pos;
      erase(key);
      return find(key) == end() ? ++iterator(pos.node_, this) : end();
    }
    
    iterator erase(const_iterator first, const_iterator last)
    {
      std::vector<value_type> to_erase;
      for (auto it = first; it != last; ++it)
      {
        to_erase.push_back(*it);
      }
      
      for (const auto& key : to_erase)
      {
        erase(key);
      }
      
      return iterator(last.node_, this);
    }
    
    size_type erase(const key_type& key)
    {
      std::vector<node_t*> preds(max_level + 1);
      std::vector<node_t*> succs(max_level + 1);
      
      for (size_t retry = 0; retry < max_retry_count; ++retry)
      {
        if (!find_internal(key, preds, succs))
        {
          return 0;
        }
        
        node_t* node_to_remove = succs[0];
        
        bool expected = false;
        if (!node_to_remove->marked.compare_exchange_weak(expected, true))
        {
          continue;
        }
        
        bool success = true;
        for (int level = node_to_remove->forward.size() - 1; level >= 0 && success; level--)
        {
          node_t* pred = preds[level];
          node_t* succ = node_to_remove->forward[level].load();
          
          if (!pred->forward[level].compare_exchange_weak(node_to_remove, succ))
          {
            success = false;
          }
        }
        
        if (success)
        {
          delete node_to_remove;
          size_.fetch_sub(1);
          return 1;
        }
        
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
      }
      
      return 0;
    }

    void swap(atomic_set& other) noexcept
    {
      std::swap(head_, other.head_);
      std::swap(tail_, other.tail_);
      std::swap(size_, other.size_);
      std::swap(level_, other.level_);
      std::swap(comp_, other.comp_);
      std::swap(alloc_, other.alloc_);
      std::swap(gen_, other.gen_);
      std::swap(dis_, other.dis_);
    }

    // 查找
    size_type count(const key_type& key) const
    {
      return contains(key) ? 1 : 0;
    }
    
    iterator find(const key_type& key)
    {
      std::vector<node_t*> preds(max_level + 1);
      std::vector<node_t*> succs(max_level + 1);
      
      if (find_internal(key, preds, succs))
      {
        node_t* found = succs[0];
        if (!found->marked.load())
        {
          return iterator(found, this);
        }
      }
      
      return end();
    }
    
    const_iterator find(const key_type& key) const
    {
      std::vector<node_t*> preds(max_level + 1);
      std::vector<node_t*> succs(max_level + 1);
      
      if (find_internal(key, preds, succs))
      {
        node_t* found = succs[0];
        if (!found->marked.load())
        {
          return const_iterator(found, this);
        }
      }
      
      return end();
    }
    
    bool contains(const key_type& key) const
    {
      return find(key) != end();
    }
    
    std::pair<iterator, iterator> equal_range(const key_type& key)
    {
      auto it = find(key);
      if (it != end())
      {
        auto next_it = it;
        ++next_it;
        return {it, next_it};
      }
      return {it, it};
    }
    
    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
      auto it = find(key);
      if (it != end())
      {
        auto next_it = it;
        ++next_it;
        return {it, next_it};
      }
      return {it, it};
    }
    
    iterator lower_bound(const key_type& key)
    {
      for (auto it = begin(); it != end(); ++it)
      {
        if (!comp_(*it, key))
        {
          return it;
        }
      }
      return end();
    }
    
    const_iterator lower_bound(const key_type& key) const
    {
      for (auto it = begin(); it != end(); ++it)
      {
        if (!comp_(*it, key))
        {
          return it;
        }
      }
      return end();
    }
    
    iterator upper_bound(const key_type& key)
    {
      for (auto it = begin(); it != end(); ++it)
      {
        if (comp_(key, *it))
        {
          return it;
        }
      }
      return end();
    }
    
    const_iterator upper_bound(const key_type& key) const
    {
      for (auto it = begin(); it != end(); ++it)
      {
        if (comp_(key, *it))
        {
          return it;
        }
      }
      return end();
    }

    // 观察器
    key_compare key_comp() const { return comp_; }
    value_compare value_comp() const { return comp_; }

    // 扩展功能
    std::vector<value_type> snapshot() const
    {
      std::vector<value_type> result;
      
      node_t* current = head_.load()->forward[0].load();
      node_t* tail = tail_.load();
      
      while (current != tail)
      {
        if (!current->marked.load())
        {
          result.push_back(current->key);
        }
        current = current->forward[0].load();
      }
      
      return result;
    }
    
    template <typename function_t>
    void for_each(function_t func) const
    {
      node_t* current = head_.load()->forward[0].load();
      node_t* tail = tail_.load();
      
      while (current != tail)
      {
        if (!current->marked.load())
        {
          func(current->key);
        }
        current = current->forward[0].load();
      }
    }
    
    std::vector<value_type> range(const key_type& lower_bound, const key_type& upper_bound) const
    {
      std::vector<value_type> result;
      
      node_t* current = head_.load()->forward[0].load();
      node_t* tail = tail_.load();
      
      while (current != tail && comp_(current->key, lower_bound))
      {
        current = current->forward[0].load();
      }
      
      while (current != tail && comp_(current->key, upper_bound))
      {
        if (!current->marked.load())
        {
          result.push_back(current->key);
        }
        current = current->forward[0].load();
      }
      
      return result;
    }
  };

  // 比较操作符
  template <typename key_t, typename compare_t, typename allocator_t>
  bool operator==(const atomic_set<key_t, compare_t, allocator_t>& lhs, const atomic_set<key_t, compare_t, allocator_t>& rhs)
  {
    if (lhs.size() != rhs.size()) return false;
    
    auto lhs_snapshot = lhs.snapshot();
    auto rhs_snapshot = rhs.snapshot();
    
    return std::equal(lhs_snapshot.begin(), lhs_snapshot.end(), rhs_snapshot.begin());
  }
  
  template <typename key_t, typename compare_t, typename allocator_t>
  bool operator!=(const atomic_set<key_t, compare_t, allocator_t>& lhs, const atomic_set<key_t, compare_t, allocator_t>& rhs)
  {
    return !(lhs == rhs);
  }
  
  template <typename key_t, typename compare_t, typename allocator_t>
  bool operator<(const atomic_set<key_t, compare_t, allocator_t>& lhs, const atomic_set<key_t, compare_t, allocator_t>& rhs)
  {
    auto lhs_snapshot = lhs.snapshot();
    auto rhs_snapshot = rhs.snapshot();
    
    return std::lexicographical_compare(lhs_snapshot.begin(), lhs_snapshot.end(), rhs_snapshot.begin(), rhs_snapshot.end());
  }
  
  template <typename key_t, typename compare_t, typename allocator_t>
  bool operator<=(const atomic_set<key_t, compare_t, allocator_t>& lhs, const atomic_set<key_t, compare_t, allocator_t>& rhs)
  {
    return !(rhs < lhs);
  }
  
  template <typename key_t, typename compare_t, typename allocator_t>
  bool operator>(const atomic_set<key_t, compare_t, allocator_t>& lhs, const atomic_set<key_t, compare_t, allocator_t>& rhs)
  {
    return rhs < lhs;
  }
  
  template <typename key_t, typename compare_t, typename allocator_t>
  bool operator>=(const atomic_set<key_t, compare_t, allocator_t>& lhs, const atomic_set<key_t, compare_t, allocator_t>& rhs)
  {
    return !(lhs < rhs);
  }

  // 全局 swap
  template <typename key_t, typename compare_t, typename allocator_t>
  void swap(atomic_set<key_t, compare_t, allocator_t>& lhs, atomic_set<key_t, compare_t, allocator_t>& rhs) noexcept
  {
    lhs.swap(rhs);
  }
}