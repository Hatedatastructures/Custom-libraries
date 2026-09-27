/**
 * @file Imitate.hpp
 * @brief 默认比较函数对象。
 */
#pragma once
#include "Exception.hpp"
namespace ImitationFunctions
{
/**
     * @brief 使用小于运算符比较两个值。
     * @tparam ImitationFunctionsLess 被比较的值类型。
     */
    template <typename ImitationFunctionsLess>
    class Less
    {
    public:
        /**
         * @brief 比较两个值是否满足小于关系。
         * @param Test1 第一个比较值。
         * @param Test2 第二个比较值。
         * @return 条件成立时返回 true，否则返回 false。
         * @note 比较操作抛出的异常会向调用方传播。
         */
        constexpr bool operator()(const ImitationFunctionsLess &Test1,
            const ImitationFunctionsLess &Test2)
            noexcept(noexcept(Test1 < Test2))
        {
            return Test1 < Test2;
        }
    };
/**
     * @brief 使用大于运算符比较两个值。
     * @tparam ImitationFunctionsGreater 被比较的值类型。
     */
    template <typename ImitationFunctionsGreater>
    class Greater
    {
    public:
        /**
         * @brief 比较两个值是否满足大于关系。
         * @param Test1 第一个比较值。
         * @param Test2 第二个比较值。
         * @return 条件成立时返回 true，否则返回 false。
         * @note 比较操作抛出的异常会向调用方传播。
         */
        constexpr bool operator()(const ImitationFunctionsGreater &Test1,
            const ImitationFunctionsGreater &Test2)
            noexcept(noexcept(Test1 > Test2))
        {
            return Test1 > Test2;
        }
    };
}
namespace StandardCon
{
    using ImitationFunctions::Greater;
    using ImitationFunctions::Less;
}
