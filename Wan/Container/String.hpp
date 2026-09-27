/**
 * @file String.hpp
 * @brief 可修改的字节字符串。
 */
#pragma once
#include "Exception.hpp"
#include "Algorithm.hpp"

#include <algorithm>
#include <iterator>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
namespace StringContainer
{
/**
     * @brief 管理以空字符结尾的可修改字节字符串。
     * @note 长度不含终止符；修改或重新分配空间后，已有字符指针和迭代器可能失效。
     */
    class String
    {
    private:
        /** @brief 以空字符结尾的字符缓冲区；空字符串可为空指针。 */
        char *MemberData = nullptr;
        /** @brief 字符串长度，不包含结尾空字符。 */
        uint64_t MemberSize = 0;
        /** @brief 当前缓冲区可容纳的字符数，不包含结尾空字符。 */
        uint64_t MemberCapacity = 0;

        /**
         * @brief 返回平台可表示的最大字符串长度。
         * @return 可存储的最大字符数，不包括结尾空字符。
         */
        [[nodiscard]] static auto MaximumSize() noexcept -> std::uint64_t
        {
            return static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max() - 1);
        }

        /**
         * @brief 检查并计算两个字符串长度之和。
         * @param Left 第一个长度。
         * @param Right 要追加的长度。
         * @return 两个长度之和。
         * @throws std::length_error 长度之和超出最大值时抛出。
         */
        [[nodiscard]] static auto CheckedLengthAdd(
            std::uint64_t Left, std::uint64_t Right) -> std::uint64_t
        {
            const auto Maximum = MaximumSize();
            if (Right > Maximum - Left)
            {
                throw std::length_error("String length exceeds max_size");
            }
            return Left + Right;
        }

    public:
        /** @brief 可修改字符的指针迭代器。 */
        using Iterator = char *;
        /** @brief 只读字符的指针迭代器。 */
        using ConstIterator = const char *;
        /** @brief 指向反向遍历基准位置的迭代器。 */
        using ReverseIterator = std::reverse_iterator<Iterator>;
        /** @brief 指向只读反向遍历基准位置的迭代器。 */
        using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
        /** @brief 表示未找到位置的哨兵值。 */
        constexpr static const uint64_t Npos = -1;
        /**
         * @brief 取得首元素迭代器。
         * @return 指向首字符的可写迭代器；空字符串返回尾后迭代器。
         */
        [[nodiscard]] Iterator Begin() noexcept
        {
            return MemberData;
        }

        /**
         * @brief 取得首元素迭代器。
         * @return 指向首字符的只读迭代器；空字符串返回尾后迭代器。
         */
        [[nodiscard]] ConstIterator Begin() const noexcept
        {
            return MemberData;
        }

        /**
         * @brief 取得尾后迭代器。
         * @return 指向末字符之后的迭代器。
         */
        [[nodiscard]] Iterator End() noexcept
        {
            return MemberData == nullptr ? nullptr : MemberData + MemberSize;
        }

        /**
         * @brief 取得尾后迭代器。
         * @return 指向末字符之后的只读迭代器。
         */
        [[nodiscard]] ConstIterator End() const noexcept
        {
            return MemberData == nullptr ? nullptr : MemberData + MemberSize;
        }

        /**
         * @brief 取得只读首元素迭代器。
         * @return 指向首字符的只读迭代器；空字符串返回尾后迭代器。
         */
        [[nodiscard]] ConstIterator Cbegin() const noexcept
        {
            return static_cast<ConstIterator>(MemberData);
        }

        /**
         * @brief 取得只读尾后迭代器。
         * @return 指向末字符之后的只读迭代器。
         */
        [[nodiscard]] ConstIterator Cend() const noexcept
        {
            return MemberData == nullptr
                ? nullptr
                : static_cast<ConstIterator>(MemberData + MemberSize);
        }

        /**
         * @brief 取得反向首元素迭代器。
         * @return 指向末字符的反向迭代器；空字符串返回反向尾后迭代器。
         */
        [[nodiscard]] ReverseIterator Rbegin() noexcept
        {
            return ReverseIterator(End());
        }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 反向尾后迭代器。
         */
        [[nodiscard]] ReverseIterator Rend() noexcept
        {
            return ReverseIterator(Begin());
        }

