/**
 * @file Metrics.hpp
 * @brief 以原子计数器收集服务器指标并导出 Prometheus 文本。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <atomic>
#include <cstdio>
#include <string>

namespace Wan::Web
{
    /**
     * @class ServerMetrics
     * @brief 服务端指标
     */
    class ServerMetrics
    {
    public:
        /** @brief 将请求总数加一。 */
        void IncRequests() noexcept
        {
            MemberTotalRequests.fetch_add(1, std::memory_order_relaxed);
        }

        /** @brief 将活动连接数加一。 */
        void IncActiveConnections() noexcept
        {
            MemberActiveConnections.fetch_add(1, std::memory_order_relaxed);
        }

        /** @brief 将活动连接数减一。 */
        void DecActiveConnections() noexcept
        {
            MemberActiveConnections.fetch_sub(1, std::memory_order_relaxed);
        }

        /** @brief 将活动连接数重置为零。 */
        void ResetActiveConnections() noexcept
        {
            MemberActiveConnections.store(0, std::memory_order_relaxed);
        }

        /** @brief 将错误总数加一。 */
        void IncErrors() noexcept
        {
            MemberTotalErrors.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 累加读取字节数。
         * @param N 本次读取的字节数。
         */
        void IncBytesRead(std::size_t N) noexcept
        {
            MemberBytesRead.fetch_add(N, std::memory_order_relaxed);
        }

        /**
         * @brief 累加写入字节数。
         * @param N 本次写入的字节数。
         */
        void IncBytesWritten(std::size_t N) noexcept
        {
            MemberBytesWritten.fetch_add(N, std::memory_order_relaxed);
        }

        /**
         * @brief 导出当前指标。
         * @return Prometheus 文本格式；指标名保持 `wan_*`。
         */
        [[nodiscard]] auto ToPrometheus() const -> std::string
        {
            return "# HELP wan_requests_total Total requests\n"
                   "# TYPE wan_requests_total counter\n"
                   "wan_requests_total " +
                std::to_string(MemberTotalRequests.load(
                    std::memory_order_relaxed)) +
                "\n"
                "# HELP wan_active_connections Active connections\n"
                "# TYPE wan_active_connections gauge\n"
                "wan_active_connections " +
                std::to_string(MemberActiveConnections.load(
                    std::memory_order_relaxed)) +
                "\n"
                "# HELP wan_errors_total Total errors\n"
                "# TYPE wan_errors_total counter\n"
                "wan_errors_total " +
                std::to_string(
                    MemberTotalErrors.load(std::memory_order_relaxed)) +
                "\n"
                "# HELP wan_bytes_read_total Bytes read\n"
                "# TYPE wan_bytes_read_total counter\n"
                "wan_bytes_read_total " +
                std::to_string(
                    MemberBytesRead.load(std::memory_order_relaxed)) +
                "\n"
                "# HELP wan_bytes_written_total Bytes written\n"
                "# TYPE wan_bytes_written_total counter\n"
                "wan_bytes_written_total " +
                std::to_string(
                    MemberBytesWritten.load(std::memory_order_relaxed)) +
                "\n";
        }

    private:
        /** @brief 已处理请求的累计数。 */
        std::atomic<std::uint64_t> MemberTotalRequests{0};

        /** @brief 当前活动连接数。 */
        std::atomic<std::uint64_t> MemberActiveConnections{0};

        /** @brief 已记录错误的累计数。 */
        std::atomic<std::uint64_t> MemberTotalErrors{0};

        /** @brief 已读取字节数的累计值。 */
        std::atomic<std::uint64_t> MemberBytesRead{0};

        /** @brief 已写入字节数的累计值。 */
        std::atomic<std::uint64_t> MemberBytesWritten{0};
    };
}
