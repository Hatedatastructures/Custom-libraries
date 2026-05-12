/**
 * @file response.hpp
 * @brief HTTP 响应封装
 * @details 基于 Boost.Beast 的 HTTP 响应封装，提供便捷的构造方法。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/beast.hpp>
#include <string>
#include <string_view>

#include <wan/network/agreement/json.hpp>

namespace wan::web
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    /**
     * @class response
     * @brief HTTP 响应封装
     * @details 封装 Boost.Beast HTTP 响应，提供静态工厂方法。
     */
    class response
    {
    public:
        using string_body = http::string_body;
        using underlying_type = http::response<string_body>;

        /**
         * @brief 默认构造
         */
        response() = default;

        /**
         * @brief 从 Beast 响应构造
         */
        explicit response(underlying_type&& resp)
            : underlying_(std::move(resp))
        {
        }

        /**
         * @brief 创建文本响应
         * @param content 文本内容
         * @param status HTTP 状态码
         */
        [[nodiscard]] static auto text(std::string_view content, http::status status = http::status::ok) -> response
        {
            underlying_type resp(status, 11);
            resp.set(http::field::server, "wan/web");
            resp.set(http::field::content_type, "text/plain; charset=utf-8");
            resp.body() = std::string(content);
            resp.prepare_payload();
            return response(std::move(resp));
        }

        /**
         * @brief 创建 HTML 响应
         */
        [[nodiscard]] static auto html(std::string_view content) -> response
        {
            underlying_type resp(http::status::ok, 11);
            resp.set(http::field::server, "wan/web");
            resp.set(http::field::content_type, "text/html; charset=utf-8");
            resp.body() = std::string(content);
            resp.prepare_payload();
            return response(std::move(resp));
        }

        /**
         * @brief 创建 JSON 响应（字符串）
         */
        [[nodiscard]] static auto json(std::string_view content) -> response
        {
            underlying_type resp(http::status::ok, 11);
            resp.set(http::field::server, "wan/web");
            resp.set(http::field::content_type, "application/json; charset=utf-8");
            resp.body() = std::string(content);
            resp.prepare_payload();
            return response(std::move(resp));
        }

        /**
         * @brief 创建重定向响应
         */
        [[nodiscard]] static auto redirect(std::string_view location) -> response
        {
            underlying_type resp(http::status::found, 11);
            resp.set(http::field::server, "wan/web");
            resp.set(http::field::location, location);
            resp.prepare_payload();
            return response(std::move(resp));
        }

        /**
         * @brief 创建错误响应
         */
        [[nodiscard]] static auto error(http::status status, std::string_view message) -> response
        {
            underlying_type resp(status, 11);
            resp.set(http::field::server, "wan/web");
            resp.set(http::field::content_type, "text/plain; charset=utf-8");
            resp.body() = std::string(message);
            resp.prepare_payload();
            return response(std::move(resp));
        }

        /**
         * @brief 创建状态码响应
         */
        [[nodiscard]] static auto status(http::status s) -> response
        {
            underlying_type resp(s, 11);
            resp.set(http::field::server, "wan/web");
            resp.prepare_payload();
            return response(std::move(resp));
        }

        /**
         * @brief 获取底层响应
         */
        [[nodiscard]] auto raw() noexcept -> underlying_type&
        {
            return underlying_;
        }

        /**
         * @brief 获取底层响应（常量）
         */
        [[nodiscard]] auto raw() const noexcept -> const underlying_type&
        {
            return underlying_;
        }

        /**
         * @brief 获取响应体
         */
        [[nodiscard]] auto body() const noexcept -> std::string_view
        {
            return underlying_.body();
        }

        /**
         * @brief 获取状态码
         */
        [[nodiscard]] auto status() const noexcept -> http::status
        {
            return underlying_.result();
        }

    private:
        underlying_type underlying_;
    };
}