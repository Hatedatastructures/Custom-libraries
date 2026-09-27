/**
 * @file Exception.hpp
 * @brief 容器异常类型。
 */
#pragma once
#define CRT_SECURE_NO_WARNINGS
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
namespace CustomException
{
/**
     * @brief 携带消息、函数名和行号的异常。
     */
    class Fault final : public std::exception
    {
    private:
        /** @brief 异常消息文本。 */
        std::string MemberMessage;
        /** @brief 记录异常时的函数名。 */
        std::string FunctionName;
        /** @brief 记录异常时的源代码行号。 */
        uint64_t LineNumber;

    public:
        /**
         * @brief 构造 Fault。
         * @param MessageTarget 异常消息。
         * @param FunctionNameTarget 异常发生的函数名。
         * @param LineNumberTarget 异常发生的行号。
         */
        Fault(const char *MessageTarget, const char *FunctionNameTarget,
            const uint64_t &LineNumberTarget)
            : MemberMessage(MessageTarget == nullptr ? "" : MessageTarget)
            , FunctionName(FunctionNameTarget == nullptr ? "" : FunctionNameTarget)
            , LineNumber(LineNumberTarget)
        {
        }
        /**
         * @brief 取得异常消息。
         * @return 异常消息的只读 C 字符串。
         */
        [[nodiscard]] const char *what() const noexcept override
        {
            return MemberMessage.c_str();
        }
        /**
         * @brief 取得异常发生的函数名。
         * @return 函数名的只读 C 字符串。
         */
        [[nodiscard]] const char *FunctionNameGet() const noexcept
        {
            return FunctionName.c_str();
        }
        /**
         * @brief 取得异常记录的行号。
         * @return 记录的行号。
         */
        [[nodiscard]] uint64_t LineNumberGet() const noexcept
        {
            return LineNumber;
        }
        /**
         * @brief 销毁 Fault。
         */
        ~Fault() noexcept override = default;
    };
}
namespace StandardCon
{
    using namespace CustomException;
}
