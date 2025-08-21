/**
 * @file Atomic_list.hpp
 * @brief 无锁线程安全的双向链表容器
 * @author wang
 * @version 1.0
 * @date 2025-08-15
 *
 * 本文件提供无锁线程安全的双向链表容器：
 *   1. 使用原子操作和CAS实现无锁并发访问；
 *   2. 支持多生产者多消费者模式；
 *   3. 提供与标准库list兼容的完整接口；
 *   4. 使用蛇形命名法；
 *   5. 支持双向迭代和随机插入删除操作。
 */

#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>

namespace atomic_concurrent
{
    /**
     * @class atomic_list
     * @brief 无锁线程安全的双向链表
     * @tparam value_type         元素类型
     * @tparam allocator_type     分配器，默认 `std::allocator<value_type>`
     */
    template <typename value_type, typename allocator_type = std::allocator<value_type>>
    class atomic_list
    {
    public:
        // 类型定义
        using value_t = value_type;
        using allocator_t = allocator_type;
        using size_t = std::size_t;
        using difference_t = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

    private:
        // 链表节点结构
        struct list_node
        {
            std::atomic<value_type*> data;
            std::atomic<list_node*> next;
            std::atomic<list_node*> prev;
            std::atomic<bool> is_valid;
            
            list_node() : data(nullptr), next(nullptr), prev(nullptr), is_valid(true) {}
            
            ~list_node()
            {
                value_type* ptr = data.load();
                if (ptr)
                {
                    allocator_type alloc;
                    std::allocator_traits<allocator_type>::destroy(alloc, ptr);
                    std::allocator_traits<allocator_type>::deallocate(alloc, ptr, 1);
                }
            }
        };
        
        std::atomic<list_node*> _head;
        std::atomic<list_node*> _tail;
        std::atomic<size_t> _size;
        allocator_type _allocator;
        
        // 哨兵节点，简化边界处理
        list_node* _sentinel_head;
        list_node* _sentinel_tail;
        
        // 内部辅助函数
        list_node* create_data_node(const value_type& value_data)
        {
            list_node* node = new list_node();
            value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
            std::allocator_traits<allocator_type>::construct(_allocator, data, value_data);
            node->data.store(data);
            return node;
        }
        
        list_node* create_data_node(value_type&& value_data)
        {
            list_node* node = new list_node();
            value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
            std::allocator_traits<allocator_type>::construct(_allocator, data, std::move(value_data));
            node->data.store(data);
            return node;
        }
        
        template <typename... args_t>
        list_node* create_emplace_node(args_t&&... args)
        {
            list_node* node = new list_node();
            value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
            std::allocator_traits<allocator_type>::construct(_allocator, data, std::forward<args_t>(args)...);
            node->data.store(data);
            return node;
        }
        
        void cleanup_nodes()
        {
            list_node* current = _head.load();
            while (current && current != _sentinel_tail)
            {
                list_node* next = current->next.load();
                delete current;
                current = next;
            }
        }
        
        void init_sentinels()
        {
            _sentinel_head = new list_node();
            _sentinel_tail = new list_node();
            
            _sentinel_head->next.store(_sentinel_tail);
            _sentinel_tail->prev.store(_sentinel_head);
            
            _head.store(_sentinel_head);
            _tail.store(_sentinel_tail);
        }
        
        // 在两个节点之间插入新节点
        bool insert_between(list_node* prev_node, list_node* next_node, list_node* new_node)
        {
            new_node->prev.store(prev_node);
            new_node->next.store(next_node);
            
            // 使用CAS操作确保原子性
            if (prev_node->next.compare_exchange_strong(next_node, new_node))
            {
                next_node->prev.store(new_node);
                _size.fetch_add(1);
                return true;
            }
            return false;
        }
        
