/**
 * @file Code.hpp
 * @brief 统一错误码定义
 * @details 定义跨模块错误码枚举，遵循 Prism 三文件 Fault 体系。
 *          数值与原 Wan::Web::Fault 保持兼容。
 * @author Hatedatastructures
 * @date 2026-05-22
 */
#pragma once

#include <string_view>

namespace Wan::Fault
{
    /**
     * @enum Code
     * @brief 库内统一错误码。
     * @details 零值表示成功，其余值按错误类别分配。
     */
    enum class Code : int
    {
        // ── 通用 (0-10) ──
        Success = 0, ///< 操作成功。
        GenericError = 1, ///< 未细分的通用错误。
        ParseError = 2, ///< 数据解析失败。
        Eof = 3, ///< 输入已到末尾。
        WouldBlock = 4, ///< 当前操作会阻塞。
        ProtocolError = 5, ///< 协议处理失败。
        BadMessage = 6, ///< 消息格式无效。
        InvalidArgument = 7, ///< 参数无效。
        NotSupported = 8, ///< 当前操作或功能不受支持。
        MessageTooLarge = 9, ///< 消息超过允许大小。
        IoError = 10, ///< 输入输出操作失败。

        // ── 网络 (11-25) ──
        Timeout = 11, ///< 操作超时。
        Canceled = 12, ///< 操作已取消。
        TlsHandshakeFailed = 13, ///< TLS 握手失败。
        TlsShutdownFailed = 14, ///< TLS 关闭失败。
        AuthFailed = 15, ///< 身份验证失败。
        DnsFailed = 16, ///< 域名解析失败。
        UpstreamUnreachable = 17, ///< 无法访问上游服务。
        ConnectionRefused = 18, ///< 连接被对端拒绝。
        ConnectionReset = 19, ///< 连接被对端重置。
        ConnectionAborted = 20, ///< 连接意外中止。
        NetworkUnreachable = 21, ///< 网络不可达。
        HostUnreachable = 22, ///< 主机不可达。
        BadGateway = 23, ///< 上游响应无效。

        // ── 协议 (26-32) ──
        PathNotFound = 26, ///< 请求路径不存在。
        MethodNotAllowed = 27, ///< 请求方法不允许。
        UnsupportedCommand = 28, ///< 命令不受支持。
        UnsupportedAddress = 29, ///< 地址类型不受支持。
        Blocked = 30, ///< 请求被策略阻止。
        ConfigError = 31, ///< 配置无效。

        // ── 安全 (32-37) ──
        PoolFull = 32, ///< 资源池已满。
        SslHandshakeFailed = 33, ///< SSL 握手失败。
        NotImplemented = 34, ///< 功能尚未实现。
        CryptoError = 35, ///< 密码学操作失败。
        SslCertLoadFailed = 36, ///< 证书加载失败。
        SslKeyLoadFailed = 37, ///< 私钥加载失败。

        Count = 38 ///< 错误码数量标记，不代表具体错误。
    };

    /**
     * @brief 获取错误码对应的文本名称。
     * @param Value 要查询的错误码。
     * @return 稳定的英文错误名称；未知值返回 `unknown`。
     */
    [[nodiscard]] constexpr auto Describe(Code Value) noexcept -> std::string_view
    {
        switch (Value)
        {
        case Code::Success:
            return "success";
        case Code::GenericError:
            return "generic_error";
        case Code::ParseError:
            return "parse_error";
        case Code::Eof:
            return "eof";
        case Code::WouldBlock:
            return "would_block";
        case Code::ProtocolError:
            return "protocol_error";
        case Code::BadMessage:
            return "bad_message";
        case Code::InvalidArgument:
            return "invalid_argument";
        case Code::NotSupported:
            return "not_supported";
        case Code::MessageTooLarge:
            return "message_too_large";
        case Code::IoError:
            return "io_error";
        case Code::Timeout:
            return "timeout";
        case Code::Canceled:
            return "canceled";
        case Code::TlsHandshakeFailed:
            return "tls_handshake_failed";
        case Code::TlsShutdownFailed:
            return "tls_shutdown_failed";
        case Code::AuthFailed:
            return "auth_failed";
        case Code::DnsFailed:
            return "dns_failed";
        case Code::UpstreamUnreachable:
            return "upstream_unreachable";
        case Code::ConnectionRefused:
            return "connection_refused";
        case Code::ConnectionReset:
            return "connection_reset";
        case Code::ConnectionAborted:
            return "connection_aborted";
        case Code::NetworkUnreachable:
            return "network_unreachable";
        case Code::HostUnreachable:
            return "host_unreachable";
        case Code::BadGateway:
            return "bad_gateway";
        case Code::PathNotFound:
            return "path_not_found";
        case Code::MethodNotAllowed:
            return "method_not_allowed";
        case Code::UnsupportedCommand:
            return "unsupported_command";
        case Code::UnsupportedAddress:
            return "unsupported_address";
        case Code::Blocked:
            return "blocked";
        case Code::ConfigError:
            return "config_error";
        case Code::PoolFull:
            return "pool_full";
        case Code::SslHandshakeFailed:
            return "ssl_handshake_failed";
        case Code::NotImplemented:
            return "not_implemented";
        case Code::CryptoError:
            return "crypto_error";
        case Code::SslCertLoadFailed:
            return "ssl_cert_load_failed";
        case Code::SslKeyLoadFailed:
            return "ssl_key_load_failed";
        default:
            return "unknown";
        }
    }

    /**
     * @brief 检查错误码是否表示成功。
     * @param C 要检查的错误码。
     * @return 仅 `Code::Success` 返回 `true`。
     */
    [[nodiscard]] constexpr auto Succeeded(Code C) noexcept -> bool
    {
        return C == Code::Success;
    }

    /**
     * @brief 检查错误码是否表示失败。
     * @param C 要检查的错误码。
     * @return 非 `Code::Success` 时返回 `true`。
     */
    [[nodiscard]] constexpr auto Failed(Code C) noexcept -> bool
    {
        return C != Code::Success;
    }
}
