/**
 * @file fault.hpp
 * @brief Web 模块错误码定义
 * @details 定义 Web 模块运行时的错误码枚举，遵循热路径无异常原则。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <string_view>

namespace wan::web
{
    /**
     * @enum fault
     * @brief Web 模块错误码
     * @details 零值表示成功，非零值表示各类错误。
     */
    enum class fault : int
    {
        /** @brief 操作成功 */
        success = 0,
        /** @brief 通用错误 */
        generic_error = 1,
        /** @brief 解析错误 */
        parse_error = 2,
        /** @brief 到达文件末尾 */
        eof = 3,
        /** @brief 操作超时 */
        timeout = 11,
        /** @brief 操作被取消 */
        canceled = 12,
        /** @brief TLS 握手失败 */
        tls_handshake_failed = 13,
        /** @brief TLS 关闭失败 */
        tls_shutdown_failed = 14,
        /** @brief DNS 解析失败 */
        dns_failed = 16,
        /** @brief 连接被拒绝 */
        connection_refused = 18,
        /** @brief 网关错误 */
        bad_gateway = 22,
        /** @brief 连接被重置 */
        connection_reset = 24,
        /** @brief HTTP 请求无效 */
        http_invalid_request = 26,
        /** @brief HTTP 响应无效 */
        http_invalid_response = 27,
        /** @brief 路径未找到 */
        path_not_found = 32,
        /** @brief 方法不允许 */
        method_not_allowed = 33,
        /** @brief WebSocket 握手失败 */
        ws_handshake_failed = 36,
        /** @brief WebSocket 协议错误 */
        ws_protocol_error = 37,
        /** @brief WebSocket 连接已关闭 */
        ws_connection_closed = 38,
        /** @brief I/O 错误 */
        io_error = 39,
        /** @brief 连接池已满 */
        pool_full = 40,
        /** @brief SSL 握手失败 */
        ssl_handshake_failed = 41,
        /** @brief 功能未实现 */
        not_implemented = 42,
        /** @brief 请求体过大 */
        payload_too_large = 43,
        /** @brief 速率限制 */
        rate_limit_exceeded = 44
    };

    /**
     * @brief 获取错误码描述
     * @param value 错误码枚举值
     * @return 错误描述字符串视图
     */
    [[nodiscard]] constexpr auto describe(fault value) noexcept -> std::string_view
    {
        switch (value)
        {
        case fault::success:
            return "success";
        case fault::generic_error:
            return "generic_error";
        case fault::parse_error:
            return "parse_error";
        case fault::eof:
            return "eof";
        case fault::timeout:
            return "timeout";
        case fault::canceled:
            return "canceled";
        case fault::tls_handshake_failed:
            return "tls_handshake_failed";
        case fault::tls_shutdown_failed:
            return "tls_shutdown_failed";
        case fault::dns_failed:
            return "dns_failed";
        case fault::connection_refused:
            return "connection_refused";
        case fault::bad_gateway:
            return "bad_gateway";
        case fault::connection_reset:
            return "connection_reset";
        case fault::http_invalid_request:
            return "http_invalid_request";
        case fault::http_invalid_response:
            return "http_invalid_response";
        case fault::path_not_found:
            return "path_not_found";
        case fault::method_not_allowed:
            return "method_not_allowed";
        case fault::ws_handshake_failed:
            return "ws_handshake_failed";
        case fault::ws_protocol_error:
            return "ws_protocol_error";
        case fault::ws_connection_closed:
            return "ws_connection_closed";
        case fault::io_error:
            return "io_error";
        case fault::pool_full:
            return "pool_full";
        case fault::ssl_handshake_failed:
            return "ssl_handshake_failed";
        case fault::not_implemented:
            return "not_implemented";
        case fault::payload_too_large:
            return "payload_too_large";
        case fault::rate_limit_exceeded:
            return "rate_limit_exceeded";
        default:
            return "unknown";
        }
    }

    /**
     * @brief 检查是否成功
     */
    [[nodiscard]] constexpr auto succeeded(fault f) noexcept -> bool
    {
        return f == fault::success;
    }

    /**
     * @brief 检查是否失败
     */
    [[nodiscard]] constexpr auto failed(fault f) noexcept -> bool
    {
        return !succeeded(f);
    }
}