/**
 * @file limits.hpp
 * @brief 连接限制管理
 * @details 异步连接限制器，无锁设计，使用 strand 序列化。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <atomic>
#include <deque>
#include <memory>
#include <functional>

namespace wan::web
{
    namespace net = boost::asio;

    /**
     * @class connection_limiter
     * @brief 连接限制器
     * @details 异步连接限制，使用 strand 序列化，无阻塞锁。
     */
    class connection_limiter : public std::enable_shared_from_this<connection_limiter>
    {
    public:
        using wait_callback = std::function<void(bool)>;

        /**
         * @brief 构造函数
         * @param strand strand 执行器
         * @param max_connections 最大连接数
         */
        explicit connection_limiter(net::strand<net::any_io_executor> strand, std::size_t max_connections)
            : strand_(strand)
            , max_connections_(max_connections)
            , active_(0)
        {
        }

        /**
         * @brief 获取连接许可（纯协程）
         * @return 协程，返回是否成功获取
         */
        auto async_acquire() -> net::awaitable<bool>
        {
            auto self = shared_from_this();

            // 通过 strand 序列化
            co_await net::post(strand_, net::use_awaitable);

            if (active_ < max_connections_)
            {
                ++active_;
                co_return true;
            }

            // 连接数已满，返回失败（不排队等待）
            co_return false;
        }

        /**
         * @brief 释放连接许可
         */
        void release()
        {
            // 通过 strand 序列化
            net::post(strand_, [this]()
            {
                if (active_ > 0)
                {
                    --active_;
                }
            });
        }

        /**
         * @brief 获取当前活跃连接数
         */
        [[nodiscard]] auto active() const noexcept -> std::size_t
        {
            return active_.load();
        }

        /**
         * @brief 获取最大连接数
         */
        [[nodiscard]] auto max_connections() const noexcept -> std::size_t
        {
            return max_connections_;
        }

        /**
         * @brief 设置最大连接数
         */
        void set_max_connections(std::size_t max)
        {
            max_connections_ = max;
        }

        /**
         * @brief 重置（清空活跃连接）
         */
        void reset()
        {
            active_.store(0, std::memory_order_relaxed);
        }

    private:
        net::strand<net::any_io_executor> strand_;
        std::size_t max_connections_;
        std::atomic<std::size_t> active_;
    };

    /**
     * @brief 连接限制器指针
     */
    using connection_limiter_ptr = std::shared_ptr<connection_limiter>;

    /**
     * @brief 创建连接限制器
     */
    [[nodiscard]] inline auto make_connection_limiter(net::strand<net::any_io_executor> strand, std::size_t max_connections)
        -> connection_limiter_ptr
    {
        return std::make_shared<connection_limiter>(strand, max_connections);
    }
}