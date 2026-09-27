/**
 * @file Hash.hpp
 * @brief 容器使用的哈希函数对象。
 */
#pragma once
#include "String.hpp"

#include <cstdint>
namespace Hash
{
    /**
     * @brief 将基础类型或字符串转换为 64 位哈希值。
     * @note 字符串哈希仅用于容器索引，不适合密码学用途。
     */
    class HashImitationFunctions
    {
    public:
        /**
         * @brief 计算输入值的 64 位哈希值。
         * @tparam HashType 要转换为哈希值的输入类型。
         * @param Data 要转换为哈希值的数据。
         * @return 转换得到的 64 位哈希值。
         */
        template <typename HashType>
        [[nodiscard]] uint64_t operator()(const HashType &Data)
            noexcept(noexcept(static_cast<uint64_t>(Data)))
        {
            return static_cast<uint64_t>(Data);
        }
        /**
         * @brief 计算输入值的 64 位哈希值。
         * @param DataString 要计算哈希值的字符串。
         * @return 字符串对应的 64 位哈希值。
         */
        [[nodiscard]] uint64_t operator()(const StandardCon::String &DataString) noexcept
        {
            std::uint64_t HashValue = UINT64_C(14695981039346656037);
            for (auto It = DataString.Begin(); It != DataString.End(); ++It)
            {
                HashValue ^= static_cast<unsigned char>(*It);
                HashValue *= UINT64_C(1099511628211);
            }
            return HashValue;
        }
        // 有需要可以重载本文件的string容器和vector容器.list容器等计算哈希的函数, 这里就不重载了
    };
    /**
     * @brief 哈希算法函数对象的命名空间。
     */
    namespace HashAlgorithm
    {
        /**
         * @brief 基于基础哈希器提供多种哈希计算方法。
         * @tparam HashAlgorithmType 输入数据类型。
         * @tparam HashIf 返回输入基础哈希值的函数对象类型。
         */
        template <typename HashAlgorithmType, typename HashIf = HashImitationFunctions>
        class HashFunction
        {
        public:
            /**
             * @brief 构造 HashFunction 对象。
             * @note 基础哈希器抛出的异常会向调用方传播。
             */
            constexpr HashFunction()
            {
                HashImitationFunctionsObject = HashIf();
            }
            /**
             * @brief 销毁 HashFunction 对象。
             */
            ~HashFunction() = default;
            /** @brief 用于计算初始哈希值的函数对象。 */
            HashIf HashImitationFunctionsObject;
            /**
             * @brief 计算 SDMMHASH 哈希值。
             * @param DataHash 待哈希的数据。
             * @return 64 位哈希值。
             * @note 基础哈希器抛出的异常会向调用方传播。
             */
            [[nodiscard]] constexpr uint64_t HashSdmmhash(const HashAlgorithmType &DataHash)
                noexcept(noexcept(HashImitationFunctionsObject(DataHash)))
            {
                uint64_t ReturnValue = HashImitationFunctionsObject(DataHash);
                ReturnValue = 65599 * ReturnValue;
                return ReturnValue;
            }
            /**
             * @brief 计算 BKDRHASH 哈希值。
             * @param DataHash 待哈希的数据。
             * @return 64 位哈希值。
             * @note 基础哈希器抛出的异常会向调用方传播。
             */
            [[nodiscard]] constexpr uint64_t HashBkdrhash(const HashAlgorithmType &DataHash)
                noexcept(noexcept(HashImitationFunctionsObject(DataHash)))
            {
                uint64_t ReturnValue = HashImitationFunctionsObject(DataHash);
                ReturnValue = 131 * ReturnValue;
                return ReturnValue;
            }
            /**
             * @brief 计算 DJBHASH 哈希值。
             * @param DataHash 待哈希的数据。
             * @return 64 位哈希值。
             * @note 基础哈希器抛出的异常会向调用方传播。
             */
            [[nodiscard]] constexpr uint64_t HashDjbhash(const HashAlgorithmType &DataHash)
                noexcept(noexcept(HashImitationFunctionsObject(DataHash)))
            {
                uint64_t ReturnValue = HashImitationFunctionsObject(DataHash);
                ReturnValue = 33 * ReturnValue;
                return ReturnValue;
            }
            /**
             * @brief 计算 APHASH 哈希值。
             * @param DataHash 待哈希的数据。
             * @return 64 位哈希值。
             * @note 基础哈希器抛出的异常会向调用方传播。
             */
            [[nodiscard]] constexpr uint64_t HashAphash(const HashAlgorithmType &DataHash)
                noexcept(noexcept(HashImitationFunctionsObject(DataHash)))
            {
                uint64_t ReturnValue = HashImitationFunctionsObject(DataHash);
                ReturnValue = ReturnValue * 1031;
                return ReturnValue;
            }
            /**
             * @brief 计算 PJWHASH 哈希值。
             * @param DataHash 待哈希的数据。
             * @return 64 位哈希值。
             * @note 基础哈希器抛出的异常会向调用方传播。
             */
            [[nodiscard]] constexpr uint64_t HashPjwhash(const HashAlgorithmType &DataHash)
                noexcept(noexcept(HashImitationFunctionsObject(DataHash)))
            {
                uint64_t ReturnValue = HashImitationFunctionsObject(DataHash);
                ReturnValue = (ReturnValue << 2) + ReturnValue;
                return ReturnValue;
            }
        };
    }
}
namespace StandardCon
{
    using Hash::HashImitationFunctions;
    using Hash::HashAlgorithm::HashFunction;
}
