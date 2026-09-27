/**
 * @file Algorithm.hpp
 * @brief 本目录的迭代器算法。
 */
#pragma once
#include "Exception.hpp"
#include "Imitate.hpp"

#include <utility>
namespace StandardCon
{
    /**
     * @brief 迭代器算法的命名空间。
     */
    namespace Algorithm {}
}
namespace StandardCon::Algorithm
{
    /**
     * @brief 将源范围的元素依次复制到目标范围。
     * @tparam SourceSequenceCopy 源迭代器类型。
     * @tparam TargetSequenceCopy 目标迭代器类型。
     * @param Begin 源范围起始位置。
     * @param End 源范围结束位置，不包含该位置。
     * @param First 目标范围起始位置，须有足够可写空间且不与源范围重叠。
     * @return 复制结束后的目标迭代器。
     */
    template <typename SourceSequenceCopy, typename TargetSequenceCopy>
    constexpr TargetSequenceCopy Copy(SourceSequenceCopy Begin,
        SourceSequenceCopy End, TargetSequenceCopy First)
    {
        while (Begin != End)
        {
            *First = *Begin;
            ++Begin;
            ++First;
        }
        return First;
    }
    /**
     * @brief 在范围内查找首个与给定值相等的元素。
     * @tparam SourceSequenceFind 输入迭代器类型。
     * @tparam TargetSequenceFind 查找值类型。
     * @param Begin 范围起始迭代器。
     * @param End 范围尾后迭代器。
     * @param Value 要查找的值。
     * @return 首个匹配位置；未找到时返回 `End`。
     */
    template <typename SourceSequenceFind, typename TargetSequenceFind>
    constexpr SourceSequenceFind Find(SourceSequenceFind Begin,
        SourceSequenceFind End, const TargetSequenceFind& Value)
    {
        while (Begin != End)
        {
            if (*Begin == Value)
            {
                return Begin;
            }
            ++Begin;
        }
        return End;
    }
    /**
     * @brief 交换两个对象的值。
     * @tparam SwapDataType 对象类型。
     * @param A 第一个对象。
     * @param B 第二个对象。
     * @note 异常规格与 `std::swap(A, B)` 一致。
     */
    template <typename SwapDataType>
    constexpr void Swap(SwapDataType &A, SwapDataType &B)
        noexcept(noexcept(std::swap(A, B)))
    {
        std::swap(A, B);
    }
}
