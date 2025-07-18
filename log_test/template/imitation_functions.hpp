#include "custom_exception.hpp"
namespace con
{
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
    template<typename imitation_functions_greater>
    class greater
    {
    public:
        constexpr bool operator()(const imitation_functions_greater& _test1 ,const imitation_functions_greater& _test2) noexcept
        {
            return _test1 > _test2;
        }
    };
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
