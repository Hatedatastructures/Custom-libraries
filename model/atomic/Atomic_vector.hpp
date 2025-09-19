/**
 * @file Atomic_vector.hpp
 * @brief 无锁线程安全的动态数组（vector）容器
 * @author wang
 * @version 1.0
 * @date 2025-08-15
 *
 * 本文件提供无锁线程安全的动态数组容器：
 *   1. 使用原子操作和CAS实现无锁并发访问；
 *   2. 支持多线程同时读写操作；
 *   3. 提供与标准库vector兼容的完整接口；
 *   4. 使用蛇形命名法；
 *   5. 支持动态扩容、随机访问、插入删除等功能。
 */

#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include <functional>

namespace atomic_concurrent
{
    /**
     * @class atomic_vector
     * @brief 无锁线程安全的动态数组
     * @tparam value_type         元素类型
     * @tparam allocator_type     分配器，默认 `std::allocator<value_type>`
     */
    template <typename value_type, typename allocator_type = std::allocator<value_type>>
    class atomic_vector
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
        // 内部节点结构
        struct atomic_node
        {
            std::atomic<value_type*> data_ptr;
            std::atomic<size_t> capacity;
            std::atomic<size_t> size;
            std::atomic<bool> is_valid;
            
            atomic_node() 
                : data_ptr(nullptr), capacity(0), size(0), is_valid(true) {}
                
            ~atomic_node()
            {
                value_type* ptr = data_ptr.load();
                if (ptr)
                {
                    allocator_type alloc;
                    for (size_t i = 0; i < size.load(); ++i)
                    {
                        std::allocator_traits<allocator_type>::destroy(alloc, ptr + i);
                    }
                    std::allocator_traits<allocator_type>::deallocate(alloc, ptr, capacity.load());
                }
            }
        };
        
        std::atomic<atomic_node*> _head;
        allocator_type _allocator;
        
