/**
 * @file context.hpp
 * @brief 请求上下文
 * @details 封装 HTTP 请求和响应，提供便捷的操作方法，支持 Cookie。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/beast.hpp>
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <map>
#include <optional>

namespace wan::web
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    /**
     * @struct cookie_options
     * @brief Cookie 设置选项
     */
    struct cookie_options
    {
        std::chrono::seconds max_age{0};    // 过期时间（秒）
        std::string path = "/";              // 路径
        std::string domain;                  // 域名
        bool secure = false;                 // 仅 HTTPS
        bool http_only = false;              // 禁止 JS 访问
        std::string same_site;               // SameSite: "strict", "lax", "none"
    };

    /**
     * @class context
     * @brief 请求上下文
     * @details 封装 HTTP 请求和响应，提供路由参数、查询参数、Cookie 和响应方法。
     */
    class context
    {
    public:
        using string_body = http::string_body;
        using request_type = http::request<string_body>;
        using response_type = http::response<string_body>;

        /**
         * @brief 构造函数
         * @param req HTTP 请求
         */
        explicit context(request_type&& req)
            : request_(std::move(req))
            , response_(http::status::ok, request_.version())
            , cookies_parsed_(false)
        {
            response_.set(http::field::server, "wan/web");
            response_.set(http::field::content_type, "text/plain");
        }

        /**
         * @brief 获取请求方法
         */
        [[nodiscard]] auto method() const noexcept -> http::verb
        {
            return request_.method();
        }

        /**
         * @brief 获取请求方法字符串
         */
        [[nodiscard]] auto method_string() const noexcept -> std::string_view
        {
            return request_.method_string();
        }

        /**
         * @brief 获取请求路径
         */
        [[nodiscard]] auto target() const noexcept -> std::string_view
        {
            return request_.target();
        }

        /**
         * @brief 获取请求体
         */
        [[nodiscard]] auto body() const noexcept -> std::string_view
        {
            return request_.body();
        }

        /**
         * @brief 获取请求头
         */
        [[nodiscard]] auto header(std::string_view name) const noexcept -> std::string_view
        {
            auto it = request_.find(name);
            if (it != request_.end())
            {
                return it->value();
            }
            return {};
        }

        // === Cookie 支持 ===

        /**
         * @brief 获取 Cookie 值
         * @param name Cookie 名称
         * @return Cookie 值，不存在返回空
         */
        [[nodiscard]] auto cookie(std::string_view name) -> std::string_view
        {
            parse_cookies();
            auto it = cookies_.find(std::string(name));
            if (it != cookies_.end())
            {
                return it->second;
            }
            return {};
        }

        /**
         * @brief 检查 Cookie 是否存在
         */
        [[nodiscard]] auto has_cookie(std::string_view name) -> bool
        {
            parse_cookies();
            return cookies_.find(std::string(name)) != cookies_.end();
        }

        /**
         * @brief 设置响应 Cookie
         * @param name Cookie 名称
         * @param value Cookie 值
         * @param opts Cookie 选项
         */
        void set_cookie(std::string_view name, std::string_view value, const cookie_options& opts = {})
        {
            std::string cookie_str = std::string(name) + "=" + std::string(value);

            if (opts.max_age.count() > 0)
            {
                cookie_str += "; Max-Age=" + std::to_string(opts.max_age.count());
            }

            if (!opts.path.empty())
            {
                cookie_str += "; Path=" + opts.path;
            }

            if (!opts.domain.empty())
            {
                cookie_str += "; Domain=" + opts.domain;
            }

            if (opts.secure)
            {
                cookie_str += "; Secure";
            }

            if (opts.http_only)
            {
                cookie_str += "; HttpOnly";
            }

            if (!opts.same_site.empty())
            {
                cookie_str += "; SameSite=" + opts.same_site;
            }

            // 添加到响应头（支持多个 Cookie）
            response_.set(http::field::set_cookie, cookie_str);
        }

        /**
         * @brief 删除 Cookie（设置过期）
         */
        void clear_cookie(std::string_view name, const cookie_options& opts = {})
        {
            cookie_options delete_opts = opts;
            delete_opts.max_age = std::chrono::seconds(0);
            set_cookie(name, "", delete_opts);
        }

        // === 参数访问 ===

        /**
         * @brief 获取查询参数
         */
        [[nodiscard]] auto query(std::string_view key) const noexcept -> std::string_view
        {
            auto it = query_params_.find(std::string(key));
            if (it != query_params_.end())
            {
                return it->second;
            }
            return {};
        }

        /**
         * @brief 获取路由参数
         */
        template <typename T>
        [[nodiscard]] auto param(std::string_view name) const -> std::optional<T>
        {
            auto it = route_params_.find(std::string(name));
            if (it == route_params_.end())
            {
                return std::nullopt;
            }
            if constexpr (std::is_same_v<T, int>)
            {
                return std::stoi(it->second);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return it->second;
            }
            return std::nullopt;
        }

        /**
         * @brief 解析 JSON 请求体（返回原始字符串）
         */
        [[nodiscard]] auto json_string() const -> std::string_view
        {
            return request_.body();
        }

        // === 响应设置 ===

        /**
         * @brief 设置响应状态码
         */
        auto status(http::status s) noexcept -> context&
        {
            response_.result(s);
            return *this;
        }

        /**
         * @brief 设置响应状态码（数字）
         */
        auto status(std::uint16_t s) noexcept -> context&
        {
            response_.result(s);
            return *this;
        }

        /**
         * @brief 设置响应头
         */
        auto set(std::string_view name, std::string_view value) noexcept -> context&
        {
            response_.set(name, value);
            return *this;
        }

        /**
         * @brief 设置 Content-Type
         */
        auto content_type(std::string_view type) noexcept -> context&
        {
            response_.set(http::field::content_type, type);
            return *this;
        }

        /**
         * @brief 响应文本
         */
        void text(std::string_view content)
        {
            response_.set(http::field::content_type, "text/plain; charset=utf-8");
            response_.body() = std::string(content);
        }

        /**
         * @brief 响应 HTML
         */
        void html(std::string_view content)
        {
            response_.set(http::field::content_type, "text/html; charset=utf-8");
            response_.body() = std::string(content);
        }

        /**
         * @brief 响应 JSON（字符串）
         */
        void json(std::string_view content)
        {
            response_.set(http::field::content_type, "application/json; charset=utf-8");
            response_.body() = std::string(content);
        }

        /**
         * @brief 重定向
         */
        void redirect(std::string_view location)
        {
            response_.result(http::status::found);
            response_.set(http::field::location, location);
            response_.body() = "";
        }

        /**
         * @brief 错误响应
         */
        void error(std::uint16_t code, std::string_view message)
        {
            response_.result(code);
            response_.set(http::field::content_type, "text/plain; charset=utf-8");
            response_.body() = std::string(message);
        }

        // === 原始访问 ===

        /**
         * @brief 获取原始请求
         */
        [[nodiscard]] auto raw_request() noexcept -> request_type&
        {
            return request_;
        }

        /**
         * @brief 获取原始响应
         */
        [[nodiscard]] auto raw_response() noexcept -> response_type&
        {
            return response_;
        }

        // === 内部设置 ===

        /**
         * @brief 设置路由参数
         */
        void set_route_param(std::string name, std::string value)
        {
            route_params_[std::move(name)] = std::move(value);
        }

        /**
         * @brief 设置查询参数
         */
        void set_query_param(std::string name, std::string value)
        {
            query_params_[std::move(name)] = std::move(value);
        }

    private:
        request_type request_;
        response_type response_;
        std::map<std::string, std::string> route_params_;
        std::unordered_map<std::string, std::string> query_params_;
        std::unordered_map<std::string, std::string> cookies_;
        bool cookies_parsed_;

        /**
         * @brief 解析请求 Cookie
         */
        void parse_cookies()
        {
            if (cookies_parsed_)
            {
                return;
            }
            cookies_parsed_ = true;

            auto cookie_header = header("Cookie");
            if (cookie_header.empty())
            {
                return;
            }

            // 解析 Cookie: name=value; name2=value2
            std::string_view remaining = cookie_header;
            while (!remaining.empty())
            {
                // 查找分隔符
                auto sep_pos = remaining.find(';');
                auto cookie_part = remaining.substr(0, sep_pos);

                // 去掉前导空格
                while (!cookie_part.empty() && cookie_part.front() == ' ')
                {
                    cookie_part = cookie_part.substr(1);
                }

                // 解析 name=value
                auto eq_pos = cookie_part.find('=');
                if (eq_pos != std::string_view::npos && eq_pos > 0)
                {
                    auto name = cookie_part.substr(0, eq_pos);
                    auto value = cookie_part.substr(eq_pos + 1);
                    cookies_[std::string(name)] = std::string(value);
                }

                if (sep_pos == std::string_view::npos)
                {
                    break;
                }
                remaining = remaining.substr(sep_pos + 1);
            }
        }
    };
}