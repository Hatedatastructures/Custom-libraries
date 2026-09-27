/**
 * @file Utility.hpp
 * @brief 容器使用的二元值与构造辅助函数。
 */
#pragma once
#include "Exception.hpp"
namespace Practicality
{
    /**
     * @brief 按值保存两个元素的二元组。
     * @tparam PairDataTypeExampleT 第一个元素的类型。
     * @tparam PairDataTypeExampleK 第二个元素的类型。
     */
    template <typename PairDataTypeExampleT, typename PairDataTypeExampleK>
    class Pair
    {
        /** @brief 第一元素的类型别名。 */
        using T = PairDataTypeExampleT;
        /** @brief 第二元素的类型别名。 */
        using K = PairDataTypeExampleK;

    public:
        /** @brief 二元组的第一个值。 */
        T First;
        /** @brief 二元组的第二个值。 */
        K Second;
        /**
         * @brief 构造 Pair 对象。
         */
        Pair() = default;

        /**
         * @brief 构造 Pair 对象。
         * @param FirstValue 要复制到 `First` 的值。
         * @param SecondValue 要复制到 `Second` 的值。
         */
        Pair(const T &FirstValue, const K &SecondValue)
            : First(FirstValue), Second(SecondValue)
        {
        }
        /**
         * @brief 构造 Pair 对象。
         * @param Other 要复制的二元组。
         */
        Pair(const Pair &Other) = default;

        /**
         * @brief 构造 Pair 对象。
         * @param FirstValue 要移动到 `First` 的值。
         * @param SecondValue 要移动到 `Second` 的值。
         */
        Pair(T &&FirstValue, K &&SecondValue)
                : First(std::forward<T>(FirstValue)), Second(std::forward<K>(SecondValue))
        {
            ;
        }
        /**
         * @brief 构造 Pair 对象。
         * @param Other 要移动的二元组。
         */
        Pair(Pair &&Other) = default;
        /**
         * @brief 赋值当前对象。
         * @param Other 要复制的二元组。
         * @return 当前对象的引用。
         */
        Pair &operator=(const Pair &Other) = default;
        /**
         * @brief 赋值当前对象。
         * @param Other 要移动的二元组。
         * @return 当前对象的引用。
         */
        Pair &operator=(Pair &&Other) = default;
        /**
         * @brief 比较两个对象是否相等。
         * @param Other 要比较的二元组。
         * @return 两个元素分别相等时返回 true。
         */
        bool operator==(const Pair &Other) const
        {
            return (this == &Other) ? true : (First == Other.First && Second == Other.Second);
        }
        /**
         * @brief 比较两个对象是否不相等。
         * @param Other 要比较的二元组。
         * @return 两个二元组不相等时返回 true。
         */
        bool operator!=(const Pair &Other) const
        {
            return !(*this == Other);
        }
        /**
         * @brief 返回当前二元组的指针。
         * @return 指向当前可修改二元组的指针。
         */
        Pair *operator->() noexcept { return this; }
        /**
         * @brief 返回当前只读二元组的指针。
         * @return 指向当前只读二元组的指针。
         */
        const Pair *operator->() const noexcept { return this; }
        /**
         * @brief 向流写入对象。
         * @tparam PairOstreamT 二元组第一个元素的类型。
         * @tparam PairOstreamK 二元组第二个元素的类型。
         * @param Os 输出流。
         * @param P 要写入流的二元组。
         * @return 输入或输出流的引用。
         */
        template <typename PairOstreamT, typename PairOstreamK>
        friend std::ostream &operator<<(std::ostream &Os, const Pair<PairOstreamT, PairOstreamK> &P);
    };
    template <typename PairOstreamT, typename PairOstreamK>
    std::ostream &operator<<(std::ostream &Os, const Pair<PairOstreamT, PairOstreamK> &P)
    {
        Os << "(" << P.First << ":" << P.Second << ")";
        return Os;
    }
    /**
     * @brief 按值构造二元组。
     * @tparam MakePairT 第一个值的类型。
     * @tparam MakePairK 第二个值的类型。
     * @param First 第一个值。
     * @param Second 第二个值。
     * @return 保存两个值的 Pair。
     */
    template <typename MakePairT, typename MakePairK>
    Pair<MakePairT, MakePairK> MakePair(const MakePairT &First, const MakePairK &Second)
    {
        return Pair<MakePairT, MakePairK>(First, Second);
    }
}
namespace StandardCon
{
    using Practicality::MakePair;
    using Practicality::Pair;
}
