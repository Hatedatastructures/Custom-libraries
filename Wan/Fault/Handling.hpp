/**
 * @file Handling.hpp
 * @brief 错误处理工具
 * @details 提供跨错误码类型的 Succeeded/Failed 模板函数和 Boost/Std 错误映射。
 * @author Hatedatastructures
 * @date 2026-05-22
 */
#pragma once

#include "Code.hpp"

#include <system_error>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

namespace Wan::Fault
{
    // ============================================================
    // 模板 Succeeded / Failed — 编译期分派
    // ============================================================

    /**
     * @brief 检查错误码是否表示成功。
     * @tparam ErrorCode `Code`、`std::error_code` 或 Boost 错误码类型。
     * @param Ec 要检查的错误码。
     * @return 错误码表示成功时为 `true`。
     */
    template <typename ErrorCode>
    [[nodiscard]] constexpr auto Succeeded(const ErrorCode& Ec) noexcept -> bool
    {
        if constexpr (std::is_same_v<ErrorCode, Code>)
        {
            return Ec == Code::Success;
        }
        else if constexpr (std::is_same_v<ErrorCode, std::error_code>)
        {
            return !Ec;
        }
        else if constexpr (std::is_same_v<ErrorCode, boost::system::error_code>)
        {
            return !Ec;
        }
        else
        {
            static_assert(std::is_same_v<ErrorCode, Code>
                    || std::is_same_v<ErrorCode, std::error_code>
                    || std::is_same_v<ErrorCode, boost::system::error_code>,
                "wan::fault::succeeded: unsupported error code type");
            return false;
        }
    }

    /**
     * @brief 检查错误码是否表示失败。
     * @tparam ErrorCode `Code`、`std::error_code` 或 Boost 错误码类型。
     * @param Ec 要检查的错误码。
     * @return 错误码表示失败时为 `true`。
     */
    template <typename ErrorCode>
    [[nodiscard]] constexpr auto Failed(const ErrorCode& Ec) noexcept -> bool
    {
        return !Succeeded(Ec);
    }

    // ============================================================
    // ToCode — Boost/Std 错误映射
    // ============================================================

    /**
     * @brief 将 Boost 错误码映射为 `Code`。
     * @param Ec 要映射的错误码。
     * @return 映射后的错误码；未识别的外部错误映射为 `IoError`，非法本地值映射为 `GenericError`。
     */
    [[nodiscard]] inline auto ToCode(const boost::system::error_code& Ec) noexcept -> Code
    {
        if (!Ec)
        {
            return Code::Success;
        }

        // 往返映射：已经是 Wan::Fault Category
        if (Ec.category().name() == std::string_view("wan::fault"))
        {
            if (Ec.value() >= 0
                && Ec.value() < static_cast<int>(Code::Count))
            {
                return static_cast<Code>(Ec.value());
            }
            return Code::GenericError;
        }

        // Boost.Asio 错误映射
        if (Ec == boost::asio::error::eof)
        {
            return Code::Eof;
        }
        if (Ec == boost::asio::error::operation_aborted)
        {
            return Code::Canceled;
        }
        if (Ec == boost::asio::error::timed_out)
        {
            return Code::Timeout;
        }
        if (Ec == boost::asio::error::connection_refused)
        {
            return Code::ConnectionRefused;
        }
        if (Ec == boost::asio::error::connection_reset)
        {
            return Code::ConnectionReset;
        }
        if (Ec == boost::asio::error::connection_aborted)
        {
            return Code::ConnectionAborted;
        }
        if (Ec == boost::asio::error::host_unreachable)
        {
            return Code::HostUnreachable;
        }
        if (Ec == boost::asio::error::network_unreachable)
        {
            return Code::NetworkUnreachable;
        }

        return Code::IoError;
    }

    /**
     * @brief 将标准错误码映射为 `Code`。
     * @param Ec 要映射的错误码。
     * @return 映射后的错误码；未识别的外部错误映射为 `IoError`，非法本地值映射为 `GenericError`。
     */
    [[nodiscard]] inline auto ToCode(const std::error_code& Ec) noexcept -> Code
    {
        if (!Ec)
        {
            return Code::Success;
        }

        // 往返映射
        if (Ec.category() == Category())
        {
            if (Ec.value() >= 0
                && Ec.value() < static_cast<int>(Code::Count))
            {
                return static_cast<Code>(Ec.value());
            }
            return Code::GenericError;
        }

        // std::errc 映射
        static const auto ConnectionRefusedEc = std::make_error_code(std::errc::connection_refused);
        static const auto ConnectionResetEc = std::make_error_code(std::errc::connection_reset);
        static const auto ConnectionAbortedEc = std::make_error_code(std::errc::connection_aborted);
        static const auto TimedOutEc = std::make_error_code(std::errc::timed_out);
        static const auto HostUnreachableEc = std::make_error_code(std::errc::host_unreachable);
        static const auto NetworkUnreachableEc = std::make_error_code(std::errc::network_unreachable);
        static const auto OperationCanceledEc = std::make_error_code(std::errc::operation_canceled);

        if (Ec == ConnectionRefusedEc)
        {
            return Code::ConnectionRefused;
        }
        if (Ec == ConnectionResetEc)
        {
            return Code::ConnectionReset;
        }
        if (Ec == ConnectionAbortedEc)
        {
            return Code::ConnectionAborted;
        }
        if (Ec == TimedOutEc)
        {
            return Code::Timeout;
        }
        if (Ec == HostUnreachableEc)
        {
            return Code::HostUnreachable;
        }
        if (Ec == NetworkUnreachableEc)
        {
            return Code::NetworkUnreachable;
        }
        if (Ec == OperationCanceledEc)
        {
            return Code::Canceled;
        }

        return Code::IoError;
    }
}
