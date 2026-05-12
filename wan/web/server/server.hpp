/**
 * @file server.hpp
 * @brief Web 服务器
 * @details 协程驱动的高性能 Web 服务器，支持 Keep-Alive、静态文件、监控指标。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wan/web/core/executor.hpp>
#include <wan/web/core/context.hpp>
#include <wan/web/core/metrics.hpp>
#include <wan/web/router/router.hpp>
#include <wan/web/transport/connection.hpp>
#include <wan/web/transport/limits.hpp>
#include <wan/web/middleware/middleware.hpp>
#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;

    /**
     * @struct server_config
     * @brief 服务器配置
     */
    struct server_config
    {
        std::size_t threads = 0;                    // 线程数（0=自动检测）
        std::chrono::seconds read_timeout{30};      // 读超时
        std::chrono::seconds write_timeout{30};     // 写超时
        std::chrono::seconds keep_alive_timeout{60};// Keep-Alive 空闲超时
        std::size_t max_body_size = 10 * 1024 * 1024; // 最大请求体 (10MB)
        std::size_t max_connections = 10000;        // 最大并发连接
        bool keep_alive_enabled = true;             // 启用 Keep-Alive
        std::size_t max_keepalive_requests = 100;   // 单连接最大请求数
        bool tcp_nodelay = true;                    // TCP_NODELAY
        bool reuse_address = true;                  // SO_REUSEADDR
        bool enable_metrics = true;                 // 启用监控指标
        std::string metrics_path = "/metrics";      // 指标路径
        std::string health_path = "/health";        // 健康检查路径
    };

    /**
     * @class server
     * @brief Web 服务器
     * @details 协程驱动的高性能 Web 服务器。
     */
    class server : public std::enable_shared_from_this<server>
    {
    public:
        /**
         * @brief 默认构造
         */
        server()
            : config_()
            , executor_ptr_(make_executor(config_.threads))
            , router_()
            , limiter_(executor_ptr_->strand(), config_.max_connections)
            , metrics_(std::make_shared<server_metrics>())
            , port_(0)
            , running_(false)
            , requests_count_(0)
        {
            setup_builtin_routes();
        }

        /**
         * @brief 构造（指定配置）
         */
        explicit server(const server_config& config)
            : config_(config)
            , executor_ptr_(make_executor(config.threads))
            , router_()
            , limiter_(executor_ptr_->strand(), config.max_connections)
            , metrics_(std::make_shared<server_metrics>())
            , port_(0)
            , running_(false)
            , requests_count_(0)
        {
            setup_builtin_routes();
        }

        // === 路由注册 ===

        /**
         * @brief 注册 GET 路由
         */
        auto get(std::string_view pattern, handler h) -> server&
        {
            router_.get(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 POST 路由
         */
        auto post(std::string_view pattern, handler h) -> server&
        {
            router_.post(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 PUT 路由
         */
        auto put(std::string_view pattern, handler h) -> server&
        {
            router_.put(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 DELETE 路由
         */
        auto del(std::string_view pattern, handler h) -> server&
        {
            router_.del(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 PATCH 路由
         */
        auto patch(std::string_view pattern, handler h) -> server&
        {
            router_.patch(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 HEAD 路由
         */
        auto head(std::string_view pattern, handler h) -> server&
        {
            router_.head(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 OPTIONS 路由
         */
        auto options(std::string_view pattern, handler h) -> server&
        {
            router_.options(pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册任意方法路由
         */
        auto route(http::verb method, std::string_view pattern, handler h) -> server&
        {
            router_.route(method, pattern, std::move(h));
            return *this;
        }

        // === 中间件 ===

        /**
         * @brief 注册中间件
         */
        auto use(middleware m) -> server&
        {
            middlewares_.push_back(std::move(m));
            return *this;
        }

        // === 监听配置 ===

        /**
         * @brief 设置 HTTP 监听端口
         */
        auto listen(std::uint16_t port) -> server&
        {
            port_ = port;
            return *this;
        }

        // === 监控指标 ===

        /**
         * @brief 获取监控指标
         */
        [[nodiscard]] auto metrics() noexcept -> std::shared_ptr<server_metrics>&
        {
            return metrics_;
        }

        /**
         * @brief 获取 Prometheus 格式指标输出
         */
        [[nodiscard]] auto prometheus_metrics() const -> std::string
        {
            return metrics_->to_prometheus();
        }

        // === 运行控制 ===

        /**
         * @brief 运行服务器
         */
        auto run() -> net::awaitable<void>
        {
            auto self = shared_from_this();

            running_ = true;

            // 启动执行器
            executor_ptr_->start();

            // 创建 acceptor
            net::ip::tcp::acceptor acceptor(executor_ptr_->context());
            net::ip::tcp::endpoint endpoint(net::ip::tcp::v4(), port_);

            acceptor.open(endpoint.protocol());
            acceptor.set_option(net::socket_base::reuse_address(config_.reuse_address));
            acceptor.bind(endpoint);
            acceptor.listen(net::socket_base::max_listen_connections);

            // 接受循环
            while (running_)
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

                // 设置 socket 选项
                if (config_.tcp_nodelay)
                {
                    socket.set_option(net::ip::tcp::no_delay(true), ec);
                }

                // 尝试获取连接许可（异步）
                auto acquired = co_await limiter_.async_acquire();
                if (!acquired)
                {
                    // 连接数超限，拒绝连接
                    boost::system::error_code close_ec;
                    socket.close(close_ec);
                    continue;
                }

                metrics_->inc_active_connections();

                // 处理连接
                executor_ptr_->spawn([self, socket = std::move(socket)]() mutable -> net::awaitable<void>
                {
                    co_await self->handle_connection(make_connection(std::move(socket)));
                    self->limiter_.release();
                    self->metrics_->dec_active_connections();
                });
            }
        }

        /**
         * @brief 同步运行（阻塞）
         */
        void run_sync()
        {
            executor_ptr_->start();
            executor_ptr_->context().run();
        }

        /**
         * @brief 停止服务器
         */
        void stop()
        {
            running_ = false;
            executor_ptr_->stop();
        }

        /**
         * @brief 检查服务器是否运行
         */
        [[nodiscard]] auto is_running() const noexcept -> bool
        {
            return running_;
        }

        // === 状态查询 ===

        /**
         * @brief 获取配置
         */
        [[nodiscard]] auto config() const noexcept -> const server_config&
        {
            return config_;
        }

        /**
         * @brief 获取活跃连接数
         */
        [[nodiscard]] auto active_connections() const noexcept -> std::size_t
        {
            return limiter_.active();
        }

        /**
         * @brief 获取总请求计数
         */
        [[nodiscard]] auto total_requests() const noexcept -> std::size_t
        {
            return requests_count_.load();
        }

        /**
         * @brief 获取路由器
         */
        [[nodiscard]] auto router_ref() noexcept -> wan::web::router&
        {
            return router_;
        }

        /**
         * @brief 获取执行器
         */
        [[nodiscard]] auto executor() noexcept -> std::shared_ptr<wan::web::executor>&
        {
            return executor_ptr_;
        }

        /**
         * @brief 获取 io_context
         */
        [[nodiscard]] auto io_context() noexcept -> net::io_context&
        {
            return executor_ptr_->context();
        }

        // === 动态配置 ===

        /**
         * @brief 动态设置最大连接数
         */
        void set_max_connections(std::size_t max)
        {
            config_.max_connections = max;
            limiter_.set_max_connections(max);
        }

        /**
         * @brief 动态启用/禁用 Keep-Alive
         */
        void set_keep_alive(bool enabled)
        {
            config_.keep_alive_enabled = enabled;
        }

    private:
        server_config config_;
        std::shared_ptr<wan::web::executor> executor_ptr_;
        wan::web::router router_;
        connection_limiter limiter_;
        std::shared_ptr<server_metrics> metrics_;
        std::vector<middleware> middlewares_;
        std::uint16_t port_{0};
        std::atomic<bool> running_{false};
        std::atomic<std::size_t> requests_count_{0};

        /**
         * @brief 设置内置路由
         */
        void setup_builtin_routes()
        {
            // 健康检查
            if (!config_.health_path.empty())
            {
                router_.get(config_.health_path, [this](context& ctx) -> net::awaitable<void>
                {
                    ctx.status(http::status::ok).json("{\"status\":\"healthy\",\"connections\":"
                        + std::to_string(limiter_.active())
                        + ",\"requests\":"
                        + std::to_string(requests_count_.load())
                        + "}");
                    co_return;
                });
            }

            // 指标导出
            if (config_.enable_metrics && !config_.metrics_path.empty())
            {
                router_.get(config_.metrics_path, [this](context& ctx) -> net::awaitable<void>
                {
                    ctx.status(http::status::ok).content_type("text/plain; version=0.0.4").text(metrics_->to_prometheus());
                    co_return;
                });
            }
        }

        /**
         * @brief 处理连接（支持 Keep-Alive）
         */
        auto handle_connection(connection_ptr conn) -> net::awaitable<void>
        {
            auto self = shared_from_this();
            std::size_t request_count = 0;

            while (conn->is_open() && running_)
            {
                // 读取请求
                std::error_code ec;
                auto req_opt = co_await conn->async_read(ec);

                if (ec || !req_opt || !running_)
                {
                    conn->close();
                    co_return;
                }

                auto& req = req_opt.value();
                ++request_count;
                ++self->requests_count_;
                metrics_->inc_requests();

                auto start_time = std::chrono::steady_clock::now();

                // 检查请求体大小
                if (req.raw().body().size() > config_.max_body_size)
                {
                    context ctx(std::move(req.raw()));
                    ctx.status(http::status::payload_too_large).text("Payload Too Large");
                    metrics_->inc_errors();
                    co_await conn->async_write_raw(ctx.raw_response(), ec);
                    conn->close();
                    co_return;
                }

                // 创建上下文
                context ctx(std::move(req.raw()));

                // 路由匹配
                auto match = router_.match(ctx.method(), ctx.target());

                if (!match)
                {
                    ctx.status(http::status::not_found).text("Not Found");
                    metrics_->inc_errors();
                }
                else
                {
                    // 设置路由参数
                    for (const auto& [name, value] : match->params)
                    {
                        ctx.set_route_param(name, value);
                    }

                    // 执行中间件链
                    if (!middlewares_.empty())
                    {
                        co_await run_middlewares(ctx, match->handler);
                    }
                    else
                    {
                        co_await match->handler(ctx);
                    }

                    metrics_->inc_success();
                }

                // 记录延迟
                auto latency = std::chrono::steady_clock::now() - start_time;
                metrics_->record_latency(latency);

                // 发送响应
                co_await conn->async_write_raw(ctx.raw_response(), ec);

                if (ec || !should_keep_alive(ctx, request_count))
                {
                    conn->close();
                    co_return;
                }

                // Keep-Alive: 继续处理下一个请求
            }
        }

        /**
         * @brief 判断是否保持连接
         */
        [[nodiscard]] auto should_keep_alive(context& ctx, std::size_t request_count) const noexcept -> bool
        {
            if (!config_.keep_alive_enabled)
            {
                return false;
            }

            if (request_count >= config_.max_keepalive_requests)
            {
                return false;
            }

            return ctx.raw_request().keep_alive();
        }

        /**
         * @brief 运行中间件链
         */
        auto run_middlewares(context& ctx, handler& final_handler) -> net::awaitable<void>
        {
            std::size_t index = 0;

            std::function<net::awaitable<void>()> next;

            next = [&, this]() -> net::awaitable<void>
            {
                if (index < middlewares_.size())
                {
                    auto& m = middlewares_.at(index++);
                    co_await m(ctx, next);
                }
                else
                {
                    co_await final_handler(ctx);
                }
            };

            co_await next();
        }
    };

    // === 工厂函数 ===

    /**
     * @brief 创建服务器
     */
    [[nodiscard]] inline auto make_server() -> std::shared_ptr<server>
    {
        return std::make_shared<server>();
    }

    /**
     * @brief 创建服务器（指定配置）
     */
    [[nodiscard]] inline auto make_server(const server_config& config) -> std::shared_ptr<server>
    {
        return std::make_shared<server>(config);
    }

    /**
     * @brief 创建服务器（指定线程数）
     */
    [[nodiscard]] inline auto make_server(std::size_t threads) -> std::shared_ptr<server>
    {
        server_config config;
        config.threads = threads;
        return std::make_shared<server>(config);
    }
}