/**
 * @file Compatible.hpp
 * @brief 错误码桥接层
 * @details 提供 Fault::Code 与 std::error_code / boost::system::error_code 的双向桥接。
 *          遵循 Prism 的 dual-Category 设计模式。
 * @author Hatedatastructures
 * @date 2026-05-22
 */
#pragma once

#include "Code.hpp"

#include <array>
#include <string>
#include <system_error>

#include <boost/system/error_code.hpp>
#include <boost/system/is_error_code_enum.hpp>

namespace Wan::Fault
{
    // ============================================================
    // 缓存消息 — 惰性初始化，一次性分配
    // ============================================================

    /**
     * @brief 获取错误码对应的缓存文本。
     * @param C 错误码。
     * @return 错误描述的只读引用；越界值返回 `unknown`。
     */
    [[nodiscard]] inline auto CachedMessage(Code C) noexcept -> const std::string&
    {
        static std::array<std::string, static_cast<std::size_t>(Code::Count)> messages = []
        {
            std::array<std::string, static_cast<std::size_t>(Code::Count)> arr{};
            for (std::size_t i = 0; i < arr.size(); ++i)
            {
                auto desc = Describe(static_cast<Code>(i));
                arr[i] = std::string(desc);
            }
            return arr;
        }();

        auto idx = static_cast<std::size_t>(C);
        if (idx < messages.size())
        {
            return messages[idx];
        }
        static const std::string unknown = "unknown";
        return unknown;
    }

    // ============================================================
    // std::error_category 桥接
    // ============================================================

    /**
     * @class FaultCategory
     * @brief 自定义 std::error_category
     */
    class FaultCategory : public std::error_category
    {
    public:
        /**
         * @brief 返回稳定的错误类别名称 `wan::fault`。
         * @return 指向静态错误类别名称的字符串。
         */
        [[nodiscard]] auto name() const noexcept -> const char* override
        {
            return "wan::fault";
        }

        /**
         * @brief 获取错误码对应的文本。
         * @param C 整数形式的错误码。
         * @return 错误描述。
         */
        [[nodiscard]] auto message(int C) const -> std::string override
        {
            return CachedMessage(static_cast<Code>(C));
        }
    };

    /**
     * @brief 获取标准错误类别单例。
     * @return 全局共享的错误类别。
     */
    [[nodiscard]] inline auto Category() noexcept -> const std::error_category&
    {
        static const FaultCategory instance;
        return instance;
    }

    /**
     * @brief 从 `Code` 构造标准错误码。
     * @param C 错误码。
     * @return 使用 `FaultCategory` 的标准错误码。
     */
    [[nodiscard]] inline auto make_error_code(Code C) noexcept -> std::error_code
    {
        return std::error_code(static_cast<int>(C), Category());
    }

    // ============================================================
    // std::is_error_code_enum 特化 — 启用隐式转换
    // ============================================================
} // namespace Wan::Fault

/** @brief 允许 `Code` 隐式转换为标准错误码。 */
template <>
struct std::is_error_code_enum<Wan::Fault::Code> : std::true_type
{
};

/** @brief 为 `Code` 提供标准哈希支持。 */
template <>
    struct std::hash<Wan::Fault::Code>
    {
        /**
         * @brief 计算错误码的哈希值。
         * @param C 错误码。
         * @return 对应的哈希值。
         */
        [[nodiscard]] auto operator()(Wan::Fault::Code C) const noexcept -> std::size_t
    {
        return std::hash<int>{}(static_cast<int>(C));
    }
};

// ============================================================
// boost::system 桥接
// ============================================================

namespace boost::system
{
    /**
     * @class FaultCategory
     * @brief 自定义 boost::system::error_category
     */
    class FaultCategory : public error_category
    {
    public:
        /**
         * @brief 返回稳定的错误类别名称 `wan::fault`。
         * @return 指向静态错误类别名称的字符串。
         */
        [[nodiscard]] auto name() const noexcept -> const char* override
        {
            return "wan::fault";
        }

        /**
         * @brief 获取错误码对应的文本。
         * @param C 整数形式的错误码。
         * @return 错误描述。
         */
        [[nodiscard]] auto message(int C) const -> std::string override
        {
            return Wan::Fault::CachedMessage(static_cast<Wan::Fault::Code>(C));
        }
    };

    /**
     * @brief 获取 Boost 错误类别单例。
     * @return 全局共享的错误类别。
     */
    [[nodiscard]] inline auto Category() noexcept -> const error_category&
    {
        static const FaultCategory instance;
        return instance;
    }

    /**
     * @brief 从 `Code` 构造 Boost 错误码。
     * @param C 错误码。
     * @return 使用 `FaultCategory` 的 Boost 错误码。
     */
    [[nodiscard]] inline auto make_error_code(Wan::Fault::Code C) noexcept -> error_code
    {
        return error_code(static_cast<int>(C), Category());
    }

    // boost::system::is_error_code_enum 特化
    template <>
    struct is_error_code_enum<Wan::Fault::Code> : std::true_type
    {
    };
} // namespace boost::system