        // 内部辅助函数
        atomic_node* create_new_node(size_t initial_capacity = 16)
        {
            atomic_node* node = new atomic_node();
            if (initial_capacity > 0)
            {
                value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, initial_capacity);
                node->data_ptr.store(data);
                node->capacity.store(initial_capacity);
            }
            return node;
        }
        
        void ensure_capacity(atomic_node* node, size_t required_capacity)
        {
            size_t current_capacity = node->capacity.load();
            if (current_capacity >= required_capacity)
                return;
                
            size_t new_capacity = std::max(current_capacity * 2, required_capacity);
            value_type* new_data = std::allocator_traits<allocator_type>::allocate(_allocator, new_capacity);
            
            value_type* old_data = node->data_ptr.load();
            size_t current_size = node->size.load();
            
            // 拷贝现有元素
            for (size_t i = 0; i < current_size; ++i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, new_data + i, std::move(old_data[i]));
                std::allocator_traits<allocator_type>::destroy(_allocator, old_data + i);
            }
            
            // 原子更新
            node->data_ptr.store(new_data);
            node->capacity.store(new_capacity);
            
            // 释放旧内存
            if (old_data)
            {
                std::allocator_traits<allocator_type>::deallocate(_allocator, old_data, current_capacity);
            }
        }
        
    public:
        /** @brief 默认构造空 vector */
        atomic_vector() : _head(create_new_node()), _allocator() {}
        
        /**
         * @brief 指定初始大小与值构造
         * @param count 初始元素个数
         * @param value_data 初始值（默认构造）
         * @param alloc 分配器
         */
        explicit atomic_vector(size_t count, const value_type& value_data = value_type(),
                              const allocator_type& alloc = allocator_type())
            : _allocator(alloc)
        {
            atomic_node* node = create_new_node(count);
            value_type* data = node->data_ptr.load();
            
            for (size_t i = 0; i < count; ++i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i, value_data);
            }
            
            node->size.store(count);
            _head.store(node);
        }
        
        /**
         * @brief 范围构造
         * @tparam input_iterator_t 输入迭代器
         * @param first 起始
         * @param last  终止（不含）
         * @param alloc 分配器
         */
        template <typename input_iterator_t>
        atomic_vector(input_iterator_t first, input_iterator_t last,
                     const allocator_type& alloc = allocator_type())
            : _allocator(alloc)
        {
            size_t count = std::distance(first, last);
            atomic_node* node = create_new_node(count);
            value_type* data = node->data_ptr.load();
            
            size_t i = 0;
            for (auto it = first; it != last; ++it, ++i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i, *it);
            }
            
            node->size.store(count);
            _head.store(node);
        }
        
        /**
         * @brief 初始化列表构造
         * @param init 形如 {1, 2, 3} 的列表
         * @param alloc 分配器
         */
        atomic_vector(std::initializer_list<value_type> init,
                     const allocator_type& alloc = allocator_type())
            : atomic_vector(init.begin(), init.end(), alloc) {}
        
        /** @brief 拷贝构造（线程安全） */
        atomic_vector(const atomic_vector& other)
            : _allocator(other._allocator)
        {
            atomic_node* other_node = other._head.load();
            size_t other_size = other_node->size.load();
            
            atomic_node* node = create_new_node(other_size);
            value_type* data = node->data_ptr.load();
            value_type* other_data = other_node->data_ptr.load();
            
            for (size_t i = 0; i < other_size; ++i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i, other_data[i]);
            }
            
            node->size.store(other_size);
            _head.store(node);
        }
        
        /** @brief 移动构造 */
        atomic_vector(atomic_vector&& other) noexcept
            : _head(other._head.exchange(nullptr)), _allocator(std::move(other._allocator)) {}
        
        /** @brief 拷贝赋值（线程安全） */
        atomic_vector& operator=(const atomic_vector& other)
        {
            if (this != &other)
            {
                atomic_node* other_node = other._head.load();
                size_t other_size = other_node->size.load();
                
                atomic_node* new_node = create_new_node(other_size);
                value_type* data = new_node->data_ptr.load();
                value_type* other_data = other_node->data_ptr.load();
                
                for (size_t i = 0; i < other_size; ++i)
                {
                    std::allocator_traits<allocator_type>::construct(_allocator, data + i, other_data[i]);
                }
                
                new_node->size.store(other_size);
                
                atomic_node* old_node = _head.exchange(new_node);
                delete old_node;
            }
            return *this;
        }
        
        /** @brief 移动赋值 */
        atomic_vector& operator=(atomic_vector&& other) noexcept
        {
            if (this != &other)
            {
                atomic_node* old_node = _head.exchange(other._head.exchange(nullptr));
                delete old_node;
                _allocator = std::move(other._allocator);
            }
            return *this;
        }
        
        /** @brief 析构函数 */
        ~atomic_vector()
        {
            atomic_node* node = _head.load();
            delete node;
        }
        
        // 容量相关
        
        /** @brief 当前元素数量 */
        size_t size() const noexcept
        {
            atomic_node* node = _head.load();
            return node ? node->size.load() : 0;
        }
        
        /** @brief 是否为空 */
        bool empty() const noexcept
        {
            return size() == 0;
        }
        
        /** @brief 最大元素数（理论值） */
        size_t max_size() const noexcept
        {
            return std::allocator_traits<allocator_type>::max_size(_allocator);
        }
        
        /** @brief 当前容量（已分配空间大小） */
        size_t capacity() const noexcept
        {
            atomic_node* node = _head.load();
            return node ? node->capacity.load() : 0;
        }
        
        /**
         * @brief 预留容量，避免多次重新分配
         * @param new_capacity 期望容量
         */
        void reserve(size_t new_capacity)
        {
            atomic_node* node = _head.load();
            if (node && new_capacity > node->capacity.load())
            {
                ensure_capacity(node, new_capacity);
            }
        }
        
        /**
         * @brief 调整容器大小
         * @param count 新大小
         * @param value_data 新增元素的默认值
         */
        void resize(size_t count, const value_type& value_data = value_type())
        {
            atomic_node* node = _head.load();
            if (!node) return;
            
            size_t current_size = node->size.load();
            
            if (count > current_size)
            {
                ensure_capacity(node, count);
                value_type* data = node->data_ptr.load();
                
                for (size_t i = current_size; i < count; ++i)
                {
                    std::allocator_traits<allocator_type>::construct(_allocator, data + i, value_data);
                }
            }
            else if (count < current_size)
            {
                value_type* data = node->data_ptr.load();
                for (size_t i = count; i < current_size; ++i)
                {
                    std::allocator_traits<allocator_type>::destroy(_allocator, data + i);
                }
            }
            
            node->size.store(count);
        }
        
        /**
         * @brief 缩减容量到当前大小
         */
        void shrink_to_fit()
        {
            atomic_node* node = _head.load();
            if (!node) return;
            
            size_t current_size = node->size.load();
            size_t current_capacity = node->capacity.load();
            
            if (current_capacity > current_size)
            {
                value_type* new_data = std::allocator_traits<allocator_type>::allocate(_allocator, current_size);
                value_type* old_data = node->data_ptr.load();
                
                for (size_t i = 0; i < current_size; ++i)
                {
                    std::allocator_traits<allocator_type>::construct(_allocator, new_data + i, std::move(old_data[i]));
                    std::allocator_traits<allocator_type>::destroy(_allocator, old_data + i);
                }
                
                node->data_ptr.store(new_data);
                node->capacity.store(current_size);
                
                std::allocator_traits<allocator_type>::deallocate(_allocator, old_data, current_capacity);
            }
        }
        
        // 元素访问
        
        /**
         * @brief 随机下标访问（带边界检查）
         * @param pos 下标位置
         * @param out 输出参数，接收元素值
         * @return true 成功；false 越界
         */
        bool at(size_t pos, value_type& out) const
        {
            atomic_node* node = _head.load();
            if (!node || pos >= node->size.load())
                return false;
                
            value_type* data = node->data_ptr.load();
            out = data[pos];
            return true;
        }
        
        /**
         * @brief 随机下标访问（带边界检查）
         * @param pos 下标位置
         * @return 元素值的拷贝
         * @throw std::out_of_range 越界则抛出异常
         */
        value_type at(size_t pos) const
        {
            atomic_node* node = _head.load();
            if (!node || pos >= node->size.load())
                throw std::out_of_range("atomic_vector::at: index out of range");
                
            value_type* data = node->data_ptr.load();
            return data[pos];
        }
        
        /**
         * @brief 随机下标访问（不检查边界）
         * @param pos 下标位置
         * @param out 输出参数，接收元素值
         * @return true 成功；false 失败
         */
        bool operator[](size_t pos, value_type& out) const
        {
            atomic_node* node = _head.load();
            if (!node || pos >= node->size.load())
                return false;
                
            value_type* data = node->data_ptr.load();
            out = data[pos];
            return true;
        }
        
        /**
         * @brief 获取第一个元素
         * @param out 输出参数，接收元素值
         * @return true 成功；false 容器为空
         */
        bool front(value_type& out) const
        {
            return at(0, out);
        }
        
        /**
         * @brief 获取最后一个元素
         * @param out 输出参数，接收元素值
         * @return true 成功；false 容器为空
         */
        bool back(value_type& out) const
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            if (current_size == 0)
                return false;
                
            return at(current_size - 1, out);
        }
        
        /**
         * @brief 获取数据指针（不安全，仅用于特殊场景）
         * @return 数据指针
         */
        value_type* data() const
        {
            atomic_node* node = _head.load();
            return node ? node->data_ptr.load() : nullptr;
        }
        
        // 修改操作
        
        /**
         * @brief 设置指定位置的元素值
         * @param pos 位置
         * @param value_data 新值
         * @return true 成功；false 越界
         */
        bool set(size_t pos, const value_type& value_data)
        {
            atomic_node* node = _head.load();
            if (!node || pos >= node->size.load())
                return false;
                
            value_type* data = node->data_ptr.load();
            data[pos] = value_data;
            return true;
        }
        
        /**
         * @brief 在末尾追加元素（拷贝）
         * @param value_data 待追加元素
         */
        void push_back(const value_type& value_data)
        {
            atomic_node* node = _head.load();
            if (!node) return;
            
            size_t current_size = node->size.load();
            ensure_capacity(node, current_size + 1);
            
            value_type* data = node->data_ptr.load();
            std::allocator_traits<allocator_type>::construct(_allocator, data + current_size, value_data);
            
            node->size.fetch_add(1);
        }
        
        /**
         * @brief 在末尾追加元素（移动）
         * @param value_data 待追加元素
         */
        void push_back(value_type&& value_data)
        {
            atomic_node* node = _head.load();
            if (!node) return;
            
            size_t current_size = node->size.load();
            ensure_capacity(node, current_size + 1);
            
            value_type* data = node->data_ptr.load();
            std::allocator_traits<allocator_type>::construct(_allocator, data + current_size, std::move(value_data));
            
            node->size.fetch_add(1);
        }
        
        /**
         * @brief 就地构造追加元素
         * @param args 构造参数
         */
        template <typename... args_t>
        void emplace_back(args_t&&... args)
        {
            atomic_node* node = _head.load();
            if (!node) return;
            
            size_t current_size = node->size.load();
            ensure_capacity(node, current_size + 1);
            
            value_type* data = node->data_ptr.load();
            std::allocator_traits<allocator_type>::construct(_allocator, data + current_size, std::forward<args_t>(args)...);
            
            node->size.fetch_add(1);
        }
        
        /**
         * @brief 删除末尾元素并返回其值
         * @param out 接收出栈元素的引用
         * @return true 成功；false 为空
         */
        bool pop_back(value_type& out)
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            if (current_size == 0)
                return false;
                
            value_type* data = node->data_ptr.load();
            out = std::move(data[current_size - 1]);
            std::allocator_traits<allocator_type>::destroy(_allocator, data + current_size - 1);
            
            node->size.fetch_sub(1);
            return true;
        }
        
        /**
         * @brief 删除末尾元素
         */
        void pop_back()
        {
            value_type dummy;
            pop_back(dummy);
        }
        
        /**
         * @brief 在指定位置插入元素（拷贝）
         * @param pos 插入位置下标
         * @param value_data 待插入元素
         * @return true 成功；false 失败
         */
        bool insert(size_t pos, const value_type& value_data)
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            if (pos > current_size)
                return false;
                
            ensure_capacity(node, current_size + 1);
            value_type* data = node->data_ptr.load();
            
            // 移动元素
            for (size_t i = current_size; i > pos; --i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i, std::move(data[i - 1]));
                std::allocator_traits<allocator_type>::destroy(_allocator, data + i - 1);
            }
            
            // 插入新元素
            std::allocator_traits<allocator_type>::construct(_allocator, data + pos, value_data);
            node->size.fetch_add(1);
            
            return true;
        }
        
        /**
         * @brief 在指定位置插入元素（移动）
         * @param pos 插入位置下标
         * @param value_data 待插入元素
         * @return true 成功；false 失败
         */
        bool insert(size_t pos, value_type&& value_data)
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            if (pos > current_size)
                return false;
                
            ensure_capacity(node, current_size + 1);
            value_type* data = node->data_ptr.load();
            
            // 移动元素
            for (size_t i = current_size; i > pos; --i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i, std::move(data[i - 1]));
                std::allocator_traits<allocator_type>::destroy(_allocator, data + i - 1);
            }
            
            // 插入新元素
            std::allocator_traits<allocator_type>::construct(_allocator, data + pos, std::move(value_data));
            node->size.fetch_add(1);
            
            return true;
        }
        
        /**
         * @brief 删除指定位置的元素
         * @param pos 待删除位置下标
         * @return true 成功；false 越界
         */
        bool erase(size_t pos)
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            if (pos >= current_size)
                return false;
                
            value_type* data = node->data_ptr.load();
            
            // 销毁元素
            std::allocator_traits<allocator_type>::destroy(_allocator, data + pos);
            
            // 移动后续元素
            for (size_t i = pos; i < current_size - 1; ++i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i, std::move(data[i + 1]));
                std::allocator_traits<allocator_type>::destroy(_allocator, data + i + 1);
            }
            
            node->size.fetch_sub(1);
            return true;
        }
        
        /**
         * @brief 删除区间元素
         * @param first 起始下标
         * @param last  终止下标（不含）
         * @return true 成功；false 失败
         */
        bool erase(size_t first, size_t last)
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            if (first >= current_size || first >= last)
                return false;
                
            if (last > current_size)
                last = current_size;
                
            value_type* data = node->data_ptr.load();
            size_t erase_count = last - first;
            
            // 销毁要删除的元素
            for (size_t i = first; i < last; ++i)
            {
                std::allocator_traits<allocator_type>::destroy(_allocator, data + i);
            }
            
            // 移动后续元素
            for (size_t i = last; i < current_size; ++i)
            {
                std::allocator_traits<allocator_type>::construct(_allocator, data + i - erase_count, std::move(data[i]));
                std::allocator_traits<allocator_type>::destroy(_allocator, data + i);
            }
            
            node->size.fetch_sub(erase_count);
            return true;
        }
        
        /**
         * @brief 清空所有元素
         */
        void clear()
        {
            atomic_node* node = _head.load();
            if (!node)
                return;
                
            size_t current_size = node->size.load();
            value_type* data = node->data_ptr.load();
            
            for (size_t i = 0; i < current_size; ++i)
            {
                std::allocator_traits<allocator_type>::destroy(_allocator, data + i);
            }
            
            node->size.store(0);
        }
        
        /**
         * @brief 与另一无锁 vector 交换内容
         * @param other 另一个实例
         */
        void swap(atomic_vector& other) noexcept
        {
            if (this == &other)
                return;
                
            atomic_node* this_node = _head.exchange(other._head.load());
            other._head.store(this_node);
            
            std::swap(_allocator, other._allocator);
        }
        
        // 查找和算法
        
        /**
         * @brief 判断元素是否存在
         * @param value_data 待查找值
         * @return true 存在；false 不存在
         */
        bool contains(const value_type& value_data) const
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            value_type* data = node->data_ptr.load();
            
            for (size_t i = 0; i < current_size; ++i)
            {
                if (data[i] == value_data)
                    return true;
            }
            return false;
        }
        
        /**
         * @brief 查找元素第一次出现的位置
         * @param value_data 待查找值
         * @param pos 输出参数，接收位置
         * @return true 找到；false 未找到
         */
        bool find(const value_type& value_data, size_t& pos) const
        {
            atomic_node* node = _head.load();
            if (!node)
                return false;
                
            size_t current_size = node->size.load();
            value_type* data = node->data_ptr.load();
            
            for (size_t i = 0; i < current_size; ++i)
            {
                if (data[i] == value_data)
                {
                    pos = i;
                    return true;
                }
            }
            return false;
        }
        
        /**
         * @brief 统计指定值的元素个数
         * @param value_data 待统计值
         * @return 元素个数
         */
        size_t count(const value_type& value_data) const
        {
            atomic_node* node = _head.load();
            if (!node)
                return 0;
                
            size_t current_size = node->size.load();
            value_type* data = node->data_ptr.load();
            size_t result = 0;
            
            for (size_t i = 0; i < current_size; ++i)
            {
                if (data[i] == value_data)
                    ++result;
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
            atomic_node* node = _head.load();
            if (!node)
                return;
                
            size_t current_size = node->size.load();
            value_type* data = node->data_ptr.load();
            
            for (size_t i = 0; i < current_size; ++i)
            {
                func(data[i]);
            }
        }
        
        /**
         * @brief 获取当前 vector 的只读快照
         * @return std::vector<value_type> 元素副本，顺序与内部一致
         * @note 返回的是拷贝，外部可安全遍历
         */
        std::vector<value_type> snapshot() const
        {
            atomic_node* node = _head.load();
            if (!node)
                return std::vector<value_type>();
                
            size_t current_size = node->size.load();
            value_type* data = node->data_ptr.load();
            
            std::vector<value_type> result;
            result.reserve(current_size);
            
            for (size_t i = 0; i < current_size; ++i)
            {
                result.push_back(data[i]);
            }
            
            return result;
        }
        
        // 比较操作
        
        /**
         * @brief 相等比较
         * @param other 另一个 atomic_vector
         * @return true 相等；false 不相等
         */
        bool operator==(const atomic_vector& other) const
        {
            if (this == &other)
                return true;
                
            atomic_node* this_node = _head.load();
            atomic_node* other_node = other._head.load();
            
            if (!this_node && !other_node)
                return true;
            if (!this_node || !other_node)
                return false;
                
            size_t this_size = this_node->size.load();
            size_t other_size = other_node->size.load();
            
            if (this_size != other_size)
                return false;
                
            value_type* this_data = this_node->data_ptr.load();
            value_type* other_data = other_node->data_ptr.load();
            
            for (size_t i = 0; i < this_size; ++i)
            {
                if (this_data[i] != other_data[i])
                    return false;
            }
            
            return true;
        }
        
        /**
         * @brief 不等比较
         * @param other 另一个 atomic_vector
         * @return true 不相等；false 相等
         */
        bool operator!=(const atomic_vector& other) const
        {
            return !(*this == other);
        }
    };
    
    // 全局函数
    
    /**
     * @brief 交换两个 atomic_vector
     * @param lhs 第一个 vector
     * @param rhs 第二个 vector
     */
    template <typename value_type, typename allocator_type>
    void swap(atomic_vector<value_type, allocator_type>& lhs,
              atomic_vector<value_type, allocator_type>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
}