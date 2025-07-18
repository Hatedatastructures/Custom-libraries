#include "custom_exception.hpp"
#include "imitation_functions.hpp"
namespace con
{
    /*
    * @brief  #### `algorithm` 命名空间

    *   - `copy`: 拷贝函数，将源序列拷贝到目标序列

    *   - `find`: 查找函数，在源序列中查找目标值

    *   - `swap`: 交换函数，交换两个变量的值

    *   - `hash_algorithm`: 哈希算法命名空间，提供多种哈希算法实现
*/
    namespace algorithm{}
}
namespace con::algorithm
{
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
    //返回下一个位置的迭代器，是否深浅拷贝取决于自定义类型重载和拷贝构造
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
    template<typename swap_data_type>
    constexpr void swap(swap_data_type& a,swap_data_type& b) noexcept
    {
        swap_data_type temp = a;
        a = b;
        b = temp;
    }
    /*
        * @brief  #### `hash_algorithm` 哈希算法命名空间

        *   - `hash_function`： 提供了五种哈希函数，提供多种哈希算法实现，基于基础哈希仿函数扩展计算
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
        template <typename hash_algorithm_type ,typename hash_if = con::imitation_functions::hash_imitation_functions>
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