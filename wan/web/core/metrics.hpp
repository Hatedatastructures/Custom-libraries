/**
 * @file metrics.hpp
 * @brief 监控指标收集器
 * @details 高性能指标收集，支持 Prometheus 格式导出。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <sstream>

namespace wan::web
{
    /**
     * @struct latency_histogram
     * @brief 延迟直方图
     * @details 分桶统计请求延迟分布。
     */
    struct latency_histogram
    {
        // 延迟桶边界（毫秒）: 0-1, 1-10, 10-50, 50-100, 100-500, 500-1000, 1000-5000, 5000+
        static constexpr std::array<std::uint64_t, 8> boundaries = {1, 10, 50, 100, 500, 1000, 5000, UINT64_MAX};
        std::array<std::atomic<std::uint64_t>, 8> buckets{};

        /**
         * @brief 记录延迟
         * @param latency_ms 延迟（毫秒）
         */
        void record(std::uint64_t latency_ms)
        {
            for (std::size_t i = 0; i < boundaries.size(); ++i)
            {
                if (latency_ms <= boundaries[i])
                {
                    buckets[i].fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
            buckets[boundaries.size() - 1].fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 获取桶值
         */
        [[nodiscard]] auto get(std::size_t index) const noexcept -> std::uint64_t
        {
            return buckets[index].load(std::memory_order_relaxed);
        }

        /**
         * @brief 计算百分位数（近似）
         * @param percentile 百分位（如 50, 90, 99）
         */
        [[nodiscard]] auto percentile(std::uint64_t percentile) const noexcept -> std::uint64_t
        {
            std::uint64_t total = 0;
            for (std::size_t i = 0; i < buckets.size(); ++i)
            {
                total += buckets[i].load(std::memory_order_relaxed);
            }

            if (total == 0)
            {
                return 0;
            }

            std::uint64_t target = total * percentile / 100;
            std::uint64_t cumulative = 0;

            for (std::size_t i = 0; i < boundaries.size(); ++i)
            {
                cumulative += buckets[i].load(std::memory_order_relaxed);
                if (cumulative >= target)
                {
                    return boundaries[i];
                }
            }

            return boundaries[boundaries.size() - 1];
        }
    };

    /**
     * @class server_metrics
     * @brief 服务器指标收集器
     * @details 线程安全的指标收集，支持 Prometheus 导出。
     */
    class server_metrics
    {
    public:
        server_metrics() = default;

        // === 计数器 ===

        /**
         * @brief 增加请求计数
         */
        void inc_requests()
        {
            requests_total_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 增加错误计数
         */
        void inc_errors()
        {
            errors_total_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 增加成功计数
         */
        void inc_success()
        {
            success_total_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 增加超时计数
         */
        void inc_timeout()
        {
            timeouts_total_.fetch_add(1, std::memory_order_relaxed);
        }

        // === WebSocket 指标 ===

        /**
         * @brief 增加 WebSocket 连接数
         */
        void inc_websocket_connections()
        {
            websocket_connections_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 减少 WebSocket 连接数
         */
        void dec_websocket_connections()
        {
            websocket_connections_.fetch_sub(1, std::memory_order_relaxed);
        }

        /**
         * @brief 增加 WebSocket 消息计数
         */
        void inc_websocket_messages()
        {
            websocket_messages_total_.fetch_add(1, std::memory_order_relaxed);
        }

        // === 关闭指标 ===

        /**
         * @brief 记录关闭开始
         */
        void record_shutdown_start()
        {
            shutdown_started_.store(1, std::memory_order_relaxed);
        }

        /**
         * @brief 记录关闭完成
         */
        void record_shutdown_complete()
        {
            shutdown_completed_.store(1, std::memory_order_relaxed);
        }

        // === Gauge ===

        /**
         * @brief 设置活跃连接数
         */
        void set_active_connections(std::uint64_t value)
        {
            active_connections_.store(value, std::memory_order_relaxed);
        }

        /**
         * @brief 增加活跃连接
         */
        void inc_active_connections()
        {
            active_connections_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 减少活跃连接
         */
        void dec_active_connections()
        {
            active_connections_.fetch_sub(1, std::memory_order_relaxed);
        }

        // === 直方图 ===

        /**
         * @brief 记录请求延迟
         * @param latency 延迟时间
         */
        void record_latency(std::chrono::steady_clock::duration latency)
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(latency).count();
            latency_histogram_.record(static_cast<std::uint64_t>(ms));
        }

        // === 读取 ===

        [[nodiscard]] auto requests_total() const noexcept -> std::uint64_t
        {
            return requests_total_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto errors_total() const noexcept -> std::uint64_t
        {
            return errors_total_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto success_total() const noexcept -> std::uint64_t
        {
            return success_total_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto timeouts_total() const noexcept -> std::uint64_t
        {
            return timeouts_total_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto active_connections() const noexcept -> std::uint64_t
        {
            return active_connections_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto latency_p50() const noexcept -> std::uint64_t
        {
            return latency_histogram_.percentile(50);
        }

        [[nodiscard]] auto latency_p90() const noexcept -> std::uint64_t
        {
            return latency_histogram_.percentile(90);
        }

        [[nodiscard]] auto latency_p99() const noexcept -> std::uint64_t
        {
            return latency_histogram_.percentile(99);
        }

        [[nodiscard]] auto websocket_connections() const noexcept -> std::uint64_t
        {
            return websocket_connections_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto websocket_messages_total() const noexcept -> std::uint64_t
        {
            return websocket_messages_total_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] auto is_shutdown_started() const noexcept -> bool
        {
            return shutdown_started_.load(std::memory_order_relaxed) != 0;
        }

        [[nodiscard]] auto is_shutdown_completed() const noexcept -> bool
        {
            return shutdown_completed_.load(std::memory_order_relaxed) != 0;
        }

        // === 导出 ===

        /**
         * @brief 导出 Prometheus 格式
         */
        [[nodiscard]] auto to_prometheus() const -> std::string
        {
            std::ostringstream oss;

            // 计数器
            oss << "# HELP wan_web_requests_total Total number of requests\n";
            oss << "# TYPE wan_web_requests_total counter\n";
            oss << "wan_web_requests_total " << requests_total() << "\n\n";

            oss << "# HELP wan_web_errors_total Total number of errors\n";
            oss << "# TYPE wan_web_errors_total counter\n";
            oss << "wan_web_errors_total " << errors_total() << "\n\n";

            oss << "# HELP wan_web_success_total Total number of successful requests\n";
            oss << "# TYPE wan_web_success_total counter\n";
            oss << "wan_web_success_total " << success_total() << "\n\n";

            oss << "# HELP wan_web_timeouts_total Total number of timeouts\n";
            oss << "# TYPE wan_web_timeouts_total counter\n";
            oss << "wan_web_timeouts_total " << timeouts_total() << "\n\n";

            // Gauge
            oss << "# HELP wan_web_active_connections Number of active connections\n";
            oss << "# TYPE wan_web_active_connections gauge\n";
            oss << "wan_web_active_connections " << active_connections() << "\n\n";

            // 延迟直方图
            oss << "# HELP wan_web_request_latency_ms Request latency in milliseconds\n";
            oss << "# TYPE wan_web_request_latency_ms histogram\n";

            std::uint64_t cumulative = 0;
            for (std::size_t i = 0; i < latency_histogram::boundaries.size(); ++i)
            {
                auto count = latency_histogram_.get(i);
                cumulative += count;
                oss << "wan_web_request_latency_ms_bucket{le=\"" << latency_histogram::boundaries[i] << "\"} "
                    << cumulative << "\n";
            }
            oss << "wan_web_request_latency_ms_sum " << requests_total() << "\n";
            oss << "wan_web_request_latency_ms_count " << requests_total() << "\n\n";

            // WebSocket 指标
            oss << "# HELP wan_web_websocket_connections Number of active WebSocket connections\n";
            oss << "# TYPE wan_web_websocket_connections gauge\n";
            oss << "wan_web_websocket_connections " << websocket_connections() << "\n\n";

            oss << "# HELP wan_web_websocket_messages_total Total WebSocket messages\n";
            oss << "# TYPE wan_web_websocket_messages_total counter\n";
            oss << "wan_web_websocket_messages_total " << websocket_messages_total() << "\n";

            return oss.str();
        }

        /**
         * @brief 重置所有指标
         */
        void reset()
        {
            requests_total_.store(0, std::memory_order_relaxed);
            errors_total_.store(0, std::memory_order_relaxed);
            success_total_.store(0, std::memory_order_relaxed);
            timeouts_total_.store(0, std::memory_order_relaxed);
            active_connections_.store(0, std::memory_order_relaxed);

            for (std::size_t i = 0; i < latency_histogram::boundaries.size(); ++i)
            {
                latency_histogram_.buckets[i].store(0, std::memory_order_relaxed);
            }
        }

    private:
        std::atomic<std::uint64_t> requests_total_{0};
        std::atomic<std::uint64_t> errors_total_{0};
        std::atomic<std::uint64_t> success_total_{0};
        std::atomic<std::uint64_t> timeouts_total_{0};
        std::atomic<std::uint64_t> active_connections_{0};
        std::atomic<std::uint64_t> websocket_connections_{0};
        std::atomic<std::uint64_t> websocket_messages_total_{0};
        std::atomic<std::uint64_t> shutdown_started_{0};
        std::atomic<std::uint64_t> shutdown_completed_{0};
        latency_histogram latency_histogram_;
    };

    /**
     * @brief 创建指标收集器
     */
    [[nodiscard]] inline auto make_metrics() -> std::shared_ptr<server_metrics>
    {
        return std::make_shared<server_metrics>();
    }
}