/**
 * @file ssl.hpp
 * @brief SSL/TLS 协程封装
 * @details 提供协程驱动的 SSL/TLS 连接支持。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>
#include <memory>
#include <string>
#include <system_error>

#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace ssl = net::ssl;
    using tcp = net::ip::tcp;

    /**
     * @class ssl_connection
     * @brief SSL/TLS 连接
     * @details 封装 SSL socket，提供协程握手方法。
     */
    class ssl_connection : public std::enable_shared_from_this<ssl_connection>
    {
    public:
        using socket_type = ssl::stream<tcp::socket>;

        /**
         * @brief 构造函数
         * @param socket SSL socket
         */
        explicit ssl_connection(socket_type socket)
            : socket_(std::move(socket))
        {
        }

        /**
         * @brief 执行 SSL 握手
         * @param type 握手类型（client/server）
         * @param ec 错误码
         */
        auto handshake(ssl::stream_base::handshake_type type, std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            co_await socket_.async_handshake(type, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::tls_handshake_failed), std::generic_category());
            }
            else
            {
                ec.clear();
            }
        }

        /**
         * @brief 异步读取
         */
        auto async_read_some(net::mutable_buffer buffer, std::error_code& ec) -> net::awaitable<std::size_t>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            auto n = co_await socket_.async_read_some(buffer, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::io_error), std::generic_category());
                co_return 0;
            }

            ec.clear();
            co_return n;
        }

        /**
         * @brief 异步写入
         */
        auto async_write_some(net::const_buffer buffer, std::error_code& ec) -> net::awaitable<std::size_t>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            auto n = co_await socket_.async_write_some(buffer, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::io_error), std::generic_category());
                co_return 0;
            }

            ec.clear();
            co_return n;
        }

        /**
         * @brief 关闭 SSL 连接
         */
        auto shutdown(std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            co_await socket_.async_shutdown(net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::tls_shutdown_failed), std::generic_category());
            }
            else
            {
                ec.clear();
            }
        }

        /**
         * @brief 关闭底层 socket
         */
        void close()
        {
            boost::system::error_code ec;
            socket_.lowest_layer().close(ec);
        }

        /**
         * @brief 检查是否打开
         */
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return socket_.lowest_layer().is_open();
        }

        /**
         * @brief 获取底层 socket
         */
        [[nodiscard]] auto socket() noexcept -> socket_type&
        {
            return socket_;
        }

        /**
         * @brief 获取执行器
         */
        [[nodiscard]] auto executor() noexcept -> net::any_io_executor
        {
            return socket_.get_executor();
        }

    private:
        socket_type socket_;
    };

    /**
     * @brief SSL 连接指针
     */
    using ssl_connection_ptr = std::shared_ptr<ssl_connection>;

    /**
     * @brief 创建 SSL 连接
     */
    [[nodiscard]] inline auto make_ssl_connection(ssl::stream<tcp::socket> socket) -> ssl_connection_ptr
    {
        return std::make_shared<ssl_connection>(std::move(socket));
    }

    /**
     * @class ssl_context
     * @brief SSL 上下文管理器
     */
    class ssl_context
    {
    public:
        /**
         * @brief 构造客户端 SSL 上下文
         */
        ssl_context()
            : ctx_(ssl::context::tls_client)
        {
            ctx_.set_default_verify_paths();
            ctx_.set_verify_mode(ssl::verify_peer);
        }

        /**
         * @brief 构造指定类型的 SSL 上下文
         */
        explicit ssl_context(ssl::context::method method)
            : ctx_(method)
        {
        }

        /**
         * @brief 加载证书文件
         */
        auto load_certificate_file(const std::string& path) -> bool
        {
            boost::system::error_code ec;
            ctx_.use_certificate_file(path, ssl::context::pem, ec);
            return !ec;
        }

        /**
         * @brief 加载私钥文件
         */
        auto load_private_key_file(const std::string& path) -> bool
        {
            boost::system::error_code ec;
            ctx_.use_private_key_file(path, ssl::context::pem, ec);
            return !ec;
        }

        /**
         * @brief 设置验证模式
         */
        void set_verify_mode(ssl::verify_mode mode)
        {
            ctx_.set_verify_mode(mode);
        }

        /**
         * @brief 获取底层 SSL 上下文
         */
        [[nodiscard]] auto context() noexcept -> ssl::context&
        {
            return ctx_;
        }

    private:
        ssl::context ctx_;
    };

    /**
     * @class ssl_client
     * @brief SSL/TLS 客户端
     */
    class ssl_client
    {
    public:
        /**
         * @brief 构造函数
         * @param ctx SSL 上下文
         */
        explicit ssl_client(ssl_context& ctx)
            : ctx_(ctx.context())
        {
        }

        /**
         * @brief 连接 SSL 服务器
         * @param host 主机名
         * @param port 端口
         * @param ec 错误码
         * @return 协程，返回连接指针
         */
        auto connect(std::string_view host, std::string_view port, std::error_code& ec)
            -> net::awaitable<ssl_connection_ptr>
        {
            auto executor = co_await net::this_coro::executor;

            // 解析
            tcp::resolver resolver(executor);
            boost::system::error_code sys_ec;
            auto results = co_await resolver.async_resolve(host, port, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::dns_failed), std::generic_category());
                co_return nullptr;
            }

            // 连接
            tcp::socket socket(executor);
            co_await socket.async_connect(*results.begin(), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::connection_refused), std::generic_category());
                co_return nullptr;
            }

            // 创建 SSL 流
            ssl::stream<tcp::socket> ssl_stream(std::move(socket), ctx_);

            // 设置 SNI（使用 SSL API）
            if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), std::string(host).c_str()))
            {
                ec = std::error_code(static_cast<int>(fault::tls_handshake_failed), std::generic_category());
                co_return nullptr;
            }

            // SSL 握手
            auto conn = make_ssl_connection(std::move(ssl_stream));
            co_await conn->handshake(ssl::stream_base::client, ec);

            if (ec)
            {
                co_return nullptr;
            }

            co_return conn;
        }

    private:
        ssl::context& ctx_;
    };
}