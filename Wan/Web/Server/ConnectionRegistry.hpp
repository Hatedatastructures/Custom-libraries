/**
 * @file ConnectionRegistry.hpp
 * @brief 管理活动 Web 连接的线程安全注册表。
 */
#pragma once

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/beast.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Wan::Web
{
    namespace Beast = boost::beast;

    /**
     * @class ConnectionRegistry
     * @brief 跟踪活动 TCP 流，并在流关联的执行器上取消和关闭连接。
     * @details `CloseAll()` 会先同步移除全部条目，再向各流的执行器派发关闭操作。
     */
    class ConnectionRegistry
    {
    public:
        /** @brief 已注册的 TCP 流类型。 */
        using Stream = std::shared_ptr<Beast::tcp_stream>;
        /** @brief 注册表条目标识类型。 */
        using Id = std::uint64_t;
        /** @brief 连接取消信号类型。 */
        using Cancellation = std::shared_ptr<
            boost::asio::cancellation_signal>;
        /** @brief 表示连接处理器是否正在等待的共享状态类型。 */
        using HandlerActive = std::shared_ptr<std::atomic_bool>;

    private:
        /** @brief 注册表中单个活动连接的资源和状态。 */
        struct Entry
        {
            /** @brief 被跟踪的 TCP 流。 */
            Stream Connection;
            /** @brief 取消该连接处理器的信号。 */
            Cancellation CancelSignal;
            /** @brief 指示处理器是否仍在等待的共享状态。 */
            HandlerActive IsHandlerActive;
        };

    public:

        /**
         * @brief 将 TCP 流加入活动连接注册表。
         * @param Connection 要跟踪的非空 TCP 流。
         * @param CancelSignal 用于取消活动处理器的信号；为空时会自动创建。
         * @param IsHandlerActive 处理器等待状态；为空时会自动创建。
         * @return 新条目的唯一标识。
         * @throws std::invalid_argument `Connection` 为空时抛出。
         * @throws std::overflow_error 标识空间耗尽时抛出。
         */
        [[nodiscard]] auto Register(Stream Connection,
            Cancellation CancelSignal = {}, HandlerActive IsHandlerActive = {})
            -> Id
        {
            if (!Connection)
            {
                throw std::invalid_argument(
                    "connection registry cannot register a null stream");
            }
            if (!CancelSignal)
            {
                CancelSignal = std::make_shared<
                    boost::asio::cancellation_signal>();
            }
            if (!IsHandlerActive)
            {
                IsHandlerActive = std::make_shared<std::atomic_bool>(false);
            }

            std::lock_guard<std::mutex> Lock{MemberMutex};
            if (MemberNextId == 0)
            {
                throw std::overflow_error(
                    "connection registry id space exhausted");
            }
            const auto IdValue = MemberNextId++;
            MemberConnections.emplace(IdValue,
                Entry{std::move(Connection), std::move(CancelSignal),
                    std::move(IsHandlerActive)});
            return IdValue;
        }

        /**
         * @brief 移除连接条目，但不关闭对应的流。
         * @param IdValue `Register()` 返回的标识。
         */
        void Remove(Id IdValue) noexcept
        {
            std::lock_guard<std::mutex> Lock{MemberMutex};
            MemberConnections.erase(IdValue);
        }

        /**
         * @brief 返回当前注册的流数量。
         * @return 注册表中的条目数。
         */
        [[nodiscard]] auto Size() const noexcept -> std::size_t
        {
            std::lock_guard<std::mutex> Lock{MemberMutex};
            return MemberConnections.size();
        }

        /**
         * @brief 移除全部条目，并派发取消和关闭操作。
         * @details Socket 操作在各流关联的执行器上执行。
         */
        void CloseAll()
        {
            std::unordered_map<Id, Entry> Connections;
            {
                std::lock_guard<std::mutex> Lock{MemberMutex};
                Connections.swap(MemberConnections);
            }

            for (auto& [IdValue, ConnectionData] : Connections)
            {
                (void)IdValue;
                auto Connection = std::move(ConnectionData.Connection);
                auto CancelSignal = std::move(ConnectionData.CancelSignal);
                auto IsHandlerActive = std::move(
                    ConnectionData.IsHandlerActive);
                if (!Connection)
                {
                    continue;
                }
                auto ConnectionExecutor = Connection->get_executor();
                auto CloseConnection =
                    [Connection = std::move(Connection),
                        CancelSignal = std::move(CancelSignal),
                        IsHandlerActive = std::move(IsHandlerActive)]() mutable
                    {
                        if (CancelSignal && IsHandlerActive
                            && IsHandlerActive->load(
                                std::memory_order_acquire))
                        {
                            CancelSignal->emit(
                                boost::asio::cancellation_type::all);
                        }
                        boost::system::error_code Ec;
                        Connection->socket().cancel(Ec);
                        Connection->socket().close(Ec);
                    };
                boost::asio::dispatch(ConnectionExecutor,
                    std::move(CloseConnection));
            }
        }

        /**
         * @brief 工作线程停止后，同步移除并关闭全部连接。
         * @note 仅可在相关工作线程全部 Join 后调用。
         */
        void CloseAllNow() noexcept
        {
            std::unordered_map<Id, Entry> Connections;
            {
                std::lock_guard<std::mutex> Lock{MemberMutex};
                Connections.swap(MemberConnections);
            }

            for (auto& [IdValue, ConnectionData] : Connections)
            {
                (void)IdValue;
                if (ConnectionData.Connection
                    && ConnectionData.CancelSignal
                    && ConnectionData.IsHandlerActive
                    && ConnectionData.IsHandlerActive->load(
                        std::memory_order_acquire))
                {
                    ConnectionData.CancelSignal->emit(
                        boost::asio::cancellation_type::all);
                }
                if (ConnectionData.Connection)
                {
                    boost::system::error_code Ec;
                    ConnectionData.Connection->socket().cancel(Ec);
                    ConnectionData.Connection->socket().close(Ec);
                }
            }
        }

    private:
        /** @brief 保护活动连接表和下一个 ID 的互斥量。 */
        mutable std::mutex MemberMutex;
        /** @brief 按注册 ID 索引的活动连接。 */
        std::unordered_map<Id, Entry> MemberConnections;
        /** @brief 下次注册时分配的 ID；零保留为无效值。 */
        Id MemberNextId = 1;
    };
}
