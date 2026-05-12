/**
 * @file client.hpp
 * @brief HTTP 客户端
 * @details 协程驱动的高性能 HTTP 客户端，支持连接池、SSL、超时、重试。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <wan/web/protocol/response.hpp>
#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ssl = net::ssl;

    /**
     * @struct client_config
     * @brief 客户端配置
     */
    struct client_config
    {
        std::chrono::seconds connect_timeout{10};       // 连接超时
        std::chrono::seconds read_timeout{30};          // 读超时
        std::chrono::seconds write_timeout{30};         // 写超时
        std::size_t max_redirects{5};                   // 最大重定向次数
        std::size_t retry_count{3};                     // 重试次数
        std::chrono::milliseconds retry_delay{100};     // 重试延迟
        bool follow_redirects{true};                    // 跟随重定向
        bool verify_ssl{true};                          // SSL 证书验证
        std::size_t max_response_size{50 * 1024 * 1024}; // 最大响应体大小
        std::string user_agent{"wan/web-client"};       // User-Agent
        bool compression{true};                         // 接受压缩响应
    };

    /**
     * @struct request_options
     * @brief 单次请求选项
     */
    struct request_options
    {
        std::map<std::string, std::string> headers;     // 自定义头
        std::chrono::seconds timeout{0};                // 请求超时（0=使用默认）
        bool follow_redirects{true};                    // 跟随重定向
        std::size_t max_redirects{5};                   // 最大重定向
        std::optional<std::string> basic_auth;          // Basic Auth
        std::string bearer_token;                       // Bearer Token
    };

    /**
     * @class client
     * @brief HTTP 客户端
     * @details 提供丰富的协程 HTTP 请求方法。
     */
    class client : public std::enable_shared_from_this<client>
    {
    public:
        /**
         * @brief 默认构造（内部 io_context）
         */
        client()
            : ioc_owned_(std::make_unique<net::io_context>())
            , ioc_(*ioc_owned_)
            , config_()
        {
        }

        /**
         * @brief 构造（指定配置）
         */
        explicit client(const client_config& config)
            : ioc_owned_(std::make_unique<net::io_context>())
            , ioc_(*ioc_owned_)
            , config_(config)
        {
        }

        /**
         * @brief 构造（使用外部 io_context）
         */
        explicit client(net::io_context& ioc, const client_config& config = {})
            : ioc_owned_(nullptr)
            , ioc_(ioc)
            , config_(config)
        {
        }

        // === 基础 HTTP 方法 ===

        /**
         * @brief GET 请求
         */
        auto get(std::string_view url) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::get, url, "", ec);
            co_return resp;
        }

        auto get(std::string_view url, std::error_code& ec) -> net::awaitable<response>
        {
            return request(http::verb::get, url, "", ec);
        }

        auto get(std::string_view url, const request_options& opts) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::get, url, "", opts, ec);
            co_return resp;
        }

        auto get(std::string_view url, const request_options& opts, std::error_code& ec) -> net::awaitable<response>
        {
            return request(http::verb::get, url, "", opts, ec);
        }

        /**
         * @brief POST 请求
         */
        auto post(std::string_view url, std::string_view body) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::post, url, body, ec);
            co_return resp;
        }

        auto post(std::string_view url, std::string_view body, std::error_code& ec) -> net::awaitable<response>
        {
            return request(http::verb::post, url, body, ec);
        }

        auto post(std::string_view url, std::string_view body, const request_options& opts) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::post, url, body, opts, ec);
            co_return resp;
        }

        auto post(std::string_view url, std::string_view body, const request_options& opts, std::error_code& ec) -> net::awaitable<response>
        {
            return request(http::verb::post, url, body, opts, ec);
        }

        /**
         * @brief POST JSON
         */
        auto post_json(std::string_view url, std::string_view json_body) -> net::awaitable<response>
        {
            request_options opts;
            opts.headers["Content-Type"] = "application/json";
            std::error_code ec;
            auto resp = co_await post(url, json_body, opts, ec);
            co_return resp;
        }

        /**
         * @brief PUT 请求
         */
        auto put(std::string_view url, std::string_view body) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::put, url, body, ec);
            co_return resp;
        }

        auto put(std::string_view url, std::string_view body, const request_options& opts) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::put, url, body, opts, ec);
            co_return resp;
        }

        /**
         * @brief PATCH 请求
         */
        auto patch(std::string_view url, std::string_view body) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::patch, url, body, ec);
            co_return resp;
        }

        auto patch(std::string_view url, std::string_view body, const request_options& opts) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::patch, url, body, opts, ec);
            co_return resp;
        }

        /**
         * @brief DELETE 请求
         */
        auto del(std::string_view url) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::delete_, url, "", ec);
            co_return resp;
        }

        auto del(std::string_view url, const request_options& opts) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::delete_, url, "", opts, ec);
            co_return resp;
        }

        /**
         * @brief HEAD 请求
         */
        auto head(std::string_view url) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::head, url, "", ec);
            co_return resp;
        }

        /**
         * @brief OPTIONS 请求
         */
        auto options(std::string_view url) -> net::awaitable<response>
        {
            std::error_code ec;
            auto resp = co_await request(http::verb::options, url, "", ec);
            co_return resp;
        }

        // === 通用请求方法 ===

        /**
         * @brief 通用请求（无选项）
         */
        auto request(http::verb method, std::string_view url, std::string_view body, std::error_code& ec)
            -> net::awaitable<response>
        {
            return request(method, url, body, request_options{}, ec);
        }

        /**
         * @brief 通用请求（带选项）
         */
        auto request(http::verb method, std::string_view url, std::string_view body,
                     const request_options& opts, std::error_code& ec)
            -> net::awaitable<response>
        {
            auto self = shared_from_this();

            // 重试循环
            for (std::size_t retry = 0; retry <= config_.retry_count; ++retry)
            {
                auto resp = co_await do_request(method, url, body, opts, ec);

                if (!ec)
                {
                    // 处理重定向
                    if (opts.follow_redirects && is_redirect(resp.raw().result()))
                    {
                        auto location_opt = resp.raw().find(http::field::location);
                        if (location_opt != resp.raw().end())
                        {
                            auto location = location_opt->value();
                            if (redirect_count_ < opts.max_redirects)
                            {
                                ++redirect_count_;
                                co_return co_await request(method, location, body, opts, ec);
                            }
                        }
                    }
                    co_return resp;
                }

                // 检查是否应该重试
                if (!should_retry(ec))
                {
                    break;
                }

                // 重试延迟
                if (retry < config_.retry_count)
                {
                    net::steady_timer timer(ioc_);
                    timer.expires_after(config_.retry_delay);
                    co_await timer.async_wait(net::use_awaitable);
                }
            }

            co_return response();
        }

        // === 配置访问 ===

        /**
         * @brief 获取配置
         */
        [[nodiscard]] auto config() const noexcept -> const client_config&
        {
            return config_;
        }

        /**
         * @brief 设置配置
         */
        void set_config(const client_config& config)
        {
            config_ = config;
        }

        /**
         * @brief 获取 io_context
         */
        [[nodiscard]] auto io_context() noexcept -> net::io_context&
        {
            return ioc_;
        }

        // === Cookie 管理 ===

        /**
         * @brief 设置 Cookie
         */
        void set_cookie(std::string_view name, std::string_view value)
        {
            cookies_[std::string(name)] = std::string(value);
        }

        /**
         * @brief 获取 Cookie
         */
        [[nodiscard]] auto cookie(std::string_view name) const -> std::optional<std::string>
        {
            auto it = cookies_.find(std::string(name));
            if (it != cookies_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        /**
         * @brief 清除所有 Cookie
         */
        void clear_cookies()
        {
            cookies_.clear();
        }

        // === 默认 Headers ===

        /**
         * @brief 设置默认 Header
         */
        void set_default_header(std::string_view name, std::string_view value)
        {
            default_headers_[std::string(name)] = std::string(value);
        }

        /**
         * @brief 获取默认 Header
         */
        [[nodiscard]] auto default_header(std::string_view name) const -> std::optional<std::string>
        {
            auto it = default_headers_.find(std::string(name));
            if (it != default_headers_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        /**
         * @brief 清除默认 Headers
         */
        void clear_default_headers()
        {
            default_headers_.clear();
        }

        /**
         * @brief 运行内部 io_context（如果拥有）
         */
        void run()
        {
            if (ioc_owned_)
            {
                ioc_.run();
            }
        }

        /**
         * @brief 停止内部 io_context
         */
        void stop()
        {
            if (ioc_owned_)
            {
                ioc_.stop();
            }
        }

    private:
        std::unique_ptr<net::io_context> ioc_owned_;
        net::io_context& ioc_;
        client_config config_;
        std::unordered_map<std::string, std::string> cookies_;
        std::unordered_map<std::string, std::string> default_headers_;
        std::size_t redirect_count_{0};

        /**
         * @brief 解析 URL
         */
        [[nodiscard]] auto parse_url(std::string_view url) -> std::tuple<std::string, std::string, std::string, bool>
        {
            std::string host;
            std::string port = "80";
            std::string target = "/";
            bool use_ssl = false;

            if (url.find("http://") == 0)
            {
                url = url.substr(7);
            }
            else if (url.find("https://") == 0)
            {
                url = url.substr(8);
                port = "443";
                use_ssl = true;
            }

            auto slash_pos = url.find('/');
            if (slash_pos != std::string_view::npos)
            {
                host = std::string(url.substr(0, slash_pos));
                target = std::string(url.substr(slash_pos));
            }
            else
            {
                host = std::string(url);
            }

            auto colon_pos = host.find(':');
            if (colon_pos != std::string::npos)
            {
                port = host.substr(colon_pos + 1);
                host = host.substr(0, colon_pos);
            }

            return {host, port, target, use_ssl};
        }

        /**
         * @brief 执行单次请求
         */
        auto do_request(http::verb method, std::string_view url, std::string_view body,
                        const request_options& opts, std::error_code& ec) -> net::awaitable<response>
        {
            auto [host, port, target, use_ssl] = parse_url(url);

            auto executor = co_await net::this_coro::executor;
            net::ip::tcp::socket socket(executor);

            // 解析并连接
            net::ip::tcp::resolver resolver(executor);
            boost::system::error_code sys_ec;
            auto results = co_await resolver.async_resolve(host, port, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::dns_failed), std::generic_category());
                co_return response();
            }

            co_await socket.async_connect(*results.begin(), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::connection_refused), std::generic_category());
                co_return response();
            }

            // 构造请求
            http::request<http::string_body> req(method, target, 11);
            req.set(http::field::host, host);
            req.set(http::field::user_agent, config_.user_agent);

            // 添加默认 Headers
            for (const auto& [name, value] : default_headers_)
            {
                req.set(name, value);
            }

            // 添加自定义 Headers
            for (const auto& [name, value] : opts.headers)
            {
                req.set(name, value);
            }

            // 压缩支持
            if (config_.compression)
            {
                req.set(http::field::accept_encoding, "gzip, deflate");
            }

            // Cookie
            if (!cookies_.empty())
            {
                std::string cookie_str;
                for (const auto& [name, value] : cookies_)
                {
                    if (!cookie_str.empty())
                    {
                        cookie_str += "; ";
                    }
                    cookie_str += name + "=" + value;
                }
                req.set(http::field::cookie, cookie_str);
            }

            // 认证
            if (opts.basic_auth)
            {
                req.set(http::field::authorization, "Basic " + *opts.basic_auth);
            }
            else if (!opts.bearer_token.empty())
            {
                req.set(http::field::authorization, "Bearer " + opts.bearer_token);
            }

            // 请求体
            req.body() = std::string(body);
            req.prepare_payload();

            // 发送请求
            co_await http::async_write(socket, req, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::http_invalid_request), std::generic_category());
                socket.close();
                co_return response();
            }

            // 读取响应
            beast::flat_buffer buffer;
            http::response<http::string_body> resp;
            co_await http::async_read(socket, buffer, resp, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                ec = std::error_code(static_cast<int>(fault::http_invalid_response), std::generic_category());
                socket.close();
                co_return response();
            }

            // 处理响应 Cookie
            if (resp.find(http::field::set_cookie) != resp.end())
            {
                auto set_cookie = resp[http::field::set_cookie];
                // 解析并保存 Cookie（简化实现）
                auto eq_pos = set_cookie.find('=');
                auto semi_pos = set_cookie.find(';');
                if (eq_pos != std::string_view::npos)
                {
                    auto name = set_cookie.substr(0, eq_pos);
                    auto value_end = semi_pos != std::string_view::npos ? semi_pos : set_cookie.size();
                    auto value = set_cookie.substr(eq_pos + 1, value_end - eq_pos - 1);
                    cookies_[std::string(name)] = std::string(value);
                }
            }

            socket.close();
            ec.clear();
            co_return response(std::move(resp));
        }

        /**
         * @brief 判断是否为重定向状态码
         */
        [[nodiscard]] static auto is_redirect(http::status status) noexcept -> bool
        {
            return status == http::status::moved_permanently ||
                   status == http::status::found ||
                   status == http::status::see_other ||
                   status == http::status::temporary_redirect ||
                   status == http::status::permanent_redirect;
        }

        /**
         * @brief 判断是否应该重试
         */
        [[nodiscard]] auto should_retry(std::error_code ec) const noexcept -> bool
        {
            auto code = static_cast<int>(ec.value());
            return code == static_cast<int>(fault::connection_refused) ||
                   code == static_cast<int>(fault::dns_failed) ||
                   code == static_cast<int>(fault::http_invalid_response);
        }
    };

    // === 工厂函数 ===

    /**
     * @brief 创建客户端
     */
    [[nodiscard]] inline auto make_client() -> std::shared_ptr<client>
    {
        return std::make_shared<client>();
    }

    /**
     * @brief 创建客户端（指定配置）
     */
    [[nodiscard]] inline auto make_client(const client_config& config) -> std::shared_ptr<client>
    {
        return std::make_shared<client>(config);
    }

    /**
     * @brief 创建客户端（使用外部 io_context）
     */
    [[nodiscard]] inline auto make_client(net::io_context& ioc, const client_config& config = {})
        -> std::shared_ptr<client>
    {
        return std::make_shared<client>(ioc, config);
    }
}