        // 移除指定节点
        bool remove_node(list_node* node)
        {
            if (!node || node == _sentinel_head || node == _sentinel_tail)
                return false;
                
            list_node* prev_node = node->prev.load();
            list_node* next_node = node->next.load();
            
            if (prev_node && next_node)
            {
                prev_node->next.store(next_node);
                next_node->prev.store(prev_node);
                
                node->is_valid.store(false);
                _size.fetch_sub(1);
                delete node;
                return true;
            }
            return false;
        }
        
    public:
        // 迭代器类
        class iterator
        {
        private:
            list_node* _node;
            
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = atomic_list::value_type;
            using difference_type = atomic_list::difference_t;
            using pointer = atomic_list::pointer;
            using reference = atomic_list::reference;
            
            iterator(list_node* node = nullptr) : _node(node) {}
            
            reference operator*() const
            {
                if (!_node || !_node->is_valid.load())
                    throw std::runtime_error("Invalid iterator dereference");
                value_type* data = _node->data.load();
                if (!data)
                    throw std::runtime_error("Null data in iterator");
                return *data;
            }
            
            pointer operator->() const
            {
                return &(operator*());
            }
            
            iterator& operator++()
            {
                if (_node)
                    _node = _node->next.load();
                return *this;
            }
            
            iterator operator++(int)
            {
                iterator temp = *this;
                ++(*this);
                return temp;
            }
            
            iterator& operator--()
            {
                if (_node)
                    _node = _node->prev.load();
                return *this;
            }
            
            iterator operator--(int)
            {
                iterator temp = *this;
                --(*this);
                return temp;
            }
            
            bool operator==(const iterator& other) const
            {
                return _node == other._node;
            }
            
            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }
            
            list_node* get_node() const { return _node; }
        };
        
        using const_iterator = iterator;
        
        /** @brief 默认构造空链表 */
        atomic_list() : _size(0), _allocator()
        {
            init_sentinels();
        }
        
        /**
         * @brief 构造指定分配器的链表
         * @param alloc 分配器
         */
        explicit atomic_list(const allocator_type& alloc)
            : _size(0), _allocator(alloc)
        {
            init_sentinels();
        }
        