        /**
         * @brief 取得反向首元素迭代器。
         * @return 指向末字符的只读反向迭代器；空字符串返回反向尾后迭代器。
         */
        [[nodiscard]] ConstReverseIterator Rbegin() const noexcept
        {
            return ConstReverseIterator(Cend());
        }

        /**
         * @brief 取得反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        [[nodiscard]] ConstReverseIterator Rend() const noexcept
        {
            return ConstReverseIterator(Cbegin());
        }

        /**
         * @brief 取得只读反向首元素迭代器。
         * @return 指向末字符的只读反向迭代器；空字符串返回反向尾后迭代器。
         */
        [[nodiscard]] ConstReverseIterator Crbegin() const noexcept
        {
            return ConstReverseIterator(Cend());
        }

        /**
         * @brief 取得只读反向尾后迭代器。
         * @return 只读反向尾后迭代器。
         */
        [[nodiscard]] ConstReverseIterator Crend() const noexcept
        {
            return ConstReverseIterator(Cbegin());
        }

        /**
         * @brief 判断容器是否为空。
         * @return 为空时返回 true。
         */
        [[nodiscard]] bool Empty() const noexcept
        {
            return MemberSize == 0;
        }

        /**
         * @brief 取得元素数量。
         * @return 当前元素数量。
         */
        [[nodiscard]] uint64_t Size() const noexcept
        {
            return MemberSize;
        }

        /**
         * @brief 取得当前容量。
         * @return 当前容量。
         */
        [[nodiscard]] uint64_t Capacity() const noexcept
        {
            return MemberCapacity;
        }

        /**
         * @brief 取得以空字符结尾的字符指针。
         * @return 只读 C 字符串指针，有效期至下次修改。
         */
        [[nodiscard]] const char *CStr() const noexcept
        {
            return MemberData == nullptr ? "" : static_cast<const char *>(MemberData);
        } // 返回C风格字符串

        /**
         * @brief 访问末元素。
         * @return 对应位置的字符。
         */
        [[nodiscard]] char Back() const noexcept
        {
            return MemberSize > 0 && MemberData != nullptr
                ? MemberData[MemberSize - 1] : '\0';
        }

        /**
         * @brief 访问首元素。
         * @return 对应位置的字符。
         */
        [[nodiscard]] char Front() const noexcept
        {
            return MemberSize > 0 && MemberData != nullptr ? MemberData[0] : '\0';
        } // 返回头字符

