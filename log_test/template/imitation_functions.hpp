#pragma once
#include "Custom_exception.hpp"
namespace con
{
    /*
    * @brief  #### `imitation_functions` 仿函数命名空间
    *     *   - 包含自定义的仿函数（函数对象）实现，用于实现比较操作
    */
    namespace imitation_functions{}
}

namespace con::imitation_functions
{
    /*
     * @brief  #### `less` 类

    *   - 仿函数（函数对象）实现，用于比较两个同类型对象的大小关系
    
    *   - 采用小于运算符（`<`）定义比较逻辑，符合严格弱序规则

     * 模板参数:

     * * - `imitation_functions_less`: 待比较的对象类型，需支持 `<` 运算符重载

     * 核心操作符:
     * * - `constexpr bool operator()(const imitation_functions_less& _test1, const imitation_functions_less& _test2) noexcept`
     * 
     *     - 功能：比较两个对象的大小，返回 `_test1 < _test2` 的结果
     * 
     *     - 参数：`_test1` 和 `_test2` 为待比较的常量引用
     * 
     *     - 返回值：若 `_test1` 小于 `_test2` 则返回 `true`，否则返回 `false`
     * 
     *     - 特性：`constexpr` 支持编译期计算，`noexcept` 保证不抛出异常

     * 适用场景:
     * * - 作为排序算法（如快速排序、归并排序）的比较器
     * 
     * * - 用于关联容器（如 `tree_map`、`tree_set`）的键值排序
     * 
     * * - 任何需要自定义小于比较逻辑的场景

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 

     * 注意事项:

     * * - 依赖类型 `imitation_functions_less` 已正确重载 `<` 运算符

     * * - 若用于自定义类型，需确保 `<` 运算符满足严格弱序（非自反、传递性等）

     * * - 线程安全：无共享状态，可在多线程环境中安全使用
    */
    template<typename imitation_functions_less>
    class less
    {
    public:
        constexpr bool operator()(const imitation_functions_less& _test1 ,const imitation_functions_less& _test2) noexcept
        {
            return _test1 < _test2;
        }
    };
    /*
     * @brief  #### `greater` 类模板

    *   - 实现"大于"比较的函数对象（仿函数）

    *   - 用于定义对象间的严格弱序关系，与标准库 `std::greater` 功能一致

     * 模板参数:

     * * - `imitation_functions_greater`: 待比较对象的类型（需支持 `>` 运算符重载）

     * 核心操作符:
     * 
     *     - 功能：比较两个对象的大小，返回 `_test1 > _test2` 的结果
     * 
     *     - 参数：`_test1` 和 `_test2` 为待比较的常量引用
     * 
     *     - 返回值：若 `_test1` 大于 `_test2` 则返回 `true`，否则返回 `false`
     * 
     *     - 特性：`constexpr` 支持编译期计算，`noexcept` 保证不抛出异常

     * 典型应用场景:

     * 
     * * - 作为关联容器（如 `std::map`、`std::set`）的键比较函数，构建降序容器
     * 
     * * - 用于自定义算法中的比较操作，替代默认的小于比较

     * 注意事项:

     * * - 依赖类型 `imitation_functions_greater` 正确实现 `>` 运算符
     * 
     * * - 比较操作需满足严格弱序关系（非自反性、传递性、反对称性）
     * 
     * * - 若用于指针类型，比较的是地址值而非指向的内容
     * 
     * * - 线程安全：无状态类，可在多线程环境中安全使用

     * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md 
    */
    template<typename imitation_functions_greater>
    class greater
    {
    public:
        constexpr bool operator()(const imitation_functions_greater& _test1 ,const imitation_functions_greater& _test2) noexcept
        {
            return _test1 > _test2;
        }
    };
    /*
     * @brief  #### `hash_imitation_functions` 类

    *   - 哈希仿函数实现，为基础数据类型提供哈希值计算

    *   - 支持多种内置类型的哈希转换，可作为哈希容器的默认哈希函数

     * 核心操作符:

     * * - 针对不同基础类型重载的 `operator()`，均返回 `size_t` 类型的哈希值
     * 
     *     - `int`: 直接转换为 `size_t`
     * 
     *     - `size_t`: 直接返回原值
     * 
     *     - `char`: 转换为对应的 `size_t` 值
     * 
     *     - `double`: 转换为 `size_t`（注意：可能丢失精度）
     * 
     *     - `float`: 转换为 `size_t`（注意：可能丢失精度）
     * 
     *     - `long`: 转换为 `size_t`
     * 
     *     - `short`: 转换为 `size_t`
     * 
     *     - `long long`: 转换为 `size_t`
     * 
     *     - `unsigned int`: 转换为 `size_t`
     * 
     *     - `unsigned short`: 转换为 `size_t`

     * 特性:

     * * - 针对基础类型的哈希计算为 O(1) 时间复杂度

     * 适用场景:

     * * - 作为 `hash_map`、`hash_set` 等哈希容器的哈希函数
     * 
     * * - 与 `hash_function` 类配合使用，扩展哈希算法
     * 
     * * - 需要为基础类型快速生成哈希值的场景

     * 注意事项:

     * * - 浮点类型（`double`、`float`）的哈希可能丢失精度，不建议用于精确匹配场景
     * 
     * * - 未提供自定义类型（如字符串、容器）的哈希实现，需用户自行重载扩展
     * 
     * * - 对于指针类型或复杂结构，需额外实现重载版本
     * 
     * * - 线程安全：无状态设计，可在多线程环境中安全使用

     * 扩展说明:

     * * - 注释中提供了字符串容器哈希计算的示例代码框架，可根据需求实现
     * 
     * * - 支持为自定义类型添加 `operator()` 重载，扩展哈希能力
     * 
    */
    class hash_imitation_functions
    {
    public:
        [[nodiscard]] size_t operator()(const int str_data)const noexcept
        {
            return static_cast<size_t>(str_data);
        }
        [[nodiscard]] size_t operator()(const size_t data_size_t)const noexcept
        {
            return data_size_t;
        }
        [[nodiscard]] size_t operator()(const char data_char)const noexcept
        {
            return static_cast<size_t>(data_char);
        }
        [[nodiscard]] size_t operator()(const double data_double)const noexcept
        {
            return static_cast<size_t>(data_double);
        }
        [[nodiscard]] size_t operator()(const float data_float)const noexcept
        {
            return static_cast<size_t>(data_float);
        }
        [[nodiscard]] size_t operator()(const long data_long)const noexcept
        {
            return static_cast<size_t>(data_long);
        }
        [[nodiscard]] size_t operator()(const short data_short)const noexcept
        {
            return static_cast<size_t>(data_short);
        }
        [[nodiscard]] size_t operator()(const long long data_long_long)const noexcept
        {
            return static_cast<size_t>(data_long_long);
        }
        [[nodiscard]] size_t operator()(const unsigned int data_unsigned)const noexcept
        {
            return static_cast<size_t>(data_unsigned);
        }
        [[nodiscard]] size_t operator()(const unsigned short data_unsigned_short)const noexcept
        {
            return static_cast<size_t>(data_unsigned_short);
        }
        

        // size_t operator()(const MY_Template::string_container::string& data_string)
        // {
        //     size_t hash_value = 0;
        //     for(size_t i = 0; i < data_string._size; ++i)
        //     {
        //         hash_value = hash_value * 31 + data_string._data[i];
        //     }
        //     return hash_value;
        // }
        //有需要可以重载本文件的string容器和vector容器.list容器等计算哈希的函数, 这里就不重载了
    };
}
