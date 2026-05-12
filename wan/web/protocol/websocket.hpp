/**
 * @file websocket.hpp
 * @brief WebSocket 协议
 * @details WebSocket 服务器和客户端实现，支持 Ping/Pong 心跳。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <functional>
#include <optional>

#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ws = beast::websocket;

    /**
     * @class websocket
     * @brief WebSocket 连接
     * @details 封装 WebSocket 流，提供协程读写方法、心跳支持。
     */
    class websocket : public std::enable_shared_from_this<websocket>
    {
    public:
        using stream_type = ws::stream<beast::tcp_stream>;

        /**
         * @brief 构造函数
         * @param socket TCP socket
         */
        explicit websocket(net::ip::tcp::socket socket)
            : stream_(std::move(socket))
            , id_(generate_id())
            , open_(true)
        {
        }

        /**
         * @brief 接受 WebSocket 握手（从 HTTP 请求）
         */
        auto accept(http::request<http::string_body> req) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            co_await stream_.async_accept(req, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                open_ = false;
            }
        }

        /**
         * @brief 接受 WebSocket 握手（默认）
         */
        auto accept(std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            boost::system::error_code sys_ec;
            co_await stream_.async_accept(net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::ws_handshake_failed), std::generic_category());
                open_ = false;
            }
            else
            {
                ec.clear();
            }
        }

        // === 发送方法 ===

        /**
         * @brief 发送文本消息
         */
        auto send(std::string_view message) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                co_return;
            }

            boost::system::error_code sys_ec;
            stream_.text(true);
            co_await stream_.async_write(net::buffer(message), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                open_ = false;
            }
        }

        /**
         * @brief 发送文本消息（带错误码）
         */
        auto send_text(std::string_view message, std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                ec = std::error_code(static_cast<int>(fault::ws_connection_closed), std::generic_category());
                co_return;
            }

            boost::system::error_code sys_ec;
            stream_.text(true);
            co_await stream_.async_write(net::buffer(message), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::ws_protocol_error), std::generic_category());
                open_ = false;
            }
            else
            {
                ec.clear();
            }
        }

        /**
         * @brief 发送二进制消息
         */
        auto send_binary(std::string_view data) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                co_return;
            }

            boost::system::error_code sys_ec;
            stream_.binary(true);
            co_await stream_.async_write(net::buffer(data), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                open_ = false;
            }
        }

        /**
         * @brief 发送二进制消息（带错误码）
         */
        auto send_binary(std::string_view data, std::error_code& ec) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                ec = std::error_code(static_cast<int>(fault::ws_connection_closed), std::generic_category());
                co_return;
            }

            boost::system::error_code sys_ec;
            stream_.binary(true);
            co_await stream_.async_write(net::buffer(data), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::ws_protocol_error), std::generic_category());
                open_ = false;
            }
            else
            {
                ec.clear();
            }
        }

        // === 接收方法 ===

        /**
         * @brief 接收消息
         * @return 消息内容（可选）
         */
        auto receive() -> net::awaitable<std::optional<std::string>>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                co_return std::nullopt;
            }

            beast::flat_buffer buffer;
            boost::system::error_code sys_ec;
            co_await stream_.async_read(buffer, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                open_ = false;
                co_return std::nullopt;
            }

            co_return beast::buffers_to_string(buffer.data());
        }

        /**
         * @brief 接收消息（带错误码）
         */
        auto receive(std::error_code& ec) -> net::awaitable<std::optional<std::string>>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                ec = std::error_code(static_cast<int>(fault::ws_connection_closed), std::generic_category());
                co_return std::nullopt;
            }

            beast::flat_buffer buffer;
            boost::system::error_code sys_ec;
            co_await stream_.async_read(buffer, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                if (sys_ec == ws::error::closed)
                {
                    open_ = false;
                    ec.clear();
                    co_return std::nullopt;
                }
                ec = std::error_code(static_cast<int>(fault::ws_protocol_error), std::generic_category());
                open_ = false;
                co_return std::nullopt;
            }

            ec.clear();
            co_return beast::buffers_to_string(buffer.data());
        }

        // === 心跳 ===

        /**
         * @brief 发送 Ping
         */
        auto ping(std::string_view payload = "") -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                co_return;
            }

            boost::system::error_code sys_ec;
            co_await stream_.async_ping(ws::ping_data(payload), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                open_ = false;
            }
        }

        /**
         * @brief 发送 Pong
         */
        auto pong(std::string_view payload = "") -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                co_return;
            }

            boost::system::error_code sys_ec;
            co_await stream_.async_pong(ws::ping_data(payload), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                open_ = false;
            }
        }

        /**
         * @brief 设置 Ping 间隔（自动心跳）
         */
        void set_ping_interval(std::chrono::seconds interval)
        {
            ping_interval_ = interval;
        }

        /**
         * @brief 设置 Ping 回调
         */
        void on_ping(std::function<net::awaitable<void>(std::string_view)> handler)
        {
            ping_handler_ = std::move(handler);
        }

        /**
         * @brief 设置 Pong 回调
         */
        void on_pong(std::function<void(std::string_view)> handler)
        {
            pong_handler_ = std::move(handler);
        }

        // === 关闭 ===

        /**
         * @brief 关闭连接
         */
        void close()
        {
            if (!open_)
            {
                return;
            }

            open_ = false;
            boost::system::error_code ec;
            stream_.close(ws::close_reason(ws::close_code::normal), ec);
        }

        /**
         * @brief 关闭连接（指定原因）
         */
        auto close(ws::close_code code, std::string_view reason = "") -> net::awaitable<void>
        {
            auto self = shared_from_this();
            if (!open_)
            {
                co_return;
            }

            open_ = false;
            boost::system::error_code sys_ec;
            co_await stream_.async_close(ws::close_reason(code, std::string(reason)),
                net::redirect_error(net::use_awaitable, sys_ec));
        }

        // === 状态 ===

        /**
         * @brief 检查是否打开
         */
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return open_ && stream_.is_open();
        }

        /**
         * @brief 获取连接 ID
         */
        [[nodiscard]] auto id() const noexcept -> const std::string&
        {
            return id_;
        }

        /**
         * @brief 获取底层流
         */
        [[nodiscard]] auto stream() noexcept -> stream_type&
        {
            return stream_;
        }

        /**
         * @brief 获取 socket
         */
        [[nodiscard]] auto socket() noexcept -> net::ip::tcp::socket&
        {
            return stream_.next_layer().socket();
        }

        /**
         * @brief 检查是否是文本消息
         */
        [[nodiscard]] auto got_text() const noexcept -> bool
        {
            return stream_.got_text();
        }

        /**
         * @brief 设置控制帧回调
         */
        void control_callback(std::function<void(ws::frame_type, std::string_view)> cb)
        {
            stream_.control_callback(std::move(cb));
        }

    private:
        stream_type stream_;
        std::string id_;
        std::atomic<bool> open_{true};
        std::chrono::seconds ping_interval_{0};
        std::function<net::awaitable<void>(std::string_view)> ping_handler_;
        std::function<void(std::string_view)> pong_handler_;

        /**
         * @brief 生成唯一 ID
         */
        static auto generate_id() -> std::string
        {
            static std::atomic<std::size_t> counter{0};
            return "ws-" + std::to_string(++counter) + "-" +
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        }
    };

    /**
     * @brief WebSocket 指针
     */
    using websocket_ptr = std::shared_ptr<websocket>;

    /**
     * @brief 创建 WebSocket
     */
    [[nodiscard]] inline auto make_websocket(net::ip::tcp::socket socket) -> websocket_ptr
    {
        return std::make_shared<websocket>(std::move(socket));
    }

    // === 旧接口兼容（ws_connection 别名） ===

    using ws_connection = websocket;
    using ws_connection_ptr = websocket_ptr;
    using ws_handler = std::function<net::awaitable<void>(websocket_ptr)>;

    [[nodiscard]] inline auto make_ws_connection(net::ip::tcp::socket socket) -> websocket_ptr
    {
        return make_websocket(std::move(socket));
    }

    /**
     * @class ws_client
     * @brief WebSocket 客户端
     */
    class ws_client : public std::enable_shared_from_this<ws_client>
    {
    public:
        /**
         * @brief 连接 WebSocket 服务器
         */
        auto connect(std::string_view host, std::string_view port, std::string_view target)
            -> net::awaitable<websocket_ptr>
        {
            std::error_code ec;
            auto conn = co_await connect(host, port, target, ec);
            co_return conn;
        }

        auto connect(std::string_view host, std::string_view port, std::string_view target, std::error_code& ec)
            -> net::awaitable<websocket_ptr>
        {
            auto self = shared_from_this();
            auto executor = co_await net::this_coro::executor;

            // 解析
            net::ip::tcp::resolver resolver(executor);
            boost::system::error_code sys_ec;
            auto results = co_await resolver.async_resolve(host, port, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::dns_failed), std::generic_category());
                co_return nullptr;
            }

            // 连接
            beast::tcp_stream stream(executor);
            co_await stream.async_connect(results, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::connection_refused), std::generic_category());
                co_return nullptr;
            }

            // WebSocket 握手
            auto conn = make_websocket(stream.release_socket());
            co_await conn->stream().async_handshake(std::string(host), std::string(target),
                net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::ws_handshake_failed), std::generic_category());
                co_return nullptr;
            }

            ec.clear();
            co_return conn;
        }

        /**
         * @brief 连接 WebSocket 服务器（SSL）
         */
        auto connect_ssl(std::string_view host, std::string_view port, std::string_view target)
            -> net::awaitable<websocket_ptr>
        {
            std::error_code ec;
            auto conn = co_await connect_ssl(host, port, target, ec);
            co_return conn;
        }

        auto connect_ssl(std::string_view host, std::string_view port, std::string_view target, std::error_code& ec)
            -> net::awaitable<websocket_ptr>
        {
            // TODO: 实现 SSL WebSocket 连接
            ec = std::error_code(static_cast<int>(fault::not_implemented), std::generic_category());
            co_return nullptr;
        }
    };

    /**
     * @brief 创建 WebSocket 客户端
     */
    [[nodiscard]] inline auto make_ws_client() -> std::shared_ptr<ws_client>
    {
        return std::make_shared<ws_client>();
    }
}