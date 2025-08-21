#ifndef ATOMIC_ARRAY_HPP
#define ATOMIC_ARRAY_HPP

#include <atomic>
#include <array>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <vector>
#include <type_traits>
#include <iterator>
#include <memory>

namespace wang
{
    /**
     * @brief 无锁线程安全的固定大小数组
     * 
     * atomic_array 是一个基于原子操作的固定大小数组容器，提供线程安全的元素访问和修改。
     * 使用 std::atomic<T> 存储每个元素，支持原子操作和并发访问。
     * 
     * 特性：
     * - 使用原子操作确保线程安全
     * - 固定大小，编译时确定
     * - 支持多生产者多消费者
     * - 提供与标准库 std::array 兼容的接口
     * - 使用蛇形命名法
     * 
     * @tparam T 元素类型，必须支持原子操作
     * @tparam N 数组大小
     */
    template<typename T, std::size_t N>
    class atomic_array
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
        using atomic_type = std::atomic<T>;
        
        // 迭代器类型（注意：原子数组的迭代器需要特殊处理）
        class iterator;
        class const_iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        
    private:
        std::array<std::atomic<T>, N> _data;
        
    public:
        // 构造函数
        
        /**
         * @brief 默认构造函数
         * 
         * 创建一个所有元素都使用默认值初始化的数组。
         */
        atomic_array() = default;
        
        /**
         * @brief 使用初始化列表构造
         * 
         * @param init_list 初始化列表
         */
        atomic_array(std::initializer_list<T> init_list)
        {
            auto it = init_list.begin();
            for (size_type i = 0; i < N && it != init_list.end(); ++i, ++it)
            {
                _data[i].store(*it, std::memory_order_relaxed);
            }
        }
        
        /**
         * @brief 使用单个值填充构造
         * 
         * @param value 填充值
         */
        explicit atomic_array(const T& value)
        {
            fill(value);
        }
        
        /**
         * @brief 拷贝构造函数
         * 
         * @param other 另一个 atomic_array
         */
        atomic_array(const atomic_array& other)
        {
            for (size_type i = 0; i < N; ++i)
            {
                _data[i].store(other._data[i].load(std::memory_order_acquire), std::memory_order_relaxed);
            }
        }
        
        /**
         * @brief 移动构造函数
         * 
         * @param other 另一个 atomic_array
         */
        atomic_array(atomic_array&& other) noexcept
        {
            for (size_type i = 0; i < N; ++i)
            {
                _data[i].store(other._data[i].load(std::memory_order_acquire), std::memory_order_relaxed);
            }
        }
        
        // 析构函数
        ~atomic_array() = default;
        
        // 赋值运算符
        
        /**
         * @brief 拷贝赋值运算符
         * 
         * @param other 另一个 atomic_array
         * @return atomic_array& 自身引用
         */
        atomic_array& operator=(const atomic_array& other)
        {
            if (this != &other)
            {
                for (size_type i = 0; i < N; ++i)
                {
                    _data[i].store(other._data[i].load(std::memory_order_acquire), std::memory_order_release);
                }
            }
            return *this;
        }
        
        /**
         * @brief 移动赋值运算符
         * 
         * @param other 另一个 atomic_array
         * @return atomic_array& 自身引用
         */
        atomic_array& operator=(atomic_array&& other) noexcept
        {
            if (this != &other)
            {
                for (size_type i = 0; i < N; ++i)
                {
                    _data[i].store(other._data[i].load(std::memory_order_acquire), std::memory_order_release);
                }
            }
            return *this;
        }
        
        /**
         * @brief 初始化列表赋值
         * 
         * @param init_list 初始化列表
         * @return atomic_array& 自身引用
         */
        atomic_array& operator=(std::initializer_list<T> init_list)
        {
            auto it = init_list.begin();
            for (size_type i = 0; i < N; ++i)
            {
                if (it != init_list.end())
                {
                    _data[i].store(*it, std::memory_order_release);
                    ++it;
                }
                else
                {
                    _data[i].store(T{}, std::memory_order_release);
                }
            }
            return *this;
        }
        
