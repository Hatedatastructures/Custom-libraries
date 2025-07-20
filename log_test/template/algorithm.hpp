#pragma once
#include "Custom_exception.hpp"
#include "Imitation_functions.hpp"
namespace con
{
    /*
    * @brief  #### `algorithm` 算法命名空间

    *   - `copy`: 拷贝函数，将源序列拷贝到目标序列

    *   - `find`: 查找函数，在源序列中查找目标值

    *   - `swap`: 交换函数，交换两个变量的值

    *   - `hash_algorithm`: 哈希算法命名空间，提供多种哈希算法实现
    */
   namespace algorithm{}
}
namespace con::algorithm
{
    /*
     * @brief  #### `copy` 函数模板

     *   - 将一个序列的元素从源范围复制到目标范围，保持元素顺序


     * 模板参数:

     * * - `source_sequence_copy`: 源序列的迭代器类型

     * * - `target_sequence_copy`: 目标序列的迭代器类型

     * 参数:

     * * - `begin`: 源序列的起始迭代器（包含）
     * 
     * * - `end`: 源序列的结束迭代器（不包含）
     * 
     * * - `first`: 目标序列的起始位置

     * 返回值:

     * * - 返回目标序列中最后一个被复制元素的下一个位置的指针（即 `&first + (end - begin)`）

     * 特性:
     * * - `constexpr`: 支持编译期计算（如果参数是编译期常量）
     * 
     * * - `noexcept`: 保证不抛出异常
     * 
     * * - 要求: 目标序列必须有足够空间容纳源序列的所有元素
     * 
     * * - 复杂度: 线性时间，O(n)，其中 n = end - begin

     * 注意事项:

     * * - 源序列和目标序列不能重叠
     * 
     * * - 目标序列必须有足够的容量，否则会导致未定义行为
     * 
     * * - 迭代器类型必须支持解引用（`*it`）和递增（`++it`）操作
     * 
     * * - 元素类型必须支持赋值操作（`*target = *source`）

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 
    */
    template <typename source_sequence_copy,typename target_sequence_copy>
    constexpr target_sequence_copy* copy(source_sequence_copy begin,source_sequence_copy end,target_sequence_copy first) noexcept
    {
        target_sequence_copy* return_first = &first;
        while(begin != end)
        {
            *first = *begin;
            ++begin;
            ++first;
        }
        return return_first;
    }
    /*
     * @brief  #### `find` 函数模板

     *   - 在指定范围内查找第一个等于给定值的元素，返回其迭代器

     * 模板参数:

     * * - `source_sequence_find`: 序列的迭代器类型
     * 
     * * - `target_sequence_find`: 查找值的类型

     * 参数:

     * * - `begin`: 搜索范围的起始迭代器（包含）
     * 
     * * - `end`: 搜索范围的结束迭代器（不包含）
     * 
     * * - `value`: 需要查找的值（常量引用）

     * 返回值:

     * * - 若找到匹配元素，返回指向该元素的迭代器
     * 
     * * - 若未找到，返回结束迭代器 `end`

     * 特性:

     * * - `constexpr`: 支持编译期计算（如果参数是编译期常量）
     * 
     * * - `noexcept`: 保证不抛出异常
     * 
     * * - 复杂度: 线性时间，O(n)，其中 n = end - begin

     * 注意事项:

     * * - 迭代器类型必须支持解引用（`*it`）、递增（`++it`）和比较（`it != end`）操作
     * 
     * * - 元素类型必须支持与查找值的相等比较（`*it == value`）
     * 
     * * - 仅返回第一个匹配元素的迭代器，若需查找所有匹配项，需多次调用
     * 
     * * - 空范围（`begin == end`）会直接返回 `end`
     * 
     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 
     * 
    */
    template<typename source_sequence_find,typename target_sequence_find>
    constexpr source_sequence_find find(source_sequence_find begin,source_sequence_find end,const target_sequence_find& value) noexcept
    {
        while(begin!= end)
        {
            if(*begin == value)
            {
                return begin;
            }
            ++begin;
        }
        return end;
    } 
    /*

     * @brief  #### `swap` 函数模板

        *   - 交换两个同类型对象的值，实现元素级别的值交换

        *   - 与标准库 `std::swap` 功能类似的自定义实现

        * 模板参数:

        * * - `swap_data_type`: 待交换对象的类型，需支持拷贝构造和赋值操作

        * 参数:

        * * - `a`: 第一个待交换的对象（引用传递）

        * * - `b`: 第二个待交换的对象（引用传递）

        * 特性:

        * * - `constexpr`: 支持编译期交换（如果参数是编译期常量）

        * * - `noexcept`: 保证不抛出异常（前提是类型的拷贝构造和赋值操作不抛异常）

        * * - 复杂度: 常数时间 O(1)

        * * - 操作: 通过临时变量中转，完成 `a` 和 `b` 的值交换

        * 注意事项:

        * * - 依赖类型 `swap_data_type` 支持拷贝构造（`swap_data_type temp = a`）和赋值操作（`a = b`、`b = temp`）

        * * - 对于大型对象，此实现可能因拷贝操作导致性能损耗（可考虑为特定类型提供重载版本优化）

        * * - 交换后，`a` 持有原 `b` 的值，`b` 持有原 `a` 的值

        * * - 不支持对同一对象的自交换（`swap(a, a)` 虽语法合法，但会产生冗余操作）

        * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 
    */
    template<typename swap_data_type>
    constexpr void swap(swap_data_type& a,swap_data_type& b) noexcept
    {
        swap_data_type temp = a;
        a = b;
        b = temp;
    }
    /*
        * @brief  #### `hash_algorithm` 哈希算法命名空间
        *
        *   - `hash_function`： 提供了五种哈希函数，提供多种哈希算法实现，基于基础哈希仿函数扩展计算
        *
    */
    namespace hash_algorithm
    {
        /*
            * @brief  #### `hash_function` 类

            *   - 哈希函数适配器，提供多种哈希算法实现，基于基础哈希仿函数扩展计算

            *   - 支持对不同类型数据进行哈希计算，适用于哈希表、布隆过滤器等场景

            * 模板参数:

            * * - `hash_algorithm_type`: 待计算哈希值的数据类型
            * 
            * * - `hash_if`: 基础哈希仿函数类型，默认为 `con::imitation_functions::hash_imitation_functions`
            * 
            *   （需实现 `operator()` 用于生成基础哈希值）

            * 构造与析构:

            * * - `constexpr hash_function()`:  constexpr 构造函数，初始化基础哈希仿函数对象
            * 
            * * - 析构函数: 默认析构，无需额外资源清理

            * 核心哈希方法:

            * * - `hash_sdmmhash(const hash_algorithm_type& data_hash)`
            * 
            * * - `hash_bkdrhash(const hash_algorithm_type& data_hash)`
            * 
            * * - `hash_djbhash(const hash_algorithm_type& data_hash)`
            * 
            * * - `hash_aphash(const hash_algorithm_type& data_hash)`
            * 
            * * - `hash_pjwhash(const hash_algorithm_type& data_hash)`

            * 方法特性:

            * * - 所有哈希方法均为 `constexpr`，支持编译期计算

            * 适用场景:
            * * - 哈希表（如 `hash_map`、`hash_set`）的键值哈希计算
            * 
            * * - 布隆过滤器（`bloom_filter`）中的多哈希函数实现
            * 
            * * - 需要自定义哈希逻辑的容器或算法

            * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 

            * 注意事项:
            * * - 依赖基础哈希仿函数 `hash_if` 对 `hash_algorithm_type` 类型的支持（需正确实现 `operator()`）
            * 
            * * - 哈希结果的分布性依赖基础哈希值和乘数的选择，不同算法适用于不同数据特征
            * 
            * * - 多线程安全：无共享状态，可在多线程环境中安全使用
            * 
            * * - 自定义类型需确保基础哈希仿函数能正确处理，或提供自定义类型哈希仿函数
        */
        template <typename hash_algorithm_type, typename hash_if = con::imitation_functions::hash_imitation_functions>
        class hash_function
        {
        public:
            constexpr hash_function()
            {
                hash_imitation_functions_object = hash_if();
            }
            ~hash_function() = default;
            hash_if hash_imitation_functions_object;
            [[nodiscard]] constexpr size_t hash_sdmmhash(const hash_algorithm_type& data_hash) noexcept
            {
                size_t return_value = hash_imitation_functions_object(data_hash);
                return_value = 65599 * return_value;
                return return_value;
            }
            [[nodiscard]] constexpr size_t hash_bkdrhash(const hash_algorithm_type& data_hash) noexcept
            {
                size_t return_value = hash_imitation_functions_object(data_hash);
                return_value = 131 * return_value;
                return return_value;
            }
            [[nodiscard]] constexpr size_t hash_djbhash(const hash_algorithm_type& data_hash) noexcept
            {
                size_t return_value = hash_imitation_functions_object(data_hash);
                return_value = 33 * return_value;
                return return_value;
            }
            [[nodiscard]] constexpr size_t hash_aphash(const hash_algorithm_type& data_hash) noexcept
            {
                size_t return_value = hash_imitation_functions_object(data_hash);
                return_value = return_value * 1031;
                return return_value;
            }
            [[nodiscard]] constexpr size_t hash_pjwhash(const hash_algorithm_type& data_hash) noexcept
            {
                size_t return_value = hash_imitation_functions_object(data_hash);
                return_value = (return_value << 2) + return_value;
                return return_value;
            }
        };
    }
}