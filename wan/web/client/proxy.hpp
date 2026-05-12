/**
 * @file proxy.hpp
 * @brief HTTPS 代理
 * @details HTTP 反向代理，支持路由转发。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <openssl/ssl.h>
#include <memory>
#include <string>
#include <unordered_map>

#include <wan/web/core/executor.hpp>
#include <wan/web/transport/connection.hpp>
#include <wan/web/transport/ssl.hpp>
#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;

    /**
     * @struct route_rule
     * @brief 代理路由规则
     */
    struct route_rule
    {
        std::string domain;     // 匹配域名
        std::string upstream;   // 上游地址
        bool use_ssl = false;   // 是否使用 SSL
    };

    /**
     * @class proxy
     * @brief HTTPS 代理
     * @details HTTP 反向代理，支持域名路由和 SSL 上游。
     */
    class proxy : public std::enable_shared_from_this<proxy>
    {
    public:
        /**
         * @brief 默认构造
         */
        proxy()
            : executor_(make_executor())
            , ssl_ctx_()
            , port_(0)
        {
        }

        /**
         * @brief 添加路由规则
         * @param domain 匹配域名（支持 * 通配符）
         * @param upstream 上游地址（如 "backend.example.com:8080"）
         * @return 代理引用
         */
        auto route(std::string_view domain, std::string_view upstream) -> proxy&
        {
            route_rule rule;
            rule.domain = std::string(domain);
            rule.upstream = std::string(upstream);

            // 检测是否需要 SSL
            if (upstream.find("https://") == 0 || upstream.find(":443") != std::string_view::npos)
            {
                rule.use_ssl = true;
                // 去掉协议前缀
                if (upstream.find("https://") == 0)
                {
                    rule.upstream = std::string(upstream.substr(8));
                }
            }
            else if (upstream.find("http://") == 0)
            {
                rule.upstream = std::string(upstream.substr(7));
            }

            routes_.push_back(std::move(rule));
            return *this;
        }

        /**
         * @brief 设置监听端口
         */
        auto listen(std::uint16_t port) -> proxy&
        {
            port_ = port;
            return *this;
        }

        /**
         * @brief 运行代理
         */
        auto run() -> net::awaitable<void>
        {
            auto self = shared_from_this();

            // 创建 acceptor
            net::ip::tcp::acceptor acceptor(executor_->context());
            net::ip::tcp::endpoint endpoint(net::ip::tcp::v4(), port_);

            acceptor.open(endpoint.protocol());
            acceptor.set_option(net::socket_base::reuse_address(true));
            acceptor.bind(endpoint);
            acceptor.listen(net::socket_base::max_listen_connections);

            // 启动执行器
            executor_->start();

            // 接受循环
            while (!executor_->context().stopped())
            {
                boost::system::error_code ec;
                auto socket = co_await acceptor.async_accept(net::redirect_error(net::use_awaitable, ec));

                if (ec)
                {
                    if (ec == net::error::operation_aborted)
                    {
                        co_return;
                    }
                    continue;
                }

                // 处理连接
                executor_->spawn([self, socket = std::move(socket)]() mutable -> net::awaitable<void>
                {
                    co_await self->handle_client(make_connection(std::move(socket)));
                });
            }
        }

        /**
         * @brief 停止代理
         */
        void stop()
        {
            executor_->stop();
        }

    private:
        /**
         * @brief 处理客户端连接
         */
        auto handle_client(connection_ptr client_conn) -> net::awaitable<void>
        {
            // 读取客户端请求
            std::error_code ec;
            auto req_opt = co_await client_conn->async_read(ec);

            if (ec || !req_opt)
            {
                client_conn->close();
                co_return;
            }

            auto& req = req_opt.value();

            // 提取目标域名
            auto host = req.header("Host");
            if (host.empty())
            {
                client_conn->close();
                co_return;
            }

            // 去掉端口
            auto colon_pos = host.find(':');
            if (colon_pos != std::string_view::npos)
            {
                host = host.substr(0, colon_pos);
            }

            // 匹配路由
            auto* rule = match_route(host);
            if (!rule)
            {
                // 404
                http::response<http::string_body> resp(http::status::not_found, 11);
                resp.set(http::field::server, "wan/web/proxy");
                resp.body() = "No route found";
                resp.prepare_payload();
                co_await client_conn->async_write_raw(resp, ec);
                client_conn->close();
                co_return;
            }

            // 解析上游地址
            auto upstream_host = rule->upstream;
            std::string upstream_port = "80";
            auto upstream_colon = upstream_host.find(':');
            if (upstream_colon != std::string::npos)
            {
                upstream_port = upstream_host.substr(upstream_colon + 1);
                upstream_host = upstream_host.substr(0, upstream_colon);
            }
            if (rule->use_ssl)
            {
                upstream_port = "443";
            }

            // 转发请求
            co_await forward_request(client_conn, req.raw(), upstream_host, upstream_port, rule->use_ssl);
        }

        /**
         * @brief 匹配路由规则
         */
        [[nodiscard]] auto match_route(std::string_view host) const -> const route_rule*
        {
            for (const auto& rule : routes_)
            {
                // 精确匹配
                if (rule.domain == host)
                {
                    return &rule;
                }

                // 通配符匹配
                if (rule.domain.find('*') != std::string::npos)
                {
                    // 简化：仅支持 *.example.com
                    auto suffix = rule.domain.substr(2);
                    if (host.size() >= suffix.size() && host.substr(host.size() - suffix.size()) == suffix)
                    {
                        return &rule;
                    }
                }
            }
            return nullptr;
        }

        /**
         * @brief 转发请求到上游
         */
        auto forward_request(connection_ptr client_conn, http::request<http::string_body>& req,
                             std::string_view upstream_host, std::string_view upstream_port, bool use_ssl)
            -> net::awaitable<void>
        {
            auto executor = co_await net::this_coro::executor;

            // 解析上游地址
            net::ip::tcp::resolver resolver(executor);
            boost::system::error_code sys_ec;
            auto results = co_await resolver.async_resolve(upstream_host, upstream_port, net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                http::response<http::string_body> resp(http::status::bad_gateway, 11);
                resp.body() = "DNS resolution failed";
                resp.prepare_payload();
                std::error_code ec;
                co_await client_conn->async_write_raw(resp, ec);
                client_conn->close();
                co_return;
            }

            // 连接上游
            net::ip::tcp::socket upstream_socket(executor);
            co_await upstream_socket.async_connect(*results.begin(), net::redirect_error(net::use_awaitable, sys_ec));

            if (sys_ec)
            {
                http::response<http::string_body> resp(http::status::bad_gateway, 11);
                resp.body() = "Connection refused";
                resp.prepare_payload();
                std::error_code ec;
                co_await client_conn->async_write_raw(resp, ec);
                client_conn->close();
                co_return;
            }

            // 修改请求头部
            req.set(http::field::host, std::string(upstream_host) + ":" + std::string(upstream_port));
            req.erase(http::field::connection);
            req.prepare_payload();

            std::error_code ec;

            if (use_ssl)
            {
                // SSL 连接
                ssl::stream<net::ip::tcp::socket> ssl_stream(std::move(upstream_socket), ssl_ctx_.context());

                // 设置 SNI（使用 SSL API）
                if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), std::string(upstream_host).c_str()))
                {
                    http::response<http::string_body> resp(http::status::bad_gateway, 11);
                    resp.body() = "SNI setup failed";
                    resp.prepare_payload();
                    co_await client_conn->async_write_raw(resp, ec);
                    client_conn->close();
                    co_return;
                }

                boost::system::error_code ssl_ec;
                co_await ssl_stream.async_handshake(ssl::stream_base::client, net::redirect_error(net::use_awaitable, ssl_ec));

                if (ssl_ec)
                {
                    http::response<http::string_body> resp(http::status::bad_gateway, 11);
                    resp.body() = "SSL handshake failed";
                    resp.prepare_payload();
                    co_await client_conn->async_write_raw(resp, ec);
                    client_conn->close();
                    co_return;
                }

                // 发送请求
                co_await http::async_write(ssl_stream, req, net::redirect_error(net::use_awaitable, sys_ec));

                if (sys_ec)
                {
                    client_conn->close();
                    co_return;
                }

                // 读取响应
                beast::flat_buffer buffer;
                http::response<http::string_body> resp;
                co_await http::async_read(ssl_stream, buffer, resp, net::redirect_error(net::use_awaitable, sys_ec));

                if (sys_ec)
                {
                    client_conn->close();
                    co_return;
                }

                // 发送响应给客户端
                resp.prepare_payload();
                co_await client_conn->async_write_raw(resp, ec);

                // 关闭 SSL
                co_await ssl_stream.async_shutdown(net::redirect_error(net::use_awaitable, ssl_ec));
            }
            else
            {
                // 发送请求
                co_await http::async_write(upstream_socket, req, net::redirect_error(net::use_awaitable, sys_ec));

                if (sys_ec)
                {
                    client_conn->close();
                    co_return;
                }

                // 读取响应
                beast::flat_buffer buffer;
                http::response<http::string_body> resp;
                co_await http::async_read(upstream_socket, buffer, resp, net::redirect_error(net::use_awaitable, sys_ec));

                if (sys_ec)
                {
                    client_conn->close();
                    co_return;
                }

                // 发送响应给客户端
                resp.prepare_payload();
                co_await client_conn->async_write_raw(resp, ec);
            }

            client_conn->close();
        }

        std::shared_ptr<executor> executor_;
        ssl_context ssl_ctx_;
        std::vector<route_rule> routes_;
        std::uint16_t port_;
    };

    /**
     * @brief 创建代理
     */
    [[nodiscard]] inline auto make_proxy() -> std::shared_ptr<proxy>
    {
        return std::make_shared<proxy>();
    }
}