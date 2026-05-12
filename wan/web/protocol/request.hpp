/**
 * @file request.hpp
 * @brief HTTP 请求封装
 * @details 基于 Boost.Beast 的 HTTP 请求封装，支持协程读取。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <string>
#include <optional>
#include <system_error>

#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;

    /**
     * @class request
     * @brief HTTP 请求封装
     * @details 封装 Boost.Beast HTTP 请求，提供便捷的访问方法。
     */
    class request
    {
    public:
        using string_body = http::string_body;
        using underlying_type = http::request<string_body>;

        /**
         * @brief 默认构造
         */
        request() = default;

        /**
         * @brief 从 Beast 请求构造
         */
        explicit request(underlying_type&& req)
            : underlying_(std::move(req))
        {
        }

        /**
         * @brief 获取请求方法
         */
        [[nodiscard]] auto method() const noexcept -> http::verb
        {
            return underlying_.method();
        }

        /**
         * @brief 获取请求方法字符串
         */
        [[nodiscard]] auto method_string() const noexcept -> std::string_view
        {
            return underlying_.method_string();
        }

        /**
         * @brief 获取请求路径
         */
        [[nodiscard]] auto target() const noexcept -> std::string_view
        {
            return underlying_.target();
        }

        /**
         * @brief 获取 HTTP 版本
         */
        [[nodiscard]] auto version() const noexcept -> unsigned
        {
            return underlying_.version();
        }

        /**
         * @brief 获取请求体
         */
        [[nodiscard]] auto body() const noexcept -> std::string_view
        {
            return underlying_.body();
        }

        /**
         * @brief 获取请求体（可修改）
         */
        [[nodiscard]] auto body() noexcept -> std::string&
        {
            return underlying_.body();
        }

        /**
         * @brief 获取请求头
         */
        [[nodiscard]] auto header(std::string_view name) const noexcept -> std::string_view
        {
            auto it = underlying_.find(name);
            if (it != underlying_.end())
            {
                return it->value();
            }
            return {};
        }

        /**
         * @brief 检查是否保持连接
         */
        [[nodiscard]] auto keep_alive() const noexcept -> bool
        {
            return underlying_.keep_alive();
        }

        /**
         * @brief 获取底层请求
         */
        [[nodiscard]] auto raw() noexcept -> underlying_type&
        {
            return underlying_;
        }

        /**
         * @brief 获取底层请求（常量）
         */
        [[nodiscard]] auto raw() const noexcept -> const underlying_type&
        {
            return underlying_;
        }

    private:
        underlying_type underlying_;
    };

    /**
     * @brief 从流读取 HTTP 请求
     * @tparam Stream 流类型（TCP socket 或 SSL stream）
     * @param stream 流引用
     * @param buffer 缓冲区
     * @param ec 错误码
     * @return 协程，返回可选的请求对象
     */
    template <typename Stream>
    auto async_read_request(Stream& stream, beast::flat_buffer& buffer, std::error_code& ec)
        -> net::awaitable<std::optional<request>>
    {
        http::request<http::string_body> req;
        boost::system::error_code sys_ec;

        auto token = net::redirect_error(net::use_awaitable, sys_ec);
        co_await http::async_read(stream, buffer, req, token);

        if (sys_ec)
        {
            if (sys_ec == http::error::end_of_stream)
            {
                ec = std::error_code(static_cast<int>(fault::eof), std::generic_category());
            }
            else
            {
                ec = std::error_code(static_cast<int>(fault::http_invalid_request), std::generic_category());
            }
            co_return std::nullopt;
        }

        ec.clear();
        co_return request(std::move(req));
    }
}