/**
 * @file Bloom.hpp
 * @brief 固定容量的布隆过滤器。
 */
#pragma once
#include "Algorithm.hpp"
#include "Base.hpp"
#include "Imitate.hpp"

#include <stdexcept>
namespace BloomFilterContainer
{
/**
     * @brief 使用三个哈希位置近似判断元素是否存在。
     * @tparam BloomFilterTypeValue 插入和查询的元素类型。
     * @tparam BloomFilterHashFunctor 默认构造的哈希对象，须提供
     *         `HashSdmmhash`、`HashDjbhash` 和 `HashPjwhash`；返回值须可转为
     *         `uint64_t` 并支持与容量取模。
     * @note 哈希对象对同一元素产生稳定映射时，不会漏报；阳性结果可能是假阳性。
     * @note 默认容量为 1000 bit，构造后固定；不支持删除，且不提供并发同步。
     * @note `Size()` 返回已置位的 bit 数，而不是插入元素数。
     */
    template <typename BloomFilterTypeValue, typename BloomFilterHashFunctor = StandardCon::HashFunction<BloomFilterTypeValue>>
    class BloomFilter
    {
        /** @brief 默认构造的三组哈希函数对象。 */
        BloomFilterHashFunctor HashFunctionsObject;
        /** @brief 存储布隆过滤器位状态的位集合类型。 */
        using BitSet = StandardCon::BitSet;
        /** @brief 保存所有哈希映射位的位集合。 */
        BitSet InstanceBitSet;
        /** @brief 可映射的 bit 数量。 */
        uint64_t MemberCapacity;

    public:
        /**
         * @brief 创建默认容量为 1000 bit 的过滤器。
         */
        BloomFilter()
        {
            MemberCapacity = 1000;
            InstanceBitSet.Resize(MemberCapacity);
        }

        /**
         * @brief 创建指定 bit 容量的过滤器。
         * @param TempCapacity bit 位容量，必须大于零。
         * @throws std::invalid_argument 容量为零时抛出。
         * @throws std::length_error 容量无法表示时抛出。
         * @throws std::bad_alloc 底层位数组分配失败时抛出。
         */
        explicit BloomFilter(const uint64_t &TempCapacity)
        {
            if (TempCapacity == 0)
            {
                throw std::invalid_argument(
                    "BloomFilter capacity must be greater than zero");
            }
            MemberCapacity = TempCapacity;
            InstanceBitSet.Resize(MemberCapacity);
        }

        /**
         * @brief 取得已置位的 bit 数。
         * @return 已置位的 bit 数，不代表插入元素数。
         */
        [[nodiscard]] uint64_t Size() const
        {
            return InstanceBitSet.Size();
        }

        /**
         * @brief 取得 bit 位容量。
         * @return bit 位容量。
         */
        [[nodiscard]] uint64_t Capacity() const
        {
            return MemberCapacity;
        }

        /**
         * @brief 检查元素映射到的位是否均已置位。
         * @param TempBfMapValue 要查询的元素。
         * @return 全部对应位均已置位时返回 `true`（可能存在）；否则返回 `false`。
         * @note 哈希函数对象抛出的异常会向调用方传播。
         */
        bool Test(const BloomFilterTypeValue &TempBfMapValue)
        {
            uint64_t PrimaryMappingLocation = HashFunctionsObject.HashSdmmhash(TempBfMapValue) % MemberCapacity;
            uint64_t SecondaryMappingLocation = HashFunctionsObject.HashDjbhash(TempBfMapValue) % MemberCapacity;
            uint64_t TertiaryMappingLocation = HashFunctionsObject.HashPjwhash(TempBfMapValue) % MemberCapacity;
            if (InstanceBitSet.Test(PrimaryMappingLocation) == true && InstanceBitSet.Test(SecondaryMappingLocation) == true &&
                    InstanceBitSet.Test(TertiaryMappingLocation) == true)
            {
                return true;
            }
            return false;
        }

        /**
         * @brief 设置元素映射到的位。
         * @param TempBfMapValue 要插入的元素。
         * @note 哈希函数对象抛出的异常会向调用方传播。
         */
        void Set(const BloomFilterTypeValue &TempBfMapValue)
        {
            uint64_t PrimaryMappingLocation = HashFunctionsObject.HashSdmmhash(TempBfMapValue) % MemberCapacity;
            uint64_t SecondaryMappingLocation = HashFunctionsObject.HashDjbhash(TempBfMapValue) % MemberCapacity;
            uint64_t TertiaryMappingLocation = HashFunctionsObject.HashPjwhash(TempBfMapValue) % MemberCapacity;
            InstanceBitSet.Set(PrimaryMappingLocation);
            InstanceBitSet.Set(SecondaryMappingLocation);
            InstanceBitSet.Set(TertiaryMappingLocation);
        }
    };
}
namespace StandardCon
{
    using BloomFilterContainer::BloomFilter;
}