        // 元素访问
        
        /**
         * @brief 安全的元素访问（带边界检查）
         * 
         * @param pos 位置索引
         * @return T 元素值
         * @throws std::out_of_range 如果索引越界
         */
        T at(size_type pos) const
        {
            if (pos >= N)
            {
                throw std::out_of_range("atomic_array::at: index out of range");
            }
            return _data[pos].load(std::memory_order_acquire);
        }
        
        /**
         * @brief 安全的元素设置（带边界检查）
         * 
         * @param pos 位置索引
         * @param value 新值
         * @throws std::out_of_range 如果索引越界
         */
        void set_at(size_type pos, const T& value)
        {
            if (pos >= N)
            {
                throw std::out_of_range("atomic_array::set_at: index out of range");
            }
            _data[pos].store(value, std::memory_order_release);
        }
        
        /**
         * @brief 元素访问运算符（不检查边界）
         * 
         * @param pos 位置索引
         * @return T 元素值
         */
        T operator[](size_type pos) const
        {
            return _data[pos].load(std::memory_order_acquire);
        }
        
        /**
         * @brief 设置指定位置的元素值
         * 
         * @param pos 位置索引
         * @param value 新值
         */
        void set(size_type pos, const T& value)
        {
            _data[pos].store(value, std::memory_order_release);
        }
        
        /**
         * @brief 获取第一个元素
         * 
         * @return T 第一个元素的值
         */
        T front() const
        {
            return _data[0].load(std::memory_order_acquire);
        }
        
        /**
         * @brief 设置第一个元素
         * 
         * @param value 新值
         */
        void set_front(const T& value)
        {
            _data[0].store(value, std::memory_order_release);
        }
        
        /**
         * @brief 获取最后一个元素
         * 
         * @return T 最后一个元素的值
         */
        T back() const
        {
            return _data[N - 1].load(std::memory_order_acquire);
        }
        
        /**
         * @brief 设置最后一个元素
         * 
         * @param value 新值
         */
        void set_back(const T& value)
        {
            _data[N - 1].store(value, std::memory_order_release);
        }
        
        /**
         * @brief 获取原始数据指针（注意：返回的是原子类型的指针）
         * 
         * @return atomic_type* 原子数据指针
         */
        atomic_type* data() noexcept
        {
            return _data.data();
        }
        
        /**
         * @brief 获取原始数据指针（常量版本）
         * 
         * @return const atomic_type* 常量原子数据指针
         */
        const atomic_type* data() const noexcept
        {
            return _data.data();
        }
        
        // 容量
        
        /**
         * @brief 检查数组是否为空
         * 
         * @return bool 如果数组大小为0则返回true
         */
        constexpr bool empty() const noexcept
        {
            return N == 0;
        }
        
        /**
         * @brief 获取数组大小
         * 
         * @return size_type 数组大小
         */
        constexpr size_type size() const noexcept
        {
            return N;
        }
        
        /**
         * @brief 获取最大可能的大小
         * 
         * @return size_type 最大大小
         */
        constexpr size_type max_size() const noexcept
        {
            return N;
        }
        
        // 修改操作
        
        /**
         * @brief 用指定值填充整个数组
         * 
         * @param value 填充值
         */
        void fill(const T& value)
        {
            for (size_type i = 0; i < N; ++i)
            {
                _data[i].store(value, std::memory_order_release);
            }
        }
        
        /**
         * @brief 交换两个数组的内容
         * 
         * @param other 另一个数组
         */
        void swap(atomic_array& other) noexcept
        {
            for (size_type i = 0; i < N; ++i)
            {
                T temp = _data[i].load(std::memory_order_acquire);
                _data[i].store(other._data[i].load(std::memory_order_acquire), std::memory_order_release);
                other._data[i].store(temp, std::memory_order_release);
            }
        }
        
        // 快照和遍历
        
