/**
 * @file pool.hpp
 * @brief 连接池
 * @details TCP 连接池，支持 LIFO 缓存，使用 strand 实现异步序列化。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>

#include <wan/web/fault.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    /**
     * @struct pool_config
     * @brief 连接池配置
     */
    struct pool_config
    {
        std::size_t max_size = 32;              // 最大缓存数
        std::chrono::seconds max_idle{30};      // 最大空闲时间
        std::chrono::milliseconds timeout{300}; // 连接超时
        bool tcp_nodelay = true;                // TCP_NODELAY
        bool keep_alive = true;                 // SO_KEEPALIVE
    };

    /**
     * @class connection_pool
     * @brief TCP 连接池
     * @details 管理 TCP 连接复用，使用 strand 实现异步序列化，无阻塞锁。
     */
    class connection_pool : public std::enable_shared_from_this<connection_pool>
    {
    public:
        /**
         * @brief 构造连接池
         * @param strand strand 执行器，用于序列化所有池操作
         * @param config 配置参数
         */
        explicit connection_pool(net::strand<net::any_io_executor> strand, const pool_config& config = {})
            : strand_(strand)
            , config_(config)
        {
        }

        ~connection_pool()
        {
            clear();
        }

        connection_pool(const connection_pool&) = delete;
        connection_pool& operator=(const connection_pool&) = delete;

        /**
         * @brief 获取连接（纯协程）
         * @param endpoint 目标端点
         * @return 协程，返回错误码和可选 socket
         */
        auto async_acquire(tcp::endpoint endpoint) -> net::awaitable<std::pair<fault, std::optional<tcp::socket>>>
        {
            auto self = shared_from_this();

            // 通过 strand 序列化访问缓存
            co_await net::post(strand_, net::use_awaitable);

            auto& stack = cache_[endpoint];

            // LIFO: 从栈顶复用
            while (!stack.empty())
            {
                auto& item = stack.back();
                stack.pop_back();

                auto now = std::chrono::steady_clock::now();
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - item.last_used);

                // 过期检测
                if (age > config_.max_idle)
                {
                    boost::system::error_code ec;
                    item.socket.close(ec);
                    continue;
                }

                // 健康检测
                if (!item.socket.is_open())
                {
                    continue;
                }

                co_return std::make_pair(fault::success, std::move(item.socket));
            }

            // 缓存未命中，创建新连接（带超时）
            co_return co_await self->async_create_with_timeout(endpoint);
        }

        /**
         * @brief 异步归还连接
         */
        void async_recycle(tcp::socket socket, tcp::endpoint endpoint)
        {
            // 通过 strand 序列化归还
            net::post(strand_, [this, socket = std::move(socket), endpoint]() mutable
            {
                if (!socket.is_open())
                {
                    return;
                }

                auto& stack = cache_[endpoint];

                // 容量检查
                if (stack.size() >= config_.max_size)
                {
                    boost::system::error_code ec;
                    socket.close(ec);
                    return;
                }

                stack.push_back({std::move(socket), std::chrono::steady_clock::now()});
            });
        }

        /**
         * @brief 清理过期连接（纯协程）
         */
        auto async_cleanup() -> net::awaitable<void>
        {
            co_await net::post(strand_, net::use_awaitable);

            auto now = std::chrono::steady_clock::now();

            for (auto& [endpoint, stack] : cache_)
            {
                auto write_idx = std::size_t{0};
                for (std::size_t read_idx = 0; read_idx < stack.size(); ++read_idx)
                {
                    auto& item = stack[read_idx];
                    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - item.last_used);

                    if (age > config_.max_idle || !item.socket.is_open())
                    {
                        boost::system::error_code ec;
                        item.socket.close(ec);
                    }
                    else
                    {
                        if (write_idx != read_idx)
                        {
                            stack[write_idx] = std::move(item);
                        }
                        ++write_idx;
                    }
                }
                stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(write_idx), stack.end());
            }
        }

        /**
         * @brief 清空连接池（纯协程）
         */
        auto async_clear() -> net::awaitable<void>
        {
            co_await net::post(strand_, net::use_awaitable);

            for (auto& [endpoint, stack] : cache_)
            {
                for (auto& item : stack)
                {
                    if (item.socket.is_open())
                    {
                        boost::system::error_code ec;
                        item.socket.close(ec);
                    }
                }
            }
            cache_.clear();
        }

    private:
        struct idle_item
        {
            tcp::socket socket;
            std::chrono::steady_clock::time_point last_used;
        };

        /**
         * @brief 创建连接（带超时）
         */
        auto async_create_with_timeout(tcp::endpoint endpoint) -> net::awaitable<std::pair<fault, std::optional<tcp::socket>>>
        {
            auto executor = co_await net::this_coro::executor;

            // 设置 socket 选项（创建前）
            tcp::socket socket(executor);
            boost::system::error_code opt_ec;
            socket.set_option(tcp::no_delay(config_.tcp_nodelay), opt_ec);
            socket.set_option(net::socket_base::keep_alive(config_.keep_alive), opt_ec);

            // 异步连接
            boost::system::error_code connect_ec;
            co_await socket.async_connect(endpoint, net::redirect_error(net::use_awaitable, connect_ec));

            if (connect_ec)
            {
                boost::system::error_code ec;
                socket.close(ec);
                if (connect_ec == net::error::operation_aborted)
                {
                    co_return std::make_pair(fault::timeout, std::nullopt);
                }
                co_return std::make_pair(fault::connection_refused, std::nullopt);
            }

            co_return std::make_pair(fault::success, std::move(socket));
        }

        /**
         * @brief 同步清空（析构用）
         */
        void clear()
        {
            for (auto& [endpoint, stack] : cache_)
            {
                for (auto& item : stack)
                {
                    if (item.socket.is_open())
                    {
                        boost::system::error_code ec;
                        item.socket.close(ec);
                    }
                }
            }
            cache_.clear();
        }

        net::strand<net::any_io_executor> strand_;
        pool_config config_;
        std::unordered_map<tcp::endpoint, std::deque<idle_item>> cache_;
    };

    /**
     * @brief 连接池指针
     */
    using connection_pool_ptr = std::shared_ptr<connection_pool>;

    /**
     * @brief 创建连接池
     * @param strand strand 执行器
     * @param config 配置参数
     */
    [[nodiscard]] inline auto make_connection_pool(net::strand<net::any_io_executor> strand, const pool_config& config = {})
        -> connection_pool_ptr
    {
        return std::make_shared<connection_pool>(strand, config);
    }
}