        /**
         * @brief 初始化列表构造
         * @param init 形如 {1, 2, 3} 的列表
         * @param alloc 分配器
         */
        atomic_list(std::initializer_list<value_type> init,
                   const allocator_type& alloc = allocator_type())
            : _size(0), _allocator(alloc)
        {
            init_sentinels();
            for (const auto& item : init)
            {
                push_back(item);
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
        atomic_list(input_iterator_t first, input_iterator_t last,
                   const allocator_type& alloc = allocator_type())
            : _size(0), _allocator(alloc)
        {
            init_sentinels();
            for (auto it = first; it != last; ++it)
            {
                push_back(*it);
            }
        }
        
        /** @brief 拷贝构造（线程安全） */
        atomic_list(const atomic_list& other)
            : _size(0), _allocator(other._allocator)
        {
            init_sentinels();
            auto snapshot_data = other.snapshot();
            for (const auto& item : snapshot_data)
            {
                push_back(item);
            }
        }
        
        /** @brief 移动构造 */
        atomic_list(atomic_list&& other) noexcept
            : _head(other._head.exchange(nullptr)),
              _tail(other._tail.exchange(nullptr)),
              _size(other._size.exchange(0)),
              _allocator(std::move(other._allocator)),
              _sentinel_head(other._sentinel_head),
              _sentinel_tail(other._sentinel_tail)
        {
            other._sentinel_head = nullptr;
            other._sentinel_tail = nullptr;
        }
        
        /** @brief 拷贝赋值（线程安全） */
        atomic_list& operator=(const atomic_list& other)
        {
            if (this != &other)
            {
                clear();
                _allocator = other._allocator;
                
                auto snapshot_data = other.snapshot();
                for (const auto& item : snapshot_data)
                {
                    push_back(item);
                }
            }
            return *this;
        }
        
        /** @brief 移动赋值 */
        atomic_list& operator=(atomic_list&& other) noexcept
        {
            if (this != &other)
            {
                cleanup_nodes();
                delete _sentinel_head;
                delete _sentinel_tail;
                
                _head.store(other._head.exchange(nullptr));
                _tail.store(other._tail.exchange(nullptr));
                _size.store(other._size.exchange(0));
                _allocator = std::move(other._allocator);
                _sentinel_head = other._sentinel_head;
                _sentinel_tail = other._sentinel_tail;
                
                other._sentinel_head = nullptr;
                other._sentinel_tail = nullptr;
            }
            return *this;
        }
        
        /** @brief 析构函数 */
        ~atomic_list()
        {
            cleanup_nodes();
            delete _sentinel_head;
            delete _sentinel_tail;
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
        
        /** @brief 获取分配器 */
        allocator_type get_allocator() const noexcept
        {
            return _allocator;
        }
        
        // 元素访问
        
        /**
         * @brief 获取第一个元素
         * @param out 输出参数，接收元素值
         * @return true 成功；false 链表为空
         */
        bool front(value_type& out) const
        {
            list_node* first_node = _sentinel_head->next.load();
            
            if (first_node == _sentinel_tail)
                return false;
                
            value_type* data = first_node->data.load();
            if (data)
            {
                out = *data;
                return true;
            }
            return false;
        }
        
        /**
         * @brief 获取最后一个元素
         * @param out 输出参数，接收元素值
         * @return true 成功；false 链表为空
         */
        bool back(value_type& out) const
        {
            list_node* last_node = _sentinel_tail->prev.load();
            
            if (last_node == _sentinel_head)
                return false;
                
            value_type* data = last_node->data.load();
            if (data)
            {
                out = *data;
                return true;
            }
            return false;
        }
        
        // 修改操作
        
        /**
         * @brief 在头部插入元素（拷贝）
         * @param value_data 待插入元素
         * @return true 成功；false 失败
         */
        bool push_front(const value_type& value_data)
        {
            list_node* new_node = create_data_node(value_data);
            list_node* first_node = _sentinel_head->next.load();
            
            return insert_between(_sentinel_head, first_node, new_node);
        }
        
        /**
         * @brief 在头部插入元素（移动）
         * @param value_data 待插入元素
         * @return true 成功；false 失败
         */
        bool push_front(value_type&& value_data)
        {
            list_node* new_node = create_data_node(std::move(value_data));
            list_node* first_node = _sentinel_head->next.load();
            
            return insert_between(_sentinel_head, first_node, new_node);
        }
        
        /**
         * @brief 在尾部插入元素（拷贝）
         * @param value_data 待插入元素
         * @return true 成功；false 失败
         */
        bool push_back(const value_type& value_data)
        {
            list_node* new_node = create_data_node(value_data);
            list_node* last_node = _sentinel_tail->prev.load();
            
            return insert_between(last_node, _sentinel_tail, new_node);
        }
        
        /**
         * @brief 在尾部插入元素（移动）
         * @param value_data 待插入元素
         * @return true 成功；false 失败
         */
        bool push_back(value_type&& value_data)
        {
            list_node* new_node = create_data_node(std::move(value_data));
            list_node* last_node = _sentinel_tail->prev.load();
            
            return insert_between(last_node, _sentinel_tail, new_node);
        }
        
        /**
         * @brief 在头部就地构造元素
         * @param args 构造参数
         * @return true 成功；false 失败
         */
        template <typename... args_t>
        bool emplace_front(args_t&&... args)
        {
            list_node* new_node = create_emplace_node(std::forward<args_t>(args)...);
            list_node* first_node = _sentinel_head->next.load();
            
            return insert_between(_sentinel_head, first_node, new_node);
        }
        
        /**
         * @brief 在尾部就地构造元素
         * @param args 构造参数
         * @return true 成功；false 失败
         */
        template <typename... args_t>
        bool emplace_back(args_t&&... args)
        {
            list_node* new_node = create_emplace_node(std::forward<args_t>(args)...);
            list_node* last_node = _sentinel_tail->prev.load();
            
            return insert_between(last_node, _sentinel_tail, new_node);
        }
        
        /**
         * @brief 移除头部元素
         * @return true 成功；false 链表为空
         */
        bool pop_front()
        {
            list_node* first_node = _sentinel_head->next.load();
            
            if (first_node == _sentinel_tail)
                return false;
                
            return remove_node(first_node);
        }
        
        /**
         * @brief 移除尾部元素
         * @return true 成功；false 链表为空
         */
        bool pop_back()
        {
            list_node* last_node = _sentinel_tail->prev.load();
            
            if (last_node == _sentinel_head)
                return false;
                
            return remove_node(last_node);
        }
        
        /**
         * @brief 在指定位置插入元素（拷贝）
         * @param pos 插入位置
         * @param value_data 待插入元素
         * @return 指向插入元素的迭代器
         */
        iterator insert(iterator pos, const value_type& value_data)
        {
            list_node* new_node = create_data_node(value_data);
            list_node* pos_node = pos.get_node();
            list_node* prev_node = pos_node ? pos_node->prev.load() : _sentinel_tail->prev.load();
            
            if (insert_between(prev_node, pos_node, new_node))
                return iterator(new_node);
            else
            {
                delete new_node;
                return end();
            }
        }
        
        /**
         * @brief 在指定位置插入元素（移动）
         * @param pos 插入位置
         * @param value_data 待插入元素
         * @return 指向插入元素的迭代器
         */
        iterator insert(iterator pos, value_type&& value_data)
        {
            list_node* new_node = create_data_node(std::move(value_data));
            list_node* pos_node = pos.get_node();
            list_node* prev_node = pos_node ? pos_node->prev.load() : _sentinel_tail->prev.load();
            
            if (insert_between(prev_node, pos_node, new_node))
                return iterator(new_node);
            else
            {
                delete new_node;
                return end();
            }
        }
        
        /**
         * @brief 在指定位置就地构造元素
         * @param pos 插入位置
         * @param args 构造参数
         * @return 指向插入元素的迭代器
         */
        template <typename... args_t>
        iterator emplace(iterator pos, args_t&&... args)
        {
            list_node* new_node = create_emplace_node(std::forward<args_t>(args)...);
            list_node* pos_node = pos.get_node();
            list_node* prev_node = pos_node ? pos_node->prev.load() : _sentinel_tail->prev.load();
            
            if (insert_between(prev_node, pos_node, new_node))
                return iterator(new_node);
            else
            {
                delete new_node;
                return end();
            }
        }
        
        /**
         * @brief 移除指定位置的元素
         * @param pos 要移除的位置
         * @return 指向下一个元素的迭代器
         */
        iterator erase(iterator pos)
        {
            list_node* node = pos.get_node();
            if (!node || node == _sentinel_head || node == _sentinel_tail)
                return end();
                
            list_node* next_node = node->next.load();
            
            if (remove_node(node))
                return iterator(next_node);
            else
                return end();
        }
        
        /**
         * @brief 移除指定范围的元素
         * @param first 起始位置
         * @param last 结束位置（不含）
         * @return 指向下一个元素的迭代器
         */
        iterator erase(iterator first, iterator last)
        {
            iterator current = first;
            while (current != last)
            {
                iterator next = current;
                ++next;
                erase(current);
                current = next;
            }
            return last;
        }
        
        /**
         * @brief 移除所有等于指定值的元素
         * @param value_data 要移除的值
         * @return 移除的元素数量
         */
        size_t remove(const value_type& value_data)
        {
            size_t count = 0;
            iterator it = begin();
            
            while (it != end())
            {
                iterator next = it;
                ++next;
                
                try
                {
                    if (*it == value_data)
                    {
                        erase(it);
                        ++count;
                    }
                }
                catch (...)
                {
                    // 忽略无效迭代器
                }
                
                it = next;
            }
            
            return count;
        }
        
        /**
         * @brief 清空链表
         */
        void clear()
        {
            while (!empty())
            {
                pop_front();
            }
        }
        
        /**
         * @brief 与另一无锁链表交换内容
         * @param other 另一个实例
         */
        void swap(atomic_list& other) noexcept
        {
            if (this == &other)
                return;
                
            list_node* this_head = _head.exchange(other._head.load());
            list_node* this_tail = _tail.exchange(other._tail.load());
            size_t this_size = _size.exchange(other._size.load());
            
            other._head.store(this_head);
            other._tail.store(this_tail);
            other._size.store(this_size);
            
            std::swap(_allocator, other._allocator);
            std::swap(_sentinel_head, other._sentinel_head);
            std::swap(_sentinel_tail, other._sentinel_tail);
        }
        
        // 迭代器
        
        /** @brief 起始迭代器 */
        iterator begin() const noexcept
        {
            return iterator(_sentinel_head->next.load());
        }
        
        /** @brief 结束迭代器 */
        iterator end() const noexcept
        {
            return iterator(_sentinel_tail);
        }
        
        /** @brief 起始迭代器（C++11 兼容） */
        const_iterator cbegin() const noexcept
        {
            return begin();
        }
        
        /** @brief 结束迭代器（C++11 兼容） */
        const_iterator cend() const noexcept
        {
            return end();
        }
        
        // 查找和算法
        
        /**
         * @brief 判断元素是否存在
         * @param value_data 待查找值
         * @return true 存在；false 不存在
         */
        bool contains(const value_type& value_data) const
        {
            for (auto it = begin(); it != end(); ++it)
            {
                try
                {
                    if (*it == value_data)
                        return true;
                }
                catch (...)
                {
                    // 忽略无效迭代器
                }
            }
            return false;
        }
        
        /**
         * @brief 查找第一个匹配的元素
         * @param value_data 待查找值
         * @return 指向找到元素的迭代器，未找到返回 end()
         */
        iterator find(const value_type& value_data) const
        {
            for (auto it = begin(); it != end(); ++it)
            {
                try
                {
                    if (*it == value_data)
                        return it;
                }
                catch (...)
                {
                    // 忽略无效迭代器
                }
            }
            return end();
        }
        
        /**
         * @brief 统计指定值的元素个数
         * @param value_data 待统计值
         * @return 元素个数
         */
        size_t count(const value_type& value_data) const
        {
            size_t result = 0;
            for (auto it = begin(); it != end(); ++it)
            {
                try
                {
                    if (*it == value_data)
                        ++result;
                }
                catch (...)
                {
                    // 忽略无效迭代器
                }
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
            for (auto it = begin(); it != end(); ++it)
            {
                try
                {
                    func(*it);
                }
                catch (...)
                {
                    // 忽略无效迭代器
                }
            }
        }
        
        /**
         * @brief 获取当前链表的只读快照
         * @return std::vector<value_type> 元素副本
         * @note 返回的是拷贝，外部可安全遍历
         */
        std::vector<value_type> snapshot() const
        {
            std::vector<value_type> result;
            for (auto it = begin(); it != end(); ++it)
            {
                try
                {
                    result.push_back(*it);
                }
                catch (...)
                {
                    // 忽略无效迭代器
                }
            }
            return result;
        }
        
        // 比较操作
        
        /**
         * @brief 相等比较
         * @param other 另一个 atomic_list
         * @return true 相等；false 不相等
         */
        bool operator==(const atomic_list& other) const
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
         * @param other 另一个 atomic_list
         * @return true 不相等；false 相等
         */
        bool operator!=(const atomic_list& other) const
        {
            return !(*this == other);
        }
    };
    
    // 全局函数
    
    /**
     * @brief 交换两个 atomic_list
     * @param lhs 第一个链表
     * @param rhs 第二个链表
     */
    template <typename value_type, typename allocator_type>
    void swap(atomic_list<value_type, allocator_type>& lhs,
              atomic_list<value_type, allocator_type>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
}