        /**
         * @brief 获取数组的快照
         * 
         * @return std::array<T, N> 当前数组状态的快照
         */
        std::array<T, N> snapshot() const
        {
            std::array<T, N> result;
            for (size_type i = 0; i < N; ++i)
            {
                result[i] = _data[i].load(std::memory_order_acquire);
            }
            return result;
        }
        
        /**
         * @brief 获取数组的快照（vector版本）
         * 
         * @return std::vector<T> 当前数组状态的快照
         */
        std::vector<T> snapshot_vector() const
        {
            std::vector<T> result;
            result.reserve(N);
            for (size_type i = 0; i < N; ++i)
            {
                result.push_back(_data[i].load(std::memory_order_acquire));
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
            for (size_type i = 0; i < N; ++i)
            {
                func(_data[i].load(std::memory_order_acquire));
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
            for (size_type i = 0; i < N; ++i)
            {
                func(i, _data[i].load(std::memory_order_acquire));
            }
        }
        
        // 原子操作
        
        /**
         * @brief 原子交换操作
         * 
         * @param pos 位置索引
         * @param value 新值
         * @return T 旧值
         */
        T exchange(size_type pos, const T& value)
        {
            return _data[pos].exchange(value, std::memory_order_acq_rel);
        }
        
        /**
         * @brief 弱比较交换操作
         * 
         * @param pos 位置索引
         * @param expected 期望值
         * @param desired 目标值
         * @return bool 是否成功交换
         */
        bool compare_exchange_weak(size_type pos, T& expected, const T& desired)
        {
            return _data[pos].compare_exchange_weak(expected, desired, std::memory_order_acq_rel);
        }
        
        /**
         * @brief 强比较交换操作
         * 
         * @param pos 位置索引
         * @param expected 期望值
         * @param desired 目标值
         * @return bool 是否成功交换
         */
        bool compare_exchange_strong(size_type pos, T& expected, const T& desired)
        {
            return _data[pos].compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
        }
        
        // 数值原子操作（仅对数值类型有效）
        
        /**
         * @brief 原子加法操作
         * 
         * @param pos 位置索引
         * @param value 要加的值
         * @return T 操作前的值
         */
        template<typename U = T>
        typename std::enable_if<std::is_arithmetic<U>::value, T>::type
        fetch_add(size_type pos, const T& value)
        {
            return _data[pos].fetch_add(value, std::memory_order_acq_rel);
        }
        
        /**
         * @brief 原子减法操作
         * 
         * @param pos 位置索引
         * @param value 要减的值
         * @return T 操作前的值
         */
        template<typename U = T>
        typename std::enable_if<std::is_arithmetic<U>::value, T>::type
        fetch_sub(size_type pos, const T& value)
        {
            return _data[pos].fetch_sub(value, std::memory_order_acq_rel);
        }
        
        // 位运算原子操作（仅对整数类型有效）
        
        /**
         * @brief 原子按位与操作
         * 
         * @param pos 位置索引
         * @param value 要与的值
         * @return T 操作前的值
         */
        template<typename U = T>
        typename std::enable_if<std::is_integral<U>::value, T>::type
        fetch_and(size_type pos, const T& value)
        {
            return _data[pos].fetch_and(value, std::memory_order_acq_rel);
        }
        
        /**
         * @brief 原子按位或操作
         * 
         * @param pos 位置索引
         * @param value 要或的值
         * @return T 操作前的值
         */
        template<typename U = T>
        typename std::enable_if<std::is_integral<U>::value, T>::type
        fetch_or(size_type pos, const T& value)
        {
            return _data[pos].fetch_or(value, std::memory_order_acq_rel);
        }
        
        /**
         * @brief 原子按位异或操作
         * 
         * @param pos 位置索引
         * @param value 要异或的值
         * @return T 操作前的值
         */
        template<typename U = T>
        typename std::enable_if<std::is_integral<U>::value, T>::type
        fetch_xor(size_type pos, const T& value)
        {
            return _data[pos].fetch_xor(value, std::memory_order_acq_rel);
        }
        
        // 查找和算法
        
        /**
         * @brief 查找第一个匹配的元素位置
         * 
         * @param value 要查找的值
         * @return size_type 第一个匹配元素的索引，如果未找到则返回 size()
         */
        size_type find_first(const T& value) const
        {
            for (size_type i = 0; i < N; ++i)
            {
                if (_data[i].load(std::memory_order_acquire) == value)
                {
                    return i;
                }
            }
            return N;
        }
        
        /**
         * @brief 查找最后一个匹配的元素位置
         * 
         * @param value 要查找的值
         * @return size_type 最后一个匹配元素的索引，如果未找到则返回 size()
         */
        size_type find_last(const T& value) const
        {
            for (size_type i = N; i > 0; --i)
            {
                if (_data[i - 1].load(std::memory_order_acquire) == value)
                {
                    return i - 1;
                }
            }
            return N;
        }
        
        /**
         * @brief 统计指定值的出现次数
         * 
         * @param value 要统计的值
         * @return size_type 出现次数
         */
        size_type count(const T& value) const
        {
            size_type count = 0;
            for (size_type i = 0; i < N; ++i)
            {
                if (_data[i].load(std::memory_order_acquire) == value)
                {
                    ++count;
                }
            }
            return count;
        }
        
        /**
         * @brief 检查是否包含指定值
         * 
         * @param value 要检查的值
         * @return bool 如果包含则返回true
         */
        bool contains(const T& value) const
        {
            return find_first(value) != N;
        }
        
        // 数值算法（仅对数值类型有效）
        
        /**
         * @brief 计算所有元素的和
         * 
         * @return T 所有元素的和
         */
        template<typename U = T>
        typename std::enable_if<std::is_arithmetic<U>::value, T>::type
        sum() const
        {
            T result = T{};
            for (size_type i = 0; i < N; ++i)
            {
                result += _data[i].load(std::memory_order_acquire);
            }
            return result;
        }
        
        /**
         * @brief 找到最小元素
         * 
         * @return T 最小元素的值
         */
        T min() const
        {
            if (N == 0) return T{};
            
            T result = _data[0].load(std::memory_order_acquire);
            for (size_type i = 1; i < N; ++i)
            {
                T current = _data[i].load(std::memory_order_acquire);
                if (current < result)
                {
                    result = current;
                }
            }
            return result;
        }
        
        /**
         * @brief 找到最大元素
         * 
         * @return T 最大元素的值
         */
        T max() const
        {
            if (N == 0) return T{};
            
            T result = _data[0].load(std::memory_order_acquire);
            for (size_type i = 1; i < N; ++i)
            {
                T current = _data[i].load(std::memory_order_acquire);
                if (current > result)
                {
                    result = current;
                }
            }
            return result;
        }
    };
    
    // 非成员函数
    
    /**
     * @brief 交换两个 atomic_array
     * 
     * @tparam T 元素类型
     * @tparam N 数组大小
     * @param lhs 第一个数组
     * @param rhs 第二个数组
     */
    template<typename T, std::size_t N>
    void swap(atomic_array<T, N>& lhs, atomic_array<T, N>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
    
    /**
     * @brief 比较两个 atomic_array 是否相等
     * 
     * @tparam T 元素类型
     * @tparam N 数组大小
     * @param lhs 第一个数组
     * @param rhs 第二个数组
     * @return bool 如果相等则返回true
     */
    template<typename T, std::size_t N>
    bool operator==(const atomic_array<T, N>& lhs, const atomic_array<T, N>& rhs)
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            if (lhs[i] != rhs[i])
            {
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief 比较两个 atomic_array 是否不相等
     * 
     * @tparam T 元素类型
     * @tparam N 数组大小
     * @param lhs 第一个数组
     * @param rhs 第二个数组
     * @return bool 如果不相等则返回true
     */
    template<typename T, std::size_t N>
    bool operator!=(const atomic_array<T, N>& lhs, const atomic_array<T, N>& rhs)
    {
        return !(lhs == rhs);
    }
    
} // namespace wang

#endif // ATOMIC_ARRAY_HPP