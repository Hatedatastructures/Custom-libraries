/**
 * @file Reliable.hpp
 * @brief 可靠流式传输实现（TCP）
 * @details 封装 TCP socket，并实现协程和完成回调两种异步读写接口。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include "Transmission.hpp"

#include <boost/asio.hpp>
#include <boost/asio/any_completion_handler.hpp>

#include <memory>
#include <optional>
#include <stdexcept>

#include "../Fault/Compatible.hpp"
#include "../Fault/Handling.hpp"

namespace Wan::Transport
{
    /**
     * @class Reliable
     * @brief 可靠流式传输实现（TCP）
     * @details TCP socket 的所有权由对象持有；`NextLayer()` 始终返回空指针。
     */
    class Reliable : public Transmission
    {
    public:
        /** @brief 底层 TCP socket 类型。 */
        using SocketType = Net::ip::tcp::socket;

        /**
         * @brief 使用执行器创建未打开的 TCP socket。
         * @param Executor 用于创建 socket 的执行器。
         */
        explicit Reliable(Net::any_io_executor Executor)
            : MemberSocket(Executor)
        {
        }

        /**
         * @brief 接管现有 TCP socket。
         * @param Socket 要接管的 socket。
         */
        explicit Reliable(SocketType Socket)
            : MemberSocket(std::move(Socket))
        {
        }

        /**
         * @brief 返回 TCP 传输类型。
         * @return `Type::Tcp`。
         */
        [[nodiscard]] auto TransportType() const noexcept
            -> Type override
        {
            return Type::Tcp;
        }

        /**
         * @brief 获取 socket 的执行器。
         * @return socket 关联的执行器。
         * @throws std::logic_error 对象已释放 socket 时抛出。
         */
        [[nodiscard]] auto Executor() const
            -> ExecutorType override
        {
            if (!MemberSocket)
            {
                throw std::logic_error("Reliable has no socket");
            }
            return const_cast<SocketType&>(NativeSocket()).get_executor();
        }

        /**
         * @brief 取得下层传输。
         * @return 空指针；`Reliable` 是传输装饰器链的叶节点。
         */
        [[nodiscard]] auto NextLayer() noexcept
            -> Transmission* override
        {
            return nullptr;
        }

        /**
         * @brief 以只读方式取得下层传输。
         * @return 空指针；`Reliable` 是传输装饰器链的叶节点。
         */
        [[nodiscard]] auto NextLayer() const noexcept
            -> const Transmission* override
        {
            return nullptr;
        }

        // ── awaitable 路径 ──

        /**
         * @brief 协程方式读取部分数据。
         * @param Buffer 接收缓冲区。
         * @param Ec 接收错误码。
         * @return 读取的字节数，可能少于缓冲区长度。
         */
        [[nodiscard]] auto AsyncReadSome(
            std::span<std::byte> Buffer, std::error_code& Ec)
            -> Net::awaitable<std::size_t> override
        {
            if (!MemberSocket)
            {
                Ec = Wan::Fault::make_error_code(
                    Wan::Fault::Code::InvalidArgument);
                co_return 0;
            }
            boost::system::error_code SysEc;
            auto token = Net::redirect_error(Net::use_awaitable, SysEc);
            const auto n = co_await NativeSocket().async_read_some(
                Net::buffer(Buffer.data(), Buffer.size()), token);
            Ec = Wan::Fault::make_error_code(Wan::Fault::ToCode(SysEc));
            co_return n;
        }

        /**
         * @brief 协程方式写入部分数据。
         * @param Buffer 待发送数据。
         * @param Ec 接收错误码。
         * @return 写入的字节数，可能少于缓冲区长度。
         */
        [[nodiscard]] auto AsyncWriteSome(
            std::span<const std::byte> Buffer, std::error_code& Ec)
            -> Net::awaitable<std::size_t> override
        {
            if (!MemberSocket)
            {
                Ec = Wan::Fault::make_error_code(
                    Wan::Fault::Code::InvalidArgument);
                co_return 0;
            }
            boost::system::error_code SysEc;
            auto token = Net::redirect_error(Net::use_awaitable, SysEc);
            const auto n = co_await NativeSocket().async_write_some(
                Net::buffer(Buffer.data(), Buffer.size()), token);
            Ec = Wan::Fault::make_error_code(Wan::Fault::ToCode(SysEc));
            co_return n;
        }

        // ── completion-Handler 路径（零协程） ──

        /**
         * @brief 使用完成回调读取部分数据。
         * @param Buffer 接收缓冲区。
         * @param Handler 操作完成时调用的处理器。
         * @note 缓冲区须保持有效，直到处理器被调用。
         */
        void AsyncReadSome(std::span<std::byte> Buffer,
            Net::any_completion_handler<void(
                boost::system::error_code, std::size_t)> Handler) override
        {
            if (!MemberSocket)
            {
                Handler(boost::asio::error::operation_aborted, 0);
                return;
            }
            NativeSocket().async_read_some(
                Net::buffer(Buffer.data(), Buffer.size()),
                std::move(Handler));
        }

        /**
         * @brief 使用完成回调写入部分数据。
         * @param Buffer 待发送数据。
         * @param Handler 操作完成时调用的处理器。
         * @note 缓冲区须保持有效，直到处理器被调用。
         */
        void AsyncWriteSome(std::span<const std::byte> Buffer,
            Net::any_completion_handler<void(
                boost::system::error_code, std::size_t)> Handler) override
        {
            if (!MemberSocket)
            {
                Handler(boost::asio::error::operation_aborted, 0);
                return;
            }
            NativeSocket().async_write_some(
                Net::buffer(Buffer.data(), Buffer.size()),
                std::move(Handler));
        }

        // ── 生命周期 ──

        /** @brief 关闭底层 socket。 */
        void Close() override
        {
            if (MemberSocket)
            {
                boost::system::error_code ec;
                MemberSocket->close(ec);
            }
        }

        /** @brief 取消底层 socket 上未完成的异步操作。 */
        void Cancel() override
        {
            if (!MemberSocket)
            {
                return;
            }
            boost::system::error_code ec;
            NativeSocket().cancel(ec);
        }

        /**
         * @brief 半关闭 TCP 写方向；读方向仍可继续。
         */
        void ShutdownWrite()
        {
            if (!MemberSocket)
            {
                return;
            }
            boost::system::error_code ec;
            NativeSocket().shutdown(SocketType::shutdown_send, ec);
        }

        // ── 原生访问 ──

        /**
         * @brief 获取底层 socket。
         * @return socket 的可写引用。
         * @throws std::logic_error 对象已释放 socket 时抛出。
         */
        [[nodiscard]] auto NativeSocket() -> SocketType&
        {
            if (!MemberSocket)
            {
                throw std::logic_error("Reliable has no socket");
            }
            return *MemberSocket;
        }

        /**
         * @brief 以只读方式获取底层 socket。
         * @return socket 的只读引用。
         * @throws std::logic_error 对象已释放 socket 时抛出。
         */
        [[nodiscard]] auto NativeSocket() const
            -> const SocketType&
        {
            if (!MemberSocket)
            {
                throw std::logic_error("Reliable has no socket");
            }
            return *MemberSocket;
        }

        /**
         * @brief 移出底层 socket 的所有权。
         * @return 移出的 socket；无 socket 时返回空值。
         * @warning 调用后此对象不再可用于传输。
         */
        [[nodiscard]] auto ReleaseSocket() noexcept
            -> std::optional<SocketType>
        {
            if (MemberSocket)
            {
                auto s = std::move(*MemberSocket);
                MemberSocket.reset();
                return s;
            }
            return std::nullopt;
        }

    private:
        /** @brief 当前接管的 TCP socket；未持有或已释放时为空。 */
        std::optional<SocketType> MemberSocket;
    };

    // ── 工厂函数 ──

    /**
     * @brief 创建共享的 TCP 传输对象。
     * @param Executor socket 使用的执行器。
     * @return 新建的 `Reliable` 对象。
     */
    [[nodiscard]] inline auto MakeReliable(
        const Net::any_io_executor& Executor) -> SharedTransmission
    {
        return std::make_shared<Reliable>(Executor);
    }

    /**
     * @brief 从现有 socket 创建共享的 TCP 传输对象。
     * @param Socket 要接管的 TCP socket。
     * @return 新建的 `Reliable` 对象。
     */
    [[nodiscard]] inline auto MakeReliable(Net::ip::tcp::socket Socket)
        -> SharedTransmission
    {
        return std::make_shared<Reliable>(std::move(Socket));
    }
}
