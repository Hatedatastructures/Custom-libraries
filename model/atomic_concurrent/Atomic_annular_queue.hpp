#ifndef ATOMIC_ANNULAR_QUEUE_HPP
#define ATOMIC_ANNULAR_QUEUE_HPP

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <iterator>

namespace wang
{
    /**
     * @brief 无锁线程安全的环形队列
     * 
     * atomic_annular_queue 是一个基于原子操作的固定容量环形队列，提供线程安全的队列操作。
     * 使用序列号机制和CAS操作实现无锁并发访问，避免ABA问题。
     * 
     * 特性：
     * - 使用原子操作和CAS实现无锁并发访问
     * - 支持多生产者多消费者模型
     * - 固定容量，避免动态内存分配
     * - 使用序列号机制避免ABA问题
     * - 提供阻塞和非阻塞操作接口
     * - 使用蛇形命名法
     * 
     * @tparam T 元素类型
     */
    template<typename T>
    class atomic_annular_queue
    {
    public:
        // 类型别名
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        
    private:
        // 槽位结构
        struct slot
        {
            std::atomic<T> data;
            std::atomic<size_type> sequence;
            
            slot() : sequence(0) {}
        };
        
        std::unique_ptr<slot[]> _buffer;
        size_type _capacity;
        size_type _mask; // capacity - 1，用于快速取模
        
        alignas(64) std::atomic<size_type> _enqueue_pos;
        alignas(64) std::atomic<size_type> _dequeue_pos;
        
        static constexpr size_type MAX_RETRY_COUNT = 1000;
        
        /**
         * @brief 检查容量是否为2的幂
         */
        static bool is_power_of_two(size_type n)
        {
            return n > 0 && (n & (n - 1)) == 0;
        }
        
        /**
         * @brief 将数字向上舍入到最近的2的幂
         */
        static size_type next_power_of_two(size_type n)
        {
            if (n <= 1) return 2;
            
            size_type power = 1;
            while (power < n)
            {
                power <<= 1;
            }
            return power;
        }
        
    public:
        // 构造函数
        