        /**
         * @brief 构造 String 对象。
         * @param StrData 输入字符串。
         */
        String(const char *StrData = "")
            : MemberSize(StrData == nullptr ? 0 : std::strlen(StrData))
            , MemberCapacity(MemberSize)
        {
            if (MemberSize > MaximumSize())
            {
                throw std::length_error("String length exceeds max_size");
            }
            if (StrData != nullptr)
            {
                MemberData = new char[MemberCapacity + 1];
                std::memcpy(MemberData, StrData, MemberSize);
                MemberData[MemberSize] = '\0';
            }
            else
            {
                MemberData = new char[1];
                MemberData[0] = '\0';
            }
        }
        /**
         * @brief 构造 String 对象。
         * @param StrData 输入字符串。
         */
        String(char *&&StrData)
            : MemberSize(StrData == nullptr ? 0 : std::strlen(StrData))
            , MemberCapacity(MemberSize)
        {
            if (StrData != nullptr)
            {
                if (MemberSize > MaximumSize())
                {
                    throw std::length_error(
                        "String length exceeds max_size");
                }
                MemberData = StrData;
                StrData = nullptr;
            }
            else
            {
                MemberData = new char[1];
                MemberData[0] = '\0';
            }
        }
        /**
         * @brief 构造 String 对象。
         * @param StrData 输入字符串。
         */
        String(const String &StrData)
            : MemberSize(StrData.MemberSize)
            , MemberCapacity(StrData.MemberCapacity)
        {
            MemberData = new char[MemberCapacity + 1];
            if (StrData.MemberData != nullptr)
            {
                std::memcpy(MemberData, StrData.MemberData, StrData.MemberSize + 1);
            }
            else
            {
                MemberData[0] = '\0';
            }
        }
        /**
         * @brief 构造 String 对象。
         * @param StrData 输入字符串。
         */
        String(String &&StrData) noexcept
            : MemberData(StrData.MemberData)
            , MemberSize(StrData.MemberSize)
            , MemberCapacity(StrData.MemberCapacity)
        {
            StrData.MemberData = nullptr;
            StrData.MemberSize = 0;
            StrData.MemberCapacity = 0;
        }
        /**
         * @brief 构造 String 对象。
         * @param StrData 输入字符串。
         */
        String(const std::initializer_list<char> StrData)
        {
            MemberSize = StrData.size();
            MemberCapacity = MemberSize;
            MemberData = new char[MemberCapacity + 1];
            StandardCon::Algorithm::Copy(StrData.begin(), StrData.end(), MemberData);
            MemberData[MemberSize] = '\0';
        }
        /**
         * @brief 销毁 String 对象。
         */
        ~String() noexcept
        {
            delete[] MemberData;
            MemberData = nullptr;
            MemberCapacity = MemberSize = 0;
        }
        /**
         * @brief 将字符转换为大写。
         * @return 当前对象或元素的引用。
         */
        String &Uppercase() noexcept
        {
            // 字符串转大写
            if (MemberData == nullptr)
            {
                return *this;
            }
            for (String::Iterator StartPosition = MemberData; StartPosition != MemberData + MemberSize; StartPosition++)
            {
                if (*StartPosition >= 'a' && *StartPosition <= 'z')
                {
                    *StartPosition -= 32;
                }
            }
            return *this;
        }
        /**
         * @brief 将字符转换为小写。
         * @return 当前对象或元素的引用。
         */
        String &Lowercase() noexcept
        {
            // 字符串转小写
            if (MemberData == nullptr)
            {
                return *this;
            }
            for (String::Iterator StartPosition = MemberData; StartPosition != MemberData + MemberSize; StartPosition++)
            {
                if (*StartPosition >= 'A' && *StartPosition <= 'Z')
                {
                    *StartPosition += 32;
                }
            }
            return *this;
        }
        // uint64_t StrSubstringKmp(const char*& SubString)
        // {
        //     //查找子串
        // }
        /**
         * @brief 在开头添加字符串。
         * @param SubString 输入字符串。
         * @return 当前对象或元素的引用。
         */
        String &Prepend(const char *SubString)
        {
            if (SubString == nullptr || *SubString == '\0')
            {
                return *this;
            }
            const std::string Prefix(SubString);
            const auto Len = static_cast<std::uint64_t>(Prefix.size());
            const auto NewSize = CheckedLengthAdd(MemberSize, Len);
            AllocateResources(NewSize);
            std::memmove(MemberData + Len, MemberData, MemberSize + 1);
            std::memcpy(MemberData, Prefix.data(), Len);
            MemberSize = NewSize;
            MemberData[MemberSize] = '\0';
            return *this;
        }
        /**
         * @brief 在指定位置插入子字符串。
         * @param SubString 输入字符串。
         * @param StartPosition 起始位置。
         * @return 当前对象或元素的引用。
         */
        String &InsertSubString(const char *SubString, const uint64_t &StartPosition)
        {
            try
            {
                // 中间位置插入子串
                if (StartPosition > MemberSize)
                {
                    throw CustomException::Fault("传入参数位置越界", "insert_sub_string", __LINE__);
                }
                if (SubString == nullptr)
                {
                    return *this;
                }
                const std::string Insertion(SubString);
                const uint64_t Len = Insertion.size();
                const auto NewSize = CheckedLengthAdd(MemberSize, Len);
                AllocateResources(NewSize);
                std::memmove(MemberData + StartPosition + Len,
                    MemberData + StartPosition,
                    MemberSize - StartPosition + 1);
                std::memcpy(MemberData + StartPosition,
                    Insertion.data(), Len);
                MemberSize = NewSize;
                MemberData[MemberSize] = '\0';
                return *this;
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
        }
        /**
         * @brief 提取子字符串。
         * @param StartPosition 起始位置。
         * @return 得到的新字符串。
         */
        [[nodiscard]] String SubString(const uint64_t &StartPosition) const
        {
            // 提取字串到'\0'
            try
            {
                if (StartPosition > MemberSize)
                {
                    throw CustomException::Fault("传入参数位置越界", "sub_string", __LINE__);
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
            String Result;
            uint64_t SubLen = MemberSize - StartPosition;
            Result.AllocateResources(SubLen);
            if (SubLen > 0)
            {
                std::memcpy(Result.MemberData, MemberData + StartPosition, SubLen);
            }
            Result.MemberSize = SubLen;
            Result.MemberData[Result.MemberSize] = '\0';
            return Result;
        }
        /**
         * @brief 从指定位置提取子字符串。
         * @param StartPosition 起始位置。
         * @return 得到的新字符串。
         */
        [[nodiscard]] String SubStringFrom(const uint64_t &StartPosition) const
        {
            // 提取字串到末尾
            try
            {
                if (StartPosition > MemberSize)
                {
                    throw CustomException::Fault("传入参数位置越界", "sub_string_from", __LINE__);
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
            String Result;
            uint64_t SubLen = MemberSize - StartPosition;
            Result.AllocateResources(SubLen);
            if (SubLen > 0)
            {
                std::memcpy(Result.MemberData, MemberData + StartPosition, SubLen);
            }
            Result.MemberSize = SubLen;
            Result.MemberData[Result.MemberSize] = '\0';
            return Result;
        }
        /**
         * @brief 提取子字符串。
         * @param StartPosition 起始位置。
         * @param TerminatePosition 结束位置，不包含该位置。
         * @return 得到的新字符串。
         */
        [[nodiscard]] String SubString(const uint64_t &StartPosition, const uint64_t &TerminatePosition) const
        {
            // 提取字串到指定位置
            try
            {
                if (StartPosition > MemberSize || TerminatePosition > MemberSize || StartPosition > TerminatePosition)
                {
                    throw CustomException::Fault("传入参数位置越界", "sub_string", __LINE__);
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
            String Result;
            uint64_t SubLen = TerminatePosition - StartPosition;
            Result.AllocateResources(SubLen);
            // strncpy更安全
            if (SubLen > 0)
            {
                std::memcpy(Result.MemberData, MemberData + StartPosition, SubLen);
            }
            Result.MemberSize = SubLen;
            Result.MemberData[Result.MemberSize] = '\0';
            return Result;
        }
        /**
         * @brief 确保字符串具有指定容量。
         * @param NewInaugurateCapacity 请求的字符存储容量，不含结尾空字符。
         */
        void AllocateResources(const uint64_t &NewInaugurateCapacity)
        {
            if (NewInaugurateCapacity > MaximumSize())
            {
                throw std::length_error("String capacity exceeds max_size");
            }
            if (NewInaugurateCapacity <= MemberCapacity && MemberData != nullptr)
            {
                return;
            }

            auto TemporaryStrArray =
                std::make_unique<char[]>(NewInaugurateCapacity + 1);
            if (MemberData == nullptr)
            {
                TemporaryStrArray[0] = '\0';
            }
            else
            {
                std::memcpy(TemporaryStrArray.get(), MemberData,
                    MemberSize + 1);
            }

            TemporaryStrArray[MemberSize] = '\0';
            delete[] MemberData;
            MemberData = TemporaryStrArray.release();
            MemberCapacity = NewInaugurateCapacity;
        }
        /**
         * @brief 在末尾追加元素。
         * @param TemporaryStrData 输入字符串。
         * @return 当前对象的引用。
         */
        String &PushBack(const char &TemporaryStrData)
        {
            const auto NewSize = CheckedLengthAdd(MemberSize, 1);
            if (NewSize > MemberCapacity)
            {
                const auto DoubledCapacity = MemberCapacity == 0
                    ? std::uint64_t{2}
                    : (MemberCapacity > MaximumSize() / 2
                        ? MaximumSize() : MemberCapacity * 2);
                AllocateResources(std::max(NewSize, DoubledCapacity));
            }
            MemberData[MemberSize] = TemporaryStrData;
            MemberSize = NewSize;
            MemberData[MemberSize] = '\0';
            return *this;
        }
        /**
         * @brief 在末尾追加元素。
         * @param TemporaryStringData 要追加的字符串。
         * @return 当前对象的引用。
         */
        String &PushBack(const String &TemporaryStringData)
        {
            if (TemporaryStringData.MemberSize == 0)
            {
                return *this;
            }
            const auto OldSize = MemberSize;
            const auto NewSize = CheckedLengthAdd(
                OldSize, TemporaryStringData.MemberSize);
            AllocateResources(NewSize);
            if (this == &TemporaryStringData)
            {
                std::memmove(MemberData + OldSize, MemberData, OldSize);
            }
            else
            {
                std::memcpy(MemberData + OldSize,
                    TemporaryStringData.MemberData,
                    TemporaryStringData.MemberSize);
            }
            MemberSize = NewSize;
            MemberData[MemberSize] = '\0';
            return *this;
        }
        /**
         * @brief 在末尾追加元素。
         * @param TemporaryStrPtrData 输入字符串。
         * @return 当前对象的引用。
         */
        String &PushBack(const char *TemporaryStrPtrData)
        {
            if (TemporaryStrPtrData == nullptr)
            {
                return *this;
            }
            const auto Len = static_cast<std::uint64_t>(
                std::strlen(TemporaryStrPtrData));
            if (Len == 0)
            {
                return *this;
            }
            const std::string Staged(TemporaryStrPtrData);
            const auto NewSize = CheckedLengthAdd(
                MemberSize, static_cast<std::uint64_t>(Staged.size()));
            AllocateResources(NewSize);
            std::memcpy(MemberData + MemberSize, Staged.data(), Staged.size());
            MemberSize = NewSize;
            MemberData[MemberSize] = '\0';
            return *this;
        }
        /**
         * @brief 调整字符串长度并填充新字符。
         * @param InaugurateSize 调整后的字符数。
         * @param DefaultData 扩展时用于填充新增位置的字符。
         * @return 当前对象的引用。
         */
        String &Resize(const uint64_t &InaugurateSize, const char &DefaultData = '\0')
        {
            if (InaugurateSize > MaximumSize())
            {
                throw std::length_error("String size exceeds max_size");
            }
            const auto OldSize = MemberSize;
            if (InaugurateSize > OldSize)
            {
                AllocateResources(InaugurateSize);
                std::fill(MemberData + OldSize,
                    MemberData + InaugurateSize, DefaultData);
            }
            if (MemberData == nullptr)
            {
                AllocateResources(InaugurateSize);
            }
            MemberSize = InaugurateSize;
            MemberData[MemberSize] = '\0';
            return *this;
        }
        /**
         * @brief 预留存储空间。
         * @param NewContainerCapacity 要预留的字符存储容量，不含结尾空字符。
         * @return 当前存储区的起始指针。
         */
        Iterator Reserve(const uint64_t &NewContainerCapacity)
        {
            try
            {
                AllocateResources(NewContainerCapacity);
            }
            catch (const std::bad_alloc &NewCharptrAbnormal)
            {
                std::cerr << NewCharptrAbnormal.what() << std::endl;
                throw;
            }
            return MemberData;
            // 返回首地址迭代器
        }
        /**
         * @brief 交换两个对象的内容。
         * @param StrData 输入字符串。
         * @return 当前对象的引用。
         */
        String &Swap(String &StrData) noexcept
        {
            StandardCon::Algorithm::Swap(MemberData, StrData.MemberData);
            StandardCon::Algorithm::Swap(MemberSize, StrData.MemberSize);
            StandardCon::Algorithm::Swap(MemberCapacity, StrData.MemberCapacity);
            return *this;
        }
        /**
         * @brief 创建反转后的字符串。
         * @return 得到的新字符串。
         */
        [[nodiscard]] String Reverse() const
        {
            try
            {
                if (MemberSize == 0)
                {
                    throw CustomException::Fault("当前string为空", "reserve", __LINE__);
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
            String ReversedString;
            for (auto Reverse = Crbegin(); Reverse != Crend(); ++Reverse)
            {
                ReversedString.PushBack(*Reverse);
            }
            return ReversedString;
        }
        /**
         * @brief 创建反转后的子字符串。
         * @param StartPosition 起始位置。
         * @param TerminatePosition 结束位置，不包含该位置。
         * @return 得到的新字符串。
         */
        [[nodiscard]] String ReverseSubString(const uint64_t &StartPosition, const uint64_t &TerminatePosition) const
        {
            try
            {
                if (StartPosition > MemberSize || TerminatePosition > MemberSize || StartPosition > TerminatePosition || MemberSize == 0)
                {
                    throw CustomException::Fault("string回滚位置异常", "reverse_sub_string", __LINE__);
                }
            }
            catch (const CustomException::Fault &Process)
            {
                std::cerr << Process.what() << " " << Process.FunctionNameGet() << " " << Process.LineNumberGet() << std::endl;
                throw;
            }
            String ReversedResult;
            auto Reverse = ConstReverseIterator(MemberData + TerminatePosition);
            const auto Finish = ConstReverseIterator(MemberData + StartPosition);
            for (; Reverse != Finish; ++Reverse)
            {
                ReversedResult.PushBack(*Reverse);
            }
            return ReversedResult;
        }
        /**
         * @brief 输出字符串。
         */
        void StringPrint() const noexcept
        {
            std::cout << MemberData << std::endl;
        }
        /**
         * @brief 逆序输出字符串。
         */
        void StringReversePrint() const noexcept
        {
            for (String::ConstReverseIterator StartPosition = Crbegin();
                 StartPosition != Crend(); ++StartPosition)
            {
                std::cout << *StartPosition;
            }
            std::cout << std::endl;
        }
        /**
         * @brief 向流写入对象。
         * @param StringOstream 输出流。
         * @param StrData 输入字符串。
         * @return 输入或输出流的引用。
         */
        friend std::ostream &operator<<(std::ostream &StringOstream, const String &StrData);
        /**
         * @brief 从流读取字符串。
         * @param StringIstream 输入流。
         * @param StrData 输入字符串。
         * @return 输入或输出流的引用。
         */
        friend std::istream &operator>>(std::istream &StringIstream, String &StrData);
        /**
         * @brief 赋值当前对象。
         * @param StrData 输入字符串。
         * @return 当前对象的引用。
         */
        String &operator=(const String &StrData)
        {
            try
            {
                if (this != &StrData) // 防止无意义拷贝
                {
                    uint64_t Capacity = StrData.MemberCapacity;
                    char *NewData = new char[Capacity + 1];
                    if (StrData.MemberData != nullptr)
                    {
                        std::memcpy(NewData, StrData.MemberData,
                            StrData.MemberSize + 1);
                    }
                    else
                    {
                        NewData[0] = '\0';
                    }
                    delete[] MemberData;
                    MemberData = NewData;
                    MemberCapacity = Capacity;
                    MemberSize = StrData.MemberSize;
                }
            }
            catch (const std::bad_alloc &Process)
            {
                std::cerr << Process.what() << std::endl;
                throw;
            }
            return *this;
        }
        /**
         * @brief 赋值当前对象。
         * @param StrData 输入字符串。
         * @return 当前对象的引用。
         */
        String &operator=(const char *StrData)
        {
            try
            {
                if (StrData == nullptr)
                {
                    StrData = "";
                }
                const std::string Copy(StrData);
                uint64_t Capacity = Copy.size();
                if (Capacity > MaximumSize())
                {
                    throw std::length_error(
                        "String length exceeds max_size");
                }
                char *NewData = new char[Capacity + 1];
                std::memcpy(NewData, Copy.data(), Capacity);
                NewData[Capacity] = '\0';
                delete[] MemberData;
                MemberData = NewData;
                MemberCapacity = Capacity;
                MemberSize = Capacity;
                MemberData[MemberSize] = '\0';
            }
            catch (const std::bad_alloc &Process)
            {
                std::cerr << Process.what() << std::endl;
                throw;
            }
            return *this;
        }
        /**
         * @brief 赋值当前对象。
         * @param StrData 输入字符串。
         * @return 当前对象的引用。
         */
        String &operator=(String &&StrData) noexcept
        {
            if (this != &StrData)
            {
                delete[] MemberData;
                MemberSize = StrData.MemberSize;
                MemberCapacity = StrData.MemberCapacity;
                MemberData = StrData.MemberData;
                StrData.MemberData = nullptr;
                StrData.MemberSize = 0;
                StrData.MemberCapacity = 0;
            }
            return *this;
        }
        /**
         * @brief 将另一字符串追加到末尾。
         * @param StrData 要追加的字符串。
         * @return 当前字符串的引用。
         */
        String &operator+=(const String &StrData)
        {
            return PushBack(StrData);
        }
        /**
         * @brief 比较两个对象是否相等。
         * @param StrData 输入字符串。
         * @return 条件成立时返回 true，否则返回 false。
         */
        bool operator==(const String &StrData) const noexcept
        {
            if (MemberSize != StrData.MemberSize)
            {
                return false;
            }
            for (uint64_t CompareTraversal = 0; CompareTraversal < MemberSize; CompareTraversal++)
            {
                if (MemberData[CompareTraversal] != StrData.MemberData[CompareTraversal])
                {
                    return false;
                }
            }
            return true;
        }
        /**
         * @brief 比较字典序是否小于另一对象。
         * @param StrData 输入字符串。
         * @return 条件成立时返回 true，否则返回 false。
         */
        bool operator<(const String &StrData) const noexcept
        {
            uint64_t MinLen = MemberSize < StrData.MemberSize ? MemberSize : StrData.MemberSize;
            for (uint64_t CompareTraversal = 0; CompareTraversal < MinLen; CompareTraversal++)
            {
                if (MemberData[CompareTraversal] != StrData.MemberData[CompareTraversal])
                {
                    return MemberData[CompareTraversal] < StrData.MemberData[CompareTraversal];
                }
            }
            return MemberSize < StrData.MemberSize;
        }
        /**
         * @brief 比较字典序是否大于另一对象。
         * @param StrData 输入字符串。
         * @return 条件成立时返回 true，否则返回 false。
         */
        bool operator>(const String &StrData) const noexcept
        {
            uint64_t MinLen = MemberSize < StrData.MemberSize ? MemberSize : StrData.MemberSize;
            for (uint64_t CompareTraversal = 0; CompareTraversal < MinLen; CompareTraversal++)
            {
                if (MemberData[CompareTraversal] != StrData.MemberData[CompareTraversal])
                {
                    return MemberData[CompareTraversal] > StrData.MemberData[CompareTraversal];
                }
            }
            return MemberSize > StrData.MemberSize;
        }
        /**
         * @brief 按索引访问元素。
         * @param AccessLocation 元素索引。
         * @return 索引处元素的引用。
         * @throws CustomException::Fault 索引超出字符串范围时抛出。
         */
        char &operator[](const uint64_t &AccessLocation)
        {
            try
            {
                if (AccessLocation < MemberSize)
                {
                    return MemberData[AccessLocation]; // 返回第ErgodicValue个元素的引用
                }
                else
                {
                    throw CustomException::Fault("越界访问", "string::operator[]", __LINE__);
                }
            }
            catch (const CustomException::Fault &AccessException)
            {
                throw;
            }
            // 就像MemberData在外面就能访问它以及它的成员，所以这种就可以理解成出了函数作用域还在，进函数之前也能访问的就是引用
        }
        /**
         * @brief 按索引访问元素。
         * @param AccessLocation 元素索引。
         * @return 索引处元素的引用。
         * @throws CustomException::Fault 索引超出字符串范围时抛出。
         */
        const char &operator[](const uint64_t &AccessLocation) const
        {
            try
            {
                if (AccessLocation < MemberSize)
                {
                    return MemberData[AccessLocation]; // 返回第ErgodicValue个元素的引用
                }
                else
                {
                    throw CustomException::Fault("越界访问", "string::operator[]const", __LINE__);
                }
            }
            catch (const CustomException::Fault &AccessException)
            {
                throw;
            }
        }
        /**
         * @brief 创建拼接后的新对象。
         * @param StringArray 要追加到当前字符串副本的字符串。
         * @return 拼接后的新对象。
         */
        [[nodiscard]] String operator+(const String &StringArray) const
        {
            String ReturnStringObject(*this);
            ReturnStringObject += StringArray;
            return ReturnStringObject;
        }
    };
    std::istream &operator>>(std::istream &StringIstream, String &StrData)
    {
        while (true)
        {
            const auto Character = StringIstream.get();
            if (Character == std::char_traits<char>::eof()
                || Character == '\n')
            {
                break;
            }
            else
            {
                StrData.PushBack(static_cast<char>(Character));
            }
        }
        return StringIstream;
    }
    inline std::ostream &operator<<(std::ostream &StringOstream, const String &StrData)
    {
        for (auto It = StrData.Begin(); It != StrData.End(); ++It)
        {
            StringOstream << *It;
        }
        return StringOstream;
    }
}
namespace StandardCon
{
    using StringContainer::String;
}
