/**
 * @file Atomic_stack.hpp
 * @brief 无锁线程安全的 LIFO 栈容器
 * @author wang
 * @version 1.0
 * @date 2025-08-15
 *
 * 本文件提供无锁线程安全的 LIFO 栈容器：
 *   1. 使用原子操作和CAS实现无锁并发访问；
 *   2. 支持多生产者多消费者模式；
 *   3. 提供与标准库stack兼容的完整接口；
 *   4. 使用蛇形命名法；
 *   5. 支持阻塞和非阻塞的入栈出栈操作。
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
     * @class atomic_stack
     * @brief 无锁线程安全的 LIFO 栈
     * @tparam value_type         元素类型
     * @tparam allocator_type     分配器，默认 `std::allocator<value_type>`
     */
    template <typename value_type, typename allocator_type = std::allocator<value_type>>
    class atomic_stack
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
        // 栈节点结构
        struct stack_node
        {
            std::atomic<value_type*> data;
            std::atomic<stack_node*> next;
            std::atomic<bool> is_valid;
            
            stack_node() : data(nullptr), next(nullptr), is_valid(true) {}
            
            ~stack_node()
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
        
        std::atomic<stack_node*> _top;
        std::atomic<size_t> _size;
        std::atomic<size_t> _max_capacity;
        allocator_type _allocator;
        
        // 内部辅助函数
        stack_node* create_data_node(const value_type& value_data)
        {
            stack_node* node = new stack_node();
            value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
            std::allocator_traits<allocator_type>::construct(_allocator, data, value_data);
            node->data.store(data);
            return node;
        }
        
        stack_node* create_data_node(value_type&& value_data)
        {
            stack_node* node = new stack_node();
            value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
            std::allocator_traits<allocator_type>::construct(_allocator, data, std::move(value_data));
            node->data.store(data);
            return node;
        }
        
        template <typename... args_t>
        stack_node* create_emplace_node(args_t&&... args)
        {
            stack_node* node = new stack_node();
            value_type* data = std::allocator_traits<allocator_type>::allocate(_allocator, 1);
            std::allocator_traits<allocator_type>::construct(_allocator, data, std::forward<args_t>(args)...);
            node->data.store(data);
            return node;
        }
        
        void cleanup_nodes()
        {
            stack_node* current = _top.load();
            while (current)
            {
                stack_node* next = current->next.load();
                delete current;
                current = next;
            }
        }
        
    public:
        /** @brief 默认构造空栈 */
        atomic_stack() : _top(nullptr), _size(0), _max_capacity(0), _allocator() {}
        
        /**
         * @brief 构造指定最大容量的栈
         * @param max_cap 最大容量，0表示无限制
         * @param alloc 分配器
         */
        explicit atomic_stack(size_t max_cap, const allocator_type& alloc = allocator_type())
            : _top(nullptr), _size(0), _max_capacity(max_cap), _allocator(alloc) {}
        
        /**
         * @brief 初始化列表构造
         * @param init 形如 {1, 2, 3} 的列表
         * @param alloc 分配器
         */
        atomic_stack(std::initializer_list<value_type> init,
                    const allocator_type& alloc = allocator_type())
            : _top(nullptr), _size(0), _max_capacity(0), _allocator(alloc)
        {
            for (const auto& item : init)
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
        atomic_stack(input_iterator_t first, input_iterator_t last,
                    const allocator_type& alloc = allocator_type())
            : _top(nullptr), _size(0), _max_capacity(0), _allocator(alloc)
        {
            for (auto it = first; it != last; ++it)
            {
                push(*it);
            }
        }
        
        /** @brief 拷贝构造（线程安全） */
        atomic_stack(const atomic_stack& other)
            : _top(nullptr), _size(0), _max_capacity(other._max_capacity.load()), _allocator(other._allocator)
        {
            // 获取快照并逐个添加（注意栈的逆序特性）
            auto snapshot_data = other.snapshot();
            for (auto it = snapshot_data.rbegin(); it != snapshot_data.rend(); ++it)
            {
                push(*it);
            }
        }
        
        /** @brief 移动构造 */
        atomic_stack(atomic_stack&& other) noexcept
            : _top(other._top.exchange(nullptr)),
              _size(other._size.exchange(0)),
              _max_capacity(other._max_capacity.exchange(0)),
              _allocator(std::move(other._allocator))
        {
        }
        
        /** @brief 拷贝赋值（线程安全） */
        atomic_stack& operator=(const atomic_stack& other)
        {
            if (this != &other)
            {
                clear();
                _max_capacity.store(other._max_capacity.load());
                _allocator = other._allocator;
                
                auto snapshot_data = other.snapshot();
                for (auto it = snapshot_data.rbegin(); it != snapshot_data.rend(); ++it)
                {
                    push(*it);
                }
            }
            return *this;
        }
        
        /** @brief 移动赋值 */
        atomic_stack& operator=(atomic_stack&& other) noexcept
        {
            if (this != &other)
            {
                cleanup_nodes();
                
                _top.store(other._top.exchange(nullptr));
                _size.store(other._size.exchange(0));
                _max_capacity.store(other._max_capacity.exchange(0));
                _allocator = std::move(other._allocator);
            }
            return *this;
        }
        
        /** @brief 析构函数 */
        ~atomic_stack()
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
        
        /** @brief 获取最大容量 */
        size_t max_capacity() const noexcept
        {
            return _max_capacity.load();
        }
        
        /** @brief 设置最大容量 */
        void set_max_capacity(size_t max_cap) noexcept
        {
            _max_capacity.store(max_cap);
        }
        
        /** @brief 判断栈是否已满 */
        bool full() const noexcept
        {
            size_t max_cap = _max_capacity.load();
            return max_cap != 0 && _size.load() >= max_cap;
        }
        
        // 元素访问
        
        /**
         * @brief 获取栈顶元素（不移除）
         * @param out 输出参数，接收元素值
         * @return true 成功；false 栈为空
         */
        bool top(value_type& out) const
        {
            stack_node* top_node = _top.load();
            
            if (!top_node)
                return false;
                
            value_type* data = top_node->data.load();
            if (data)
            {
                out = *data;
                return true;
            }
            return false;
        }
        
        // 修改操作
        
        /**
         * @brief 入栈（拷贝）
         * @param value_data 待入栈元素
         * @return true 成功；false 栈已满
         */
        bool push(const value_type& value_data)
        {
            size_t max_cap = _max_capacity.load();
            if (max_cap != 0 && _size.load() >= max_cap)
                return false;
                
            stack_node* new_node = create_data_node(value_data);
            stack_node* old_top = _top.load();
            
            do
            {
                new_node->next.store(old_top);
            } while (!_top.compare_exchange_weak(old_top, new_node));
            
            _size.fetch_add(1);
            return true;
        }
        
        /**
         * @brief 入栈（移动）
         * @param value_data 待入栈元素
         * @return true 成功；false 栈已满
         */
        bool push(value_type&& value_data)
        {
            size_t max_cap = _max_capacity.load();
            if (max_cap != 0 && _size.load() >= max_cap)
                return false;
                
            stack_node* new_node = create_data_node(std::move(value_data));
            stack_node* old_top = _top.load();
            
            do
            {
                new_node->next.store(old_top);
            } while (!_top.compare_exchange_weak(old_top, new_node));
            
            _size.fetch_add(1);
            return true;
        }
        
        /**
         * @brief 就地构造入栈
         * @param args 构造参数
         * @return true 成功；false 栈已满
         */
        template <typename... args_t>
        bool emplace(args_t&&... args)
        {
            size_t max_cap = _max_capacity.load();
            if (max_cap != 0 && _size.load() >= max_cap)
                return false;
                
            stack_node* new_node = create_emplace_node(std::forward<args_t>(args)...);
            stack_node* old_top = _top.load();
            
            do
            {
                new_node->next.store(old_top);
            } while (!_top.compare_exchange_weak(old_top, new_node));
            
            _size.fetch_add(1);
            return true;
        }
        
        /**
         * @brief 阻塞入栈（拷贝）
         * @param value_data 待入栈元素
         */
        void push_blocking(const value_type& value_data)
        {
            while (!push(value_data))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        /**
         * @brief 阻塞入栈（移动）
         * @param value_data 待入栈元素
         */
        void push_blocking(value_type&& value_data)
        {
            while (!push(std::move(value_data)))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        /**
         * @brief 出栈（阻塞等待）
         * @param out 接收出栈元素的引用
         * @return true 成功；false 失败（理论上不会发生）
         */
        bool pop(value_type& out)
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
         * @brief 尝试出栈（非阻塞）
         * @param out 接收出栈元素的引用
         * @return true 成功；false 栈为空
         */
        bool try_pop(value_type& out)
        {
            stack_node* old_top = _top.load();
            
            if (!old_top)
                return false;
                
            stack_node* new_top = old_top->next.load();
            
            if (_top.compare_exchange_weak(old_top, new_top))
            {
                value_type* data = old_top->data.load();
                if (data)
                {
                    out = std::move(*data);
                    delete old_top;
                    _size.fetch_sub(1);
                    return true;
                }
            }
            
            return false;
        }
        
        /**
         * @brief 带超时的出栈操作
         * @param out 接收出栈元素的引用
         * @param timeout_ms 超时时间（毫秒）
         * @return true 成功；false 超时
         */
        bool pop_for(value_type& out, size_t timeout_ms)
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
         * @brief 批量入栈
         * @param values 待入栈元素的容器
         * @return 实际入栈数量
         */
        template <typename container_t>
        size_t push_range(const container_t& values)
        {
            size_t count = 0;
            for (const auto& value_data : values)
            {
                if (push(value_data))
                    ++count;
                else
                    break; // 栈已满
            }
            return count;
        }
        
        /**
         * @brief 批量出栈
         * @param out 接收出栈元素的容器
         * @param max_count 最大出栈数量
         * @return 实际出栈数量
         */
        template <typename container_t>
        size_t pop_range(container_t& out, size_t max_count)
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
         * @brief 清空栈
         */
        void clear()
        {
            value_type dummy;
            while (try_pop(dummy))
            {
                // 继续出栈直到为空
            }
        }
        
        /**
         * @brief 与另一无锁栈交换内容
         * @param other 另一个实例
         */
        void swap(atomic_stack& other) noexcept
        {
            if (this == &other)
                return;
                
            stack_node* this_top = _top.exchange(other._top.load());
            size_t this_size = _size.exchange(other._size.load());
            size_t this_max_cap = _max_capacity.exchange(other._max_capacity.load());
            
            other._top.store(this_top);
            other._size.store(this_size);
            other._max_capacity.store(this_max_cap);
            
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
            stack_node* current = _top.load();
            
            while (current)
            {
                value_type* data = current->data.load();
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
        size_t count(const value_type& value_data) const
        {
            size_t result = 0;
            stack_node* current = _top.load();
            
            while (current)
            {
                value_type* data = current->data.load();
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
            stack_node* current = _top.load();
            
            while (current)
            {
                value_type* data = current->data.load();
                if (data)
                    func(*data);
                current = current->next.load();
            }
        }
        
        /**
         * @brief 获取当前栈的只读快照
         * @return std::vector<value_type> 元素副本，按 LIFO 顺序（栈顶在前）
         * @note 返回的是拷贝，外部可安全遍历
         */
        std::vector<value_type> snapshot() const
        {
            std::vector<value_type> result;
            stack_node* current = _top.load();
            
            while (current)
            {
                value_type* data = current->data.load();
                if (data)
                    result.push_back(*data);
                current = current->next.load();
            }
            
            return result;
        }
        
        // 比较操作
        
        /**
         * @brief 相等比较
         * @param other 另一个 atomic_stack
         * @return true 相等；false 不相等
         */
        bool operator==(const atomic_stack& other) const
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
         * @param other 另一个 atomic_stack
         * @return true 不相等；false 相等
         */
        bool operator!=(const atomic_stack& other) const
        {
            return !(*this == other);
        }
    };
    
    // 全局函数
    
    /**
     * @brief 交换两个 atomic_stack
     * @param lhs 第一个栈
     * @param rhs 第二个栈
     */
    template <typename value_type, typename allocator_type>
    void swap(atomic_stack<value_type, allocator_type>& lhs,
              atomic_stack<value_type, allocator_type>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
}