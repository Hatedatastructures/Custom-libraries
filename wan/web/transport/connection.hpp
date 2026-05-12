/**
 * @file connection.hpp
 * @brief 协程连接
 * @details 封装 TCP/SSL 连接，提供协程读写方法。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <optional>
#include <system_error>

#include <wan/web/protocol/request.hpp>
#include <wan/web/protocol/response.hpp>
#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;

    /**
     * @class connection
     * @brief 协程连接
     * @details 封装 TCP socket，提供协程 HTTP 读写方法。
     */
    class connection : public std::enable_shared_from_this<connection>
    {
    public:
        using socket_type = net::ip::tcp::socket;

        /**
         * @brief 构造函数
         * @param socket TCP socket
         */
        explicit connection(socket_type&& socket)
            : socket_(std::move(socket))
            , buffer_()
        {
        }

        /**
         * @brief 获取执行器
         */
        [[nodiscard]] auto executor() noexcept -> net::any_io_executor
        {
            return socket_.get_executor();
        }

        /**
         * @brief 检查是否打开
         */
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return socket_.is_open();
        }

        /**
         * @brief 异步读取 HTTP 请求
         * @param ec 错误码
         * @return 协程，返回可选的请求对象
         */
        auto async_read(std::error_code& ec) -> net::awaitable<std::optional<request>>
        {
            auto self = shared_from_this();
            return async_read_request(socket_, buffer_, ec);
        }

        /**
         * @brief 异步写入 HTTP 响应
         * @param resp 响应对象
         * @param ec 错误码
         */
        auto async_write(const response& resp, std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            auto token = net::redirect_error(net::use_awaitable, sys_ec);
            co_await http::async_write(socket_, resp.raw(), token);

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::http_invalid_response), std::generic_category());
            }
            else
            {
                ec.clear();
            }
        }

        /**
         * @brief 异步写入原始响应
         * @param resp Beast 响应
         * @param ec 错误码
         */
        auto async_write_raw(http::response<http::string_body>& resp, std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            auto token = net::redirect_error(net::use_awaitable, sys_ec);
            co_await http::async_write(socket_, resp, token);

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::http_invalid_response), std::generic_category());
            }
            else
            {
                ec.clear();
            }
        }

        /**
         * @brief 关闭连接
         */
        void close()
        {
            if (socket_.is_open())
            {
                boost::system::error_code ec;
                socket_.close(ec);
            }
        }

        /**
         * @brief 关闭写端（半关闭）
         */
        void shutdown_send()
        {
            if (socket_.is_open())
            {
                boost::system::error_code ec;
                socket_.shutdown(socket_type::shutdown_send, ec);
            }
        }

        /**
         * @brief 获取底层 socket
         */
        [[nodiscard]] auto socket() noexcept -> socket_type&
        {
            return socket_;
        }

        /**
         * @brief 释放 socket（用于 WebSocket 升级）
         */
        [[nodiscard]] auto release_socket() noexcept -> socket_type
        {
            return std::move(socket_);
        }

    private:
        socket_type socket_;
        beast::flat_buffer buffer_;
    };

    /**
     * @brief 连接指针类型
     */
    using connection_ptr = std::shared_ptr<connection>;

    /**
     * @brief 创建连接
     * @param socket TCP socket
     */
    [[nodiscard]] inline auto make_connection(net::ip::tcp::socket socket) -> connection_ptr
    {
        return std::make_shared<connection>(std::move(socket));
    }
}