        /**
         * @brief 构造函数
         * 
         * @param capacity 队列容量（会自动调整为2的幂）
         * @throws std::invalid_argument 如果容量为0
         */
        explicit atomic_annular_queue(size_type capacity)
        {
            if (capacity == 0)
            {
                throw std::invalid_argument("Capacity must be greater than 0");
            }
            
            // 确保容量是2的幂，便于使用位运算优化
            if (!is_power_of_two(capacity))
            {
                capacity = next_power_of_two(capacity);
            }
            
            _capacity = capacity;
            _mask = capacity - 1;
            _buffer = std::make_unique<slot[]>(capacity);
            
            // 初始化序列号
            for (size_type i = 0; i < capacity; ++i)
            {
                _buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
            
            _enqueue_pos.store(0, std::memory_order_relaxed);
            _dequeue_pos.store(0, std::memory_order_relaxed);
        }
        
        // 禁用拷贝构造和赋值
        atomic_annular_queue(const atomic_annular_queue&) = delete;
        atomic_annular_queue& operator=(const atomic_annular_queue&) = delete;
        
        /**
         * @brief 移动构造函数
         * 
         * @param other 另一个队列
         */
        atomic_annular_queue(atomic_annular_queue&& other) noexcept
            : _buffer(std::move(other._buffer))
            , _capacity(other._capacity)
            , _mask(other._mask)
            , _enqueue_pos(other._enqueue_pos.load(std::memory_order_acquire))
            , _dequeue_pos(other._dequeue_pos.load(std::memory_order_acquire))
        {
            other._capacity = 0;
            other._mask = 0;
        }
        
        /**
         * @brief 移动赋值运算符
         * 
         * @param other 另一个队列
         * @return atomic_annular_queue& 自身引用
         */
        atomic_annular_queue& operator=(atomic_annular_queue&& other) noexcept
        {
            if (this != &other)
            {
                _buffer = std::move(other._buffer);
                _capacity = other._capacity;
                _mask = other._mask;
                _enqueue_pos.store(other._enqueue_pos.load(std::memory_order_acquire), std::memory_order_release);
                _dequeue_pos.store(other._dequeue_pos.load(std::memory_order_acquire), std::memory_order_release);
                
                other._capacity = 0;
                other._mask = 0;
            }
            return *this;
        }
        
        // 析构函数
        ~atomic_annular_queue() = default;
        
        // 容量
        
        /**
         * @brief 检查队列是否为空
         * 
         * @return bool 如果队列为空则返回true
         */
        bool empty() const noexcept
        {
            return _enqueue_pos.load(std::memory_order_acquire) == _dequeue_pos.load(std::memory_order_acquire);
        }
        
        /**
         * @brief 获取队列当前大小（近似值）
         * 
         * @return size_type 当前元素数量
         */
        size_type size() const noexcept
        {
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            
            if (enqueue_pos >= dequeue_pos)
            {
                return enqueue_pos - dequeue_pos;
            }
            else
            {
                return _capacity - (dequeue_pos - enqueue_pos);
            }
        }
        
        /**
         * @brief 获取队列最大容量
         * 
         * @return size_type 队列容量
         */
        size_type max_size() const noexcept
        {
            return _capacity;
        }
        
        /**
         * @brief 获取队列容量
         * 
         * @return size_type 队列容量
         */
        size_type capacity() const noexcept
        {
            return _capacity;
        }
        
        /**
         * @brief 检查队列是否已满
         * 
         * @return bool 如果队列已满则返回true
         */
        bool full() const noexcept
        {
            return size() >= _capacity;
        }
        
        // 元素访问
        
        /**
         * @brief 获取队列前端元素（不出队）
         * 
         * @param item 接收元素的引用
         * @return bool 成功返回true，队列空返回false
         */
        bool front(T& item) const
        {
            size_type pos = _dequeue_pos.load(std::memory_order_acquire);
            slot* slot_ptr = &_buffer[pos & _mask];
            size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
            
            if (seq == pos + 1)
            {
                item = slot_ptr->data.load(std::memory_order_acquire);
                return true;
            }
            
            return false;
        }
        
        /**
         * @brief 获取队列后端元素（不出队）
         * 
         * @param item 接收元素的引用
         * @return bool 成功返回true，队列空返回false
         */
        bool back(T& item) const
        {
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            if (enqueue_pos == 0) return false;
            
            size_type pos = enqueue_pos - 1;
            slot* slot_ptr = &_buffer[pos & _mask];
            size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
            
            if (seq == pos + 1)
            {
                item = slot_ptr->data.load(std::memory_order_acquire);
                return true;
            }
            
            return false;
        }
        
        // 修改操作
        
        /**
         * @brief 入队操作（非阻塞）
         * 
         * @param item 待入队元素
         * @return bool 成功返回true，队列满返回false
         */
        bool try_push(const T& item)
        {
            for (size_type retry = 0; retry < MAX_RETRY_COUNT; ++retry)
            {
                size_type pos = _enqueue_pos.load(std::memory_order_relaxed);
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
                
                if (diff == 0)
                {
                    // 尝试占用这个槽位
                    if (_enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        slot_ptr->data.store(item, std::memory_order_relaxed);
                        slot_ptr->sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0)
                {
                    // 队列已满
                    return false;
                }
                
                std::this_thread::yield();
            }
            
            return false;
        }
        
        /**
         * @brief 入队操作（移动语义，非阻塞）
         * 
         * @param item 待入队元素
         * @return bool 成功返回true，队列满返回false
         */
        bool try_push(T&& item)
        {
            for (size_type retry = 0; retry < MAX_RETRY_COUNT; ++retry)
            {
                size_type pos = _enqueue_pos.load(std::memory_order_relaxed);
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
                
                if (diff == 0)
                {
                    if (_enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        slot_ptr->data.store(std::move(item), std::memory_order_relaxed);
                        slot_ptr->sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0)
                {
                    return false;
                }
                
                std::this_thread::yield();
            }
            
            return false;
        }
        
        /**
         * @brief 就地构造入队（非阻塞）
         * 
         * @tparam Args 构造参数类型
         * @param args 构造参数
         * @return bool 成功返回true，队列满返回false
         */
        template<typename... Args>
        bool try_emplace(Args&&... args)
        {
            return try_push(T(std::forward<Args>(args)...));
        }
        
        /**
         * @brief 入队操作（阻塞）
         * 
         * @param item 待入队元素
         */
        void push(const T& item)
        {
            while (!try_push(item))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        /**
         * @brief 入队操作（移动语义，阻塞）
         * 
         * @param item 待入队元素
         */
        void push(T&& item)
        {
            while (!try_push(std::move(item)))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        /**
         * @brief 就地构造入队（阻塞）
         * 
         * @tparam Args 构造参数类型
         * @param args 构造参数
         */
        template<typename... Args>
        void emplace(Args&&... args)
        {
            push(T(std::forward<Args>(args)...));
        }
        
        /**
         * @brief 出队操作（非阻塞）
         * 
         * @param item 接收出队元素的引用
         * @return bool 成功返回true，队列空返回false
         */
        bool try_pop(T& item)
        {
            for (size_type retry = 0; retry < MAX_RETRY_COUNT; ++retry)
            {
                size_type pos = _dequeue_pos.load(std::memory_order_relaxed);
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
                
                if (diff == 0)
                {
                    // 尝试占用这个槽位
                    if (_dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        item = slot_ptr->data.load(std::memory_order_relaxed);
                        slot_ptr->sequence.store(pos + _mask + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0)
                {
                    // 队列为空
                    return false;
                }
                
                std::this_thread::yield();
            }
            
            return false;
        }
        
        /**
         * @brief 出队操作（阻塞）
         * 
         * @param item 接收出队元素的引用
         */
        void pop(T& item)
        {
            while (!try_pop(item))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        /**
         * @brief 清空队列
         */
        void clear()
        {
            T dummy;
            while (try_pop(dummy))
            {
                // 继续出队直到队列为空
            }
        }
        
        /**
         * @brief 交换两个队列的内容
         * 
         * @param other 另一个队列
         */
        void swap(atomic_annular_queue& other) noexcept
        {
            if (this != &other)
            {
                std::swap(_buffer, other._buffer);
                std::swap(_capacity, other._capacity);
                std::swap(_mask, other._mask);
                
                size_type this_enqueue = _enqueue_pos.load(std::memory_order_acquire);
                size_type this_dequeue = _dequeue_pos.load(std::memory_order_acquire);
                size_type other_enqueue = other._enqueue_pos.load(std::memory_order_acquire);
                size_type other_dequeue = other._dequeue_pos.load(std::memory_order_acquire);
                
                _enqueue_pos.store(other_enqueue, std::memory_order_release);
                _dequeue_pos.store(other_dequeue, std::memory_order_release);
                other._enqueue_pos.store(this_enqueue, std::memory_order_release);
                other._dequeue_pos.store(this_dequeue, std::memory_order_release);
            }
        }
        
        // 超时操作
        
        /**
         * @brief 超时入队操作
         * 
         * @tparam Rep 时间表示类型
         * @tparam Period 时间周期类型
         * @param item 待入队元素
         * @param timeout 超时时间
         * @return bool 成功返回true，超时返回false
         */
        template<typename Rep, typename Period>
        bool try_push_for(const T& item, const std::chrono::duration<Rep, Period>& timeout)
        {
            auto start_time = std::chrono::steady_clock::now();
            
            while (!try_push(item))
            {
                if (std::chrono::steady_clock::now() - start_time >= timeout)
                {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            
            return true;
        }
        
        /**
         * @brief 超时入队操作（移动语义）
         * 
         * @tparam Rep 时间表示类型
         * @tparam Period 时间周期类型
         * @param item 待入队元素
         * @param timeout 超时时间
         * @return bool 成功返回true，超时返回false
         */
        template<typename Rep, typename Period>
        bool try_push_for(T&& item, const std::chrono::duration<Rep, Period>& timeout)
        {
            auto start_time = std::chrono::steady_clock::now();
            
            while (!try_push(std::move(item)))
            {
                if (std::chrono::steady_clock::now() - start_time >= timeout)
                {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            
            return true;
        }
        
        /**
         * @brief 超时出队操作
         * 
         * @tparam Rep 时间表示类型
         * @tparam Period 时间周期类型
         * @param item 接收出队元素的引用
         * @param timeout 超时时间
         * @return bool 成功返回true，超时返回false
         */
        template<typename Rep, typename Period>
        bool try_pop_for(T& item, const std::chrono::duration<Rep, Period>& timeout)
        {
            auto start_time = std::chrono::steady_clock::now();
            
            while (!try_pop(item))
            {
                if (std::chrono::steady_clock::now() - start_time >= timeout)
                {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            
            return true;
        }
        
        // 批量操作
        
        /**
         * @brief 批量入队操作
         * 
         * @tparam InputIt 输入迭代器类型
         * @param first 起始迭代器
         * @param last 结束迭代器
         */
        template<typename InputIt>
        void push_range(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
            {
                push(*first);
            }
        }
        
        /**
         * @brief 批量出队操作
         * 
         * @tparam OutputIt 输出迭代器类型
         * @param first 输出迭代器
         * @param n 出队元素数量
         */
        template<typename OutputIt>
        void pop_range(OutputIt first, size_type n)
        {
            for (size_type i = 0; i < n; ++i)
            {
                T item;
                pop(item);
                *first++ = std::move(item);
            }
        }
        
        // 查找和算法
        
        /**
         * @brief 检查队列是否包含指定元素
         * 
         * @param value 要查找的元素
         * @return bool 包含返回true，否则返回false
         */
        bool contains(const T& value) const
        {
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            
            for (size_type pos = dequeue_pos; pos < enqueue_pos; ++pos)
            {
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                if (seq == pos + 1 && slot_ptr->data.load(std::memory_order_acquire) == value)
                {
                    return true;
                }
            }
            
            return false;
        }
        
        /**
         * @brief 统计指定元素的数量
         * 
         * @param value 要统计的元素
         * @return size_type 元素数量
         */
        size_type count(const T& value) const
        {
            size_type count = 0;
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            
            for (size_type pos = dequeue_pos; pos < enqueue_pos; ++pos)
            {
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                if (seq == pos + 1 && slot_ptr->data.load(std::memory_order_acquire) == value)
                {
                    ++count;
                }
            }
            
            return count;
        }
        
        /**
         * @brief 查找第一个匹配的元素
         * 
         * @tparam Predicate 谓词类型
         * @param pred 谓词函数
         * @param item 接收找到元素的引用
         * @return bool 找到返回true，否则返回false
         */
        template<typename Predicate>
        bool find_if(Predicate pred, T& item) const
        {
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            
            for (size_type pos = dequeue_pos; pos < enqueue_pos; ++pos)
            {
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                if (seq == pos + 1)
                {
                    T current = slot_ptr->data.load(std::memory_order_acquire);
                    if (pred(current))
                    {
                        item = current;
                        return true;
                    }
                }
            }
            
            return false;
        }
        
        // 快照和遍历
        
        /**
         * @brief 获取队列快照
         * 
         * @return std::vector<T> 当前所有元素的副本
         */
        std::vector<T> snapshot() const
        {
            std::vector<T> result;
            
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            
            for (size_type pos = dequeue_pos; pos < enqueue_pos; ++pos)
            {
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                // 检查槽位是否有效
                if (seq == pos + 1)
                {
                    result.push_back(slot_ptr->data.load(std::memory_order_acquire));
                }
            }
            
            return result;
        }
        
        /**
         * @brief 对每个元素执行函数
         * 
         * @tparam Func 函数类型
         * @param func 要执行的函数
         */
        template<typename Func>
        void for_each(Func&& func) const
        {
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            
            for (size_type pos = dequeue_pos; pos < enqueue_pos; ++pos)
            {
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                if (seq == pos + 1)
                {
                    func(slot_ptr->data.load(std::memory_order_acquire));
                }
            }
        }
        
        /**
         * @brief 对每个元素及其索引执行函数
         * 
         * @tparam Func 函数类型
         * @param func 要执行的函数，接受 (index, value) 参数
         */
        template<typename Func>
        void for_each_indexed(Func&& func) const
        {
            size_type dequeue_pos = _dequeue_pos.load(std::memory_order_acquire);
            size_type enqueue_pos = _enqueue_pos.load(std::memory_order_acquire);
            size_type index = 0;
            
            for (size_type pos = dequeue_pos; pos < enqueue_pos; ++pos)
            {
                slot* slot_ptr = &_buffer[pos & _mask];
                size_type seq = slot_ptr->sequence.load(std::memory_order_acquire);
                
                if (seq == pos + 1)
                {
                    func(index++, slot_ptr->data.load(std::memory_order_acquire));
                }
            }
        }
    };
    
    // 非成员函数
    
    /**
     * @brief 交换两个 atomic_annular_queue
     * 
     * @tparam T 元素类型
     * @param lhs 第一个队列
     * @param rhs 第二个队列
     */
    template<typename T>
    void swap(atomic_annular_queue<T>& lhs, atomic_annular_queue<T>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
    
    /**
     * @brief 比较两个 atomic_annular_queue 是否相等
     * 
     * @tparam T 元素类型
     * @param lhs 第一个队列
     * @param rhs 第二个队列
     * @return bool 如果相等则返回true
     */
    template<typename T>
    bool operator==(const atomic_annular_queue<T>& lhs, const atomic_annular_queue<T>& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return false;
        }
        
        auto lhs_snapshot = lhs.snapshot();
        auto rhs_snapshot = rhs.snapshot();
        
        return std::equal(lhs_snapshot.begin(), lhs_snapshot.end(), rhs_snapshot.begin());
    }
    
    /**
     * @brief 比较两个 atomic_annular_queue 是否不相等
     * 
     * @tparam T 元素类型
     * @param lhs 第一个队列
     * @param rhs 第二个队列
     * @return bool 如果不相等则返回true
     */
    template<typename T>
    bool operator!=(const atomic_annular_queue<T>& lhs, const atomic_annular_queue<T>& rhs)
    {
        return !(lhs == rhs);
    }
    
} // namespace wang

#endif // ATOMIC_ANNULAR_QUEUE_HPP