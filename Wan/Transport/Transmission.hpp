/**
 * @file Transmission.hpp
 * @brief 传输层流式抽象接口
 * @details 派生类实现部分读写和执行器访问；完整读写由自由函数提供。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <system_error>

#include "../Fault/Compatible.hpp"

namespace Wan::Transport
{
    namespace Net = boost::asio;

    /**
     * @class Transmission
     * @brief 传输层流式抽象接口
     * @details 派生类提供部分读写、执行器访问、关闭和取消；装饰器通过 `NextLayer()` 导航。
     */
    class Transmission : public std::enable_shared_from_this<Transmission>
    {
    public:
        /** @brief 与传输层关联的执行器类型。 */
        using ExecutorType = Net::any_io_executor;
        /** @brief 完成回调的签名类型。 */
        using CompletionHandler = Net::any_completion_handler<void(
            boost::system::error_code, std::size_t)>;

    private:
        /** @brief 线程安全地保存并至多调用一次异步完成处理器。 */
        class CompletionState
        {
        public:
            /**
             * @brief 保存完成处理器。
             * @param HandlerValue 要调用的完成处理器。
             */
            explicit CompletionState(CompletionHandler HandlerValue)
                : MemberHandler(std::move(HandlerValue))
            {
            }

            /**
             * @brief 取出并调用完成处理器。
             * @param Ec 操作错误码。
             * @param Bytes 本次操作传输的字节数。
             */
            void Complete(boost::system::error_code Ec, std::size_t Bytes)
            {
                std::optional<CompletionHandler> Handler;
                {
                    std::lock_guard<std::mutex> Lock{MemberMutex};
                    if (!MemberHandler)
                    {
                        return;
                    }
                    Handler.emplace(std::move(*MemberHandler));
                    MemberHandler.reset();
                }
                std::move(*Handler)(Ec, Bytes);
            }

        private:
            /** @brief 保护完成状态和处理器的互斥量。 */
            std::mutex MemberMutex;
            /** @brief 尚未调用的完成处理器；完成后清空。 */
            std::optional<CompletionHandler> MemberHandler;
        };

        /**
         * @brief 将标准错误码转换为 Boost.System 错误码。
         * @param Ec 要转换的标准错误码。
         * @return 对应的 Boost.System 错误码；本库错误类别会映射到 `Wan::Fault` 类别。
         */
        [[nodiscard]] static auto ToBoostError(
            const std::error_code& Ec) -> boost::system::error_code
        {
            if (!Ec)
            {
                return {};
            }
            if (Ec.category() == Wan::Fault::Category())
            {
                return {Ec.value(), boost::system::Category()};
            }
            return {Ec.value(), boost::system::generic_category()};
        }

        /**
         * @brief 将协程式部分读取桥接到一次性完成处理器。
         * @param Self 保持传输对象存活的共享指针。
         * @param Buffer 接收读取数据的缓冲区。
         * @param State 保存完成处理器的共享状态。
         * @return 读取完成时完成处理器被调用的 awaitable。
         * @note 读取协程抛出异常时以 `operation_aborted` 完成。
         */
        [[nodiscard]] static auto BridgeRead(
            std::shared_ptr<Transmission> Self,
            std::span<std::byte> Buffer,
            std::shared_ptr<CompletionState> State)
            -> Net::awaitable<void>
        {
            boost::system::error_code ResultEc;
            std::size_t Bytes = 0;
            try
            {
                std::error_code Ec;
                Bytes = co_await Self->AsyncReadSome(Buffer, Ec);
                ResultEc = ToBoostError(Ec);
            }
            catch (...)
            {
                ResultEc = Net::error::operation_aborted;
            }
            State->Complete(ResultEc, Bytes);
        }

        /**
         * @brief 将协程式部分写入桥接到一次性完成处理器。
         * @param Self 保持传输对象存活的共享指针。
         * @param Buffer 要写出的只读缓冲区。
         * @param State 保存完成处理器的共享状态。
         * @return 写入完成时完成处理器被调用的 awaitable。
         * @note 写入协程抛出异常时以 `operation_aborted` 完成。
         */
        [[nodiscard]] static auto BridgeWrite(
            std::shared_ptr<Transmission> Self,
            std::span<const std::byte> Buffer,
            std::shared_ptr<CompletionState> State)
            -> Net::awaitable<void>
        {
            boost::system::error_code ResultEc;
            std::size_t Bytes = 0;
            try
            {
                std::error_code Ec;
                Bytes = co_await Self->AsyncWriteSome(Buffer, Ec);
                ResultEc = ToBoostError(Ec);
            }
            catch (...)
            {
                ResultEc = Net::error::operation_aborted;
            }
            State->Complete(ResultEc, Bytes);
        }

    public:

        /**
         * @brief 传输层类型标识。
         */
        enum class Type : std::uint8_t
        {
            Tcp, ///< 可靠流式传输（TCP）
            Udp  ///< 不可靠数据报传输（UDP）
        };

        virtual ~Transmission() = default;

        /**
         * @brief 获取传输层类型。
         * @return 最底层传输层的类型。
         */
        [[nodiscard]] virtual auto TransportType() const noexcept -> Type
        {
            auto* n = NextLayer();
            if (n)
            {
                return n->TransportType();
            }
            return Type::Tcp;
        }

        /**
         * @brief 获取关联的执行器。
         * @return 传输层使用的执行器。
         */
        [[nodiscard]] virtual auto Executor() const -> ExecutorType = 0;

        /**
         * @brief 获取关联的执行器。
         * @return `Executor()` 返回的执行器。
         */
        [[nodiscard]] auto GetExecutor() const -> ExecutorType
        {
            return Executor();
        }

        /**
         * @brief 异步读取部分数据。
         * @param Buffer 接收缓冲区。
         * @param Ec 接收本次操作的错误码。
         * @return 本次读取的字节数，可能少于缓冲区容量。
         */
        virtual auto AsyncReadSome(std::span<std::byte> Buffer, std::error_code& Ec)
            -> Net::awaitable<std::size_t> = 0;

        /**
         * @brief 异步写入部分数据。
         * @param Buffer 待发送数据。
         * @param Ec 接收本次操作的错误码。
         * @return 本次写入的字节数，可能少于缓冲区长度。
         */
        virtual auto AsyncWriteSome(std::span<const std::byte> Buffer, std::error_code& Ec)
            -> Net::awaitable<std::size_t> = 0;

        /**
         * @brief 使用完成回调异步读取。
         * @details 默认通过 `co_spawn` 桥接到 awaitable 接口。
         * @param Buffer 接收缓冲区。
         * @param Handler 操作完成时调用的处理器。
         * @note 缓冲区须保持有效，直到处理器被调用。
         */
        virtual void AsyncReadSome(std::span<std::byte> Buffer,
            Net::any_completion_handler<void(boost::system::error_code, std::size_t)> Handler)
        {
            ExecutorType Ex;
            std::shared_ptr<Transmission> Self;
            try
            {
                Ex = Executor();
                Self = shared_from_this();
            }
            catch (...)
            {
                Handler(boost::asio::error::operation_aborted, 0);
                return;
            }
            auto State = std::make_shared<CompletionState>(
                std::move(Handler));
            try
            {
                Net::co_spawn(Ex,
                    BridgeRead(std::move(Self), Buffer, State),
                    [](std::exception_ptr Failure)
                    {
                        if (Failure)
                        {
                            std::rethrow_exception(Failure);
                        }
                    });
            }
            catch (...)
            {
                State->Complete(Net::error::operation_aborted, 0);
            }
        }

        /**
         * @brief 使用完成回调异步写入。
         * @details 默认通过 `co_spawn` 桥接到 awaitable 接口。
         * @param Buffer 待发送数据。
         * @param Handler 操作完成时调用的处理器。
         * @note 缓冲区须保持有效，直到处理器被调用。
         */
        virtual void AsyncWriteSome(std::span<const std::byte> Buffer,
            Net::any_completion_handler<void(boost::system::error_code, std::size_t)> Handler)
        {
            ExecutorType Ex;
            std::shared_ptr<Transmission> Self;
            try
            {
                Ex = Executor();
                Self = shared_from_this();
            }
            catch (...)
            {
                Handler(boost::asio::error::operation_aborted, 0);
                return;
            }
            auto State = std::make_shared<CompletionState>(
                std::move(Handler));
            try
            {
                Net::co_spawn(Ex,
                    BridgeWrite(std::move(Self), Buffer, State),
                    [](std::exception_ptr Failure)
                    {
                        if (Failure)
                        {
                            std::rethrow_exception(Failure);
                        }
                    });
            }
            catch (...)
            {
                State->Complete(Net::error::operation_aborted, 0);
            }
        }

        /**
         * @brief 关闭传输层。
         */
        virtual void Close() = 0;

        /**
         * @brief 取消所有未完成的异步操作。
         */
        virtual void Cancel() = 0;

        /**
         * @brief 获取内层传输（装饰器链导航）
         * @return 内层传输指针；叶子节点返回空指针。
         */
        [[nodiscard]] virtual auto NextLayer() noexcept -> Transmission*
        {
            return nullptr;
        }

        /**
         * @brief 以只读方式获取内层传输。
         * @return 内层传输指针；叶子节点返回空指针。
         */
        [[nodiscard]] virtual auto NextLayer() const noexcept -> const Transmission*
        {
            return nullptr;
        }

        /**
         * @brief 沿装饰器链查找目标类型的最底层传输。
         * @tparam T 要查找的传输类型。
         * @return 匹配的传输指针；未匹配时返回空指针。
         */
        template <typename T>
        [[nodiscard]] auto LowestLayer() noexcept -> T*
        {
            auto* current = this;
            while (auto* n = current->NextLayer())
            {
                current = n;
            }
            return dynamic_cast<T*>(current);
        }

        /**
         * @brief 以只读方式查找最底层传输。
         * @tparam T 要查找的传输类型。
         * @return 匹配的传输指针；未匹配时返回空指针。
         */
        template <typename T>
        [[nodiscard]] auto LowestLayer() const noexcept -> const T*
        {
            const auto* current = this;
            while (const auto* n = current->NextLayer())
            {
                current = n;
            }
            return dynamic_cast<const T*>(current);
        }
    };

    /**
     * @brief 传输层共享指针类型。
     */
    using SharedTransmission = std::shared_ptr<Transmission>;

    // ── 组合操作自由函数 ──

    /**
     * @brief 循环写入，直到缓冲区写完或操作失败。
     * @param T 传输对象。
     * @param Buffer 待发送数据。
     * @param Ec 接收错误码。
     * @return 实际写入的字节数，失败时可能为部分长度。
     */
    inline auto AsyncWrite(Transmission& T, std::span<const std::byte> Buffer, std::error_code& Ec)
        -> Net::awaitable<std::size_t>
    {
        Ec.clear();
        std::size_t total = 0;
        while (total < Buffer.size())
        {
            const auto remaining = Buffer.subspan(total);
            const auto n = co_await T.AsyncWriteSome(remaining, Ec);
            if (n > remaining.size())
            {
                Ec = std::make_error_code(std::errc::message_size);
                co_return total;
            }
            total += n;
            if (Ec)
            {
                co_return total;
            }
            if (n == 0)
            {
                Ec = std::make_error_code(std::errc::io_error);
                co_return total;
            }
        }
        co_return total;
    }

    /**
     * @brief 循环读取，直到缓冲区填满或操作失败。
     * @param T 传输对象。
     * @param Buffer 接收缓冲区。
     * @param Ec 接收错误码。
     * @return 实际读取的字节数，失败时可能为部分长度。
     */
    inline auto AsyncRead(Transmission& T, std::span<std::byte> Buffer, std::error_code& Ec)
        -> Net::awaitable<std::size_t>
    {
        Ec.clear();
        std::size_t total = 0;
        while (total < Buffer.size())
        {
            const auto remaining = Buffer.subspan(total);
            const auto n = co_await T.AsyncReadSome(remaining, Ec);
            if (n > remaining.size())
            {
                Ec = std::make_error_code(std::errc::message_size);
                co_return total;
            }
            total += n;
            if (Ec)
            {
                co_return total;
            }
            if (n == 0)
            {
                Ec = std::make_error_code(std::errc::io_error);
                co_return total;
            }
        }
        co_return total;
    